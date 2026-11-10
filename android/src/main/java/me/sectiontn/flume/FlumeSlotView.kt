package me.sectiontn.flume

import android.content.Context
import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.ReactStylesDiffMap
import com.facebook.react.uimanager.StateWrapper
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.ViewGroupManager
import com.facebook.react.uimanager.ViewManagerDelegate
import com.facebook.react.uimanager.annotations.ReactProp
import com.facebook.react.viewmanagers.FlumeSlotViewManagerDelegate
import com.facebook.react.viewmanagers.FlumeSlotViewManagerInterface
import com.facebook.react.views.view.ReactViewGroup

// One React cell: Fabric measures it at its Yoga size, the holder that shows it places it.
class FlumeSlotView(context: Context) : ReactViewGroup(context) {
  var slot = 0
  var itemKey = ""
  var kind = 0
  var measured = false
    private set
  var onMeasured: ((FlumeSlotView) -> Unit)? = null
  var stateWrapper: StateWrapper? = null
  var writtenX = Long.MIN_VALUE
  var writtenY = Long.MIN_VALUE
  private var lastWidth = -1
  private var lastHeight = -1

  fun lengthPx(horizontal: Boolean): Int = if (horizontal) measuredWidth else measuredHeight

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    super.onMeasure(widthMeasureSpec, heightMeasureSpec)
    measured = true
    if (measuredWidth == lastWidth && measuredHeight == lastHeight) return
    lastWidth = measuredWidth
    lastHeight = measuredHeight
    onMeasured?.invoke(this)
  }
}

@ReactModule(name = FlumeSlotViewManager.NAME)
class FlumeSlotViewManager : ViewGroupManager<FlumeSlotView>(), FlumeSlotViewManagerInterface<FlumeSlotView> {
  private val delegate = FlumeSlotViewManagerDelegate(this)

  override fun getDelegate(): ViewManagerDelegate<FlumeSlotView> = delegate

  override fun getName(): String = NAME

  override fun createViewInstance(context: ThemedReactContext): FlumeSlotView = FlumeSlotView(context)

  // Mounting destroys the previous wrapper after each update, so only the latest one can write.
  override fun updateState(view: FlumeSlotView, props: ReactStylesDiffMap, stateWrapper: StateWrapper): Any? {
    view.stateWrapper = stateWrapper
    return null
  }

  override fun onDropViewInstance(view: FlumeSlotView) {
    view.stateWrapper = null
    super.onDropViewInstance(view)
  }

  @ReactProp(name = "slot")
  override fun setSlot(view: FlumeSlotView, value: Int) {
    view.slot = value
  }

  @ReactProp(name = "itemKey")
  override fun setItemKey(view: FlumeSlotView, value: String?) {
    view.itemKey = value ?: ""
  }

  @ReactProp(name = "kind")
  override fun setKind(view: FlumeSlotView, value: Int) {
    view.kind = value
  }

  companion object {
    const val NAME = "FlumeSlotView"
  }
}
