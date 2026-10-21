package me.sectiontn.flume

import android.content.Context
import android.content.res.Configuration
import android.widget.FrameLayout
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.facebook.react.bridge.ReactContext
import com.facebook.react.uimanager.UIManagerHelper
import kotlin.math.roundToInt

class FlumeView(context: Context) : FrameLayout(context) {
  private val measurer = FlumeTextMeasurer(context)
  private val density = context.resources.displayMetrics.density
  val recycler = RecyclerView(context)
  private val adapter = FlumeAdapter(context, measurer, recycler)
  private var layoutManager: LinearLayoutManager = FlumeLinearLayoutManager(context, RecyclerView.VERTICAL, false)
  private val paddingPx = IntArray(4)
  private var initialScrollPending = false
  private var separator: SeparatorDecoration? = null
  private var separatorSizeDp = 0.0
  private var separatorColor = 0
  private var separatorInsetStartDp = 0.0
  private var separatorInsetEndDp = 0.0

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

  var horizontal = false
    set(value) {
      field = value
      rebuildLayoutManager()
    }

  var numColumns = 1
    set(value) {
      field = value
      rebuildLayoutManager()
    }

  var inverted = false
    set(value) {
      field = value
      rebuildLayoutManager()
    }

  // -1 is one viewport on each side.
  var drawDistanceDp = -1.0
    set(value) {
      field = value
      applyDrawDistance()
    }

  var initialScrollIndex = -1
    set(value) {
      field = value
      initialScrollPending = value >= 0
    }

  private val measureAndLayout = Runnable {
    measure(
      MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
      MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY),
    )
    layout(left, top, right, bottom)
  }

  init {
    recycler.layoutManager = layoutManager
    recycler.adapter = adapter
    recycler.itemAnimator = animator
    recycler.clipToPadding = false
    adapter.onPress = { position, pressId, long -> emitPress(position, FlumeJni.keyAt(storeId, position), pressId, long) }
    adapter.onFirstData = { applyInitialScroll() }
    addView(recycler, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
  }

  fun setSeparator(sizeDp: Double? = null, color: Int? = null, insetStartDp: Double? = null, insetEndDp: Double? = null) {
    if (sizeDp != null) separatorSizeDp = sizeDp
    if (color != null) separatorColor = color
    if (insetStartDp != null) separatorInsetStartDp = insetStartDp
    if (insetEndDp != null) separatorInsetEndDp = insetEndDp
    separator?.let { recycler.removeItemDecoration(it) }
    val sizePx = (separatorSizeDp * density).roundToInt()
    separator = if (sizePx > 0) {
      SeparatorDecoration(sizePx, separatorColor, (separatorInsetStartDp * density).roundToInt(), (separatorInsetEndDp * density).roundToInt())
    } else {
      null
    }
    separator?.let { recycler.addItemDecoration(it) }
    recycler.invalidateItemDecorations()
  }

  // Edges are 0 left, 1 top, 2 right, 3 bottom, in dp.
  fun setContentPadding(edge: Int, dp: Double) {
    paddingPx[edge] = (dp * density).roundToInt()
    recycler.setPadding(paddingPx[0], paddingPx[1], paddingPx[2], paddingPx[3])
    adapter.configure(horizontal, numColumns, inverted, paddingPx)
  }

  // A grid only reverses; stackFromEnd is unsupported there, so an inverted grid with little content starts at the top.
  private fun rebuildLayoutManager() {
    val orientation = if (horizontal) RecyclerView.HORIZONTAL else RecyclerView.VERTICAL
    layoutManager = if (numColumns > 1) {
      FlumeGridLayoutManager(context, numColumns, orientation, inverted)
    } else {
      FlumeLinearLayoutManager(context, orientation, inverted).apply { stackFromEnd = inverted }
    }
    applyDrawDistance()
    recycler.layoutManager = layoutManager
    adapter.configure(horizontal, numColumns, inverted, paddingPx)
  }

  private fun applyDrawDistance() {
    val px = if (drawDistanceDp < 0) -1 else (drawDistanceDp * density).roundToInt()
    when (val lm = layoutManager) {
      is FlumeLinearLayoutManager -> lm.drawDistancePx = px
      is FlumeGridLayoutManager -> lm.drawDistancePx = px
    }
  }

  // Applied once, when the first data lands with enough rows; a later index change starts over.
  private fun applyInitialScroll() {
    if (!initialScrollPending) return
    initialScrollPending = false
    if (initialScrollIndex < adapter.itemCount) layoutManager.scrollToPositionWithOffset(initialScrollIndex, 0)
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
    adapter.onViewportChanged(w, h, resources.configuration.fontScale)
  }

  override fun onConfigurationChanged(newConfig: Configuration) {
    super.onConfigurationChanged(newConfig)
    adapter.onViewportChanged(width, height, newConfig.fontScale)
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
