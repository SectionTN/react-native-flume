package me.sectiontn.flume

import android.content.Context
import android.content.res.Configuration
import android.widget.FrameLayout
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.facebook.react.bridge.ReactContext
import com.facebook.react.uimanager.UIManagerHelper

class FlumeView(context: Context) : FrameLayout(context) {
  private val measurer = FlumeTextMeasurer(context)
  val recycler = RecyclerView(context).apply { layoutManager = LinearLayoutManager(context) }
  private val adapter = FlumeAdapter(context, measurer, recycler)

  var storeId = 0
    set(value) {
      field = value
      if (isAttachedToWindow && value != 0) adapter.attach(value)
    }

  // Change animations stay off so a rebind never flickers; false removes the animator entirely.
  private val animator = DefaultItemAnimator().apply { supportsChangeAnimations = false }
  var itemAnimations = true
    set(value) {
      field = value
      recycler.itemAnimator = if (value) animator else null
    }

  private val measureAndLayout = Runnable {
    measure(
      MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
      MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY),
    )
    layout(left, top, right, bottom)
  }

  init {
    recycler.adapter = adapter
    recycler.itemAnimator = animator
    adapter.onPress = { position, pressId, long -> emitPress(position, FlumeJni.keyAt(storeId, position), pressId, long) }
    addView(recycler, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
  }

  fun emitPress(index: Int, key: String, pressId: String?, long: Boolean) {
    val reactContext = context as? ReactContext ?: return
    val dispatcher = UIManagerHelper.getEventDispatcherForReactTag(reactContext, id) ?: return
    dispatcher.dispatchEvent(FlumePressEvent(UIManagerHelper.getSurfaceId(this), id, long, index, key, pressId))
  }

  override fun onAttachedToWindow() {
    super.onAttachedToWindow()
    if (storeId != 0) adapter.attach(storeId)
  }

  override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
    super.onSizeChanged(w, h, oldw, oldh)
    adapter.onViewportChanged(w, resources.configuration.fontScale)
  }

  override fun onConfigurationChanged(newConfig: Configuration) {
    super.onConfigurationChanged(newConfig)
    adapter.onViewportChanged(width, newConfig.fontScale)
  }

  override fun onDetachedFromWindow() {
    adapter.detach()
    super.onDetachedFromWindow()
  }

  // Fabric never runs a second Android layout pass, so a RecyclerView relayout has to be driven here.
  override fun requestLayout() {
    super.requestLayout()
    post(measureAndLayout)
  }
}
