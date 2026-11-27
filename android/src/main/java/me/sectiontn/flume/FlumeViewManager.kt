package me.sectiontn.flume

import android.view.View
import com.facebook.react.bridge.ReadableArray
import com.facebook.react.bridge.ReadableMap
import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.PixelUtil
import com.facebook.react.uimanager.ReactStylesDiffMap
import com.facebook.react.uimanager.StateWrapper
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.ViewGroupManager
import com.facebook.react.uimanager.ViewManagerDelegate
import com.facebook.react.uimanager.annotations.ReactProp
import com.facebook.react.viewmanagers.FlumeViewManagerDelegate
import com.facebook.react.viewmanagers.FlumeViewManagerInterface

@ReactModule(name = FlumeViewManager.NAME)
class FlumeViewManager : ViewGroupManager<FlumeView>(), FlumeViewManagerInterface<FlumeView> {
  private val delegate = FlumeViewManagerDelegate(this)

  override fun getDelegate(): ViewManagerDelegate<FlumeView> = delegate

  override fun getName(): String = NAME

  override fun createViewInstance(context: ThemedReactContext): FlumeView = FlumeView(context)

  override fun onAfterUpdateTransaction(view: FlumeView) {
    super.onAfterUpdateTransaction(view)
    view.afterProps()
  }

  // Slot views live in holders and the bay; Fabric's child order is mirrored so its removeViewAt finds them.
  override fun needsCustomLayoutForChildren(): Boolean = true

  override fun updateState(view: FlumeView, props: ReactStylesDiffMap, stateWrapper: StateWrapper): Any? {
    view.setStateWrapper(stateWrapper)
    return null
  }

  override fun onDropViewInstance(view: FlumeView) {
    view.endScrollCommand()
    // A view dropped while detached registered its mount listener after its own detach cleanup.
    view.dropMountListener()
    view.setStateWrapper(null)
    super.onDropViewInstance(view)
  }

  override fun addView(parent: FlumeView, child: View, index: Int) = parent.slots.add(child, index)

  override fun getChildCount(parent: FlumeView): Int = parent.slots.count

  override fun getChildAt(parent: FlumeView, index: Int): View? = parent.slots.childAt(index)

  override fun removeViewAt(parent: FlumeView, index: Int) = parent.slots.removeAt(index)

  @ReactProp(name = "storeId")
  override fun setStoreId(view: FlumeView, value: Int) {
    view.storeId = value
  }

  override fun getExportedCustomDirectEventTypeConstants(): MutableMap<String, Any> = mutableMapOf(
    FlumePressEvent.PRESS to mapOf("registrationName" to "onCellPress"),
    FlumePressEvent.LONG_PRESS to mapOf("registrationName" to "onCellLongPress"),
    ScrollKind.SCROLL.eventName to mapOf("registrationName" to "onScroll"),
    ScrollKind.BEGIN_DRAG.eventName to mapOf("registrationName" to "onScrollBeginDrag"),
    ScrollKind.END_DRAG.eventName to mapOf("registrationName" to "onScrollEndDrag"),
    ScrollKind.MOMENTUM_BEGIN.eventName to mapOf("registrationName" to "onMomentumScrollBegin"),
    ScrollKind.MOMENTUM_END.eventName to mapOf("registrationName" to "onMomentumScrollEnd"),
    FlumeViewabilityEvent.NAME to mapOf("registrationName" to "onViewableItemsChanged"),
    FlumeEdgeEvent.END to mapOf("registrationName" to "onEndReached"),
    FlumeEdgeEvent.START to mapOf("registrationName" to "onStartReached"),
    FlumeRefreshEvent.NAME to mapOf("registrationName" to "onRefresh"),
    FlumeSlotsEvent.NAME to mapOf("registrationName" to "onSlotsChanged"),
    FlumeScrollCommandEndEvent.NAME to mapOf("registrationName" to "onScrollCommandEnd"),
  )

  @ReactProp(name = "itemAnimations", defaultBoolean = true)
  override fun setItemAnimations(view: FlumeView, value: Boolean) {
    view.itemAnimations = value
  }

  @ReactProp(name = "horizontal")
  override fun setHorizontal(view: FlumeView, value: Boolean) {
    view.horizontal = value
  }

  @ReactProp(name = "numColumns", defaultInt = 1)
  override fun setNumColumns(view: FlumeView, value: Int) {
    view.numColumns = value
  }

  @ReactProp(name = "inverted")
  override fun setInverted(view: FlumeView, value: Boolean) {
    view.inverted = value
  }

  @ReactProp(name = "contentPaddingTop")
  override fun setContentPaddingTop(view: FlumeView, value: Double) = view.setContentPadding(1, value)

  @ReactProp(name = "contentPaddingLeft")
  override fun setContentPaddingLeft(view: FlumeView, value: Double) = view.setContentPadding(0, value)

  @ReactProp(name = "contentPaddingBottom")
  override fun setContentPaddingBottom(view: FlumeView, value: Double) = view.setContentPadding(3, value)

  @ReactProp(name = "contentPaddingRight")
  override fun setContentPaddingRight(view: FlumeView, value: Double) = view.setContentPadding(2, value)

  @ReactProp(name = "paddingPercentTop")
  override fun setPaddingPercentTop(view: FlumeView, value: Double) = view.setPaddingPercent(1, value)

  @ReactProp(name = "paddingPercentLeft")
  override fun setPaddingPercentLeft(view: FlumeView, value: Double) = view.setPaddingPercent(0, value)

  @ReactProp(name = "paddingPercentBottom")
  override fun setPaddingPercentBottom(view: FlumeView, value: Double) = view.setPaddingPercent(3, value)

  @ReactProp(name = "paddingPercentRight")
  override fun setPaddingPercentRight(view: FlumeView, value: Double) = view.setPaddingPercent(2, value)

  @ReactProp(name = "contentJustify")
  override fun setContentJustify(view: FlumeView, value: String?) {
    view.contentJustify = ContentJustify.of(value)
  }

  @ReactProp(name = "growFooter")
  override fun setGrowFooter(view: FlumeView, value: Boolean) {
    view.growFooter = value
  }

  @ReactProp(name = "contentBackgroundColor", customType = "Color")
  override fun setContentBackgroundColor(view: FlumeView, value: Int?) {
    view.recycler.contentBackground = value ?: 0
  }

  @ReactProp(name = "mainGap")
  override fun setMainGap(view: FlumeView, value: Double) = view.setGap(0, value)

  @ReactProp(name = "gridRowGap")
  override fun setGridRowGap(view: FlumeView, value: Double) = view.setGap(1, value)

  @ReactProp(name = "crossGap")
  override fun setCrossGap(view: FlumeView, value: Double) = view.setGap(2, value)

  @ReactProp(name = "laneInsetStart")
  override fun setLaneInsetStart(view: FlumeView, value: Double) = view.setGap(3, value)

  @ReactProp(name = "laneInsetEnd")
  override fun setLaneInsetEnd(view: FlumeView, value: Double) = view.setGap(4, value)

  @ReactProp(name = "drawDistance", defaultDouble = -1.0)
  override fun setDrawDistance(view: FlumeView, value: Double) {
    view.drawDistanceDp = value
  }

  @ReactProp(name = "listHeader")
  override fun setListHeader(view: FlumeView, value: Boolean) {
    view.listHeader = value
  }

  @ReactProp(name = "listFooter")
  override fun setListFooter(view: FlumeView, value: Boolean) {
    view.listFooter = value
  }

  @ReactProp(name = "listEmpty")
  override fun setListEmpty(view: FlumeView, value: Boolean) {
    view.listEmpty = value
  }

  @ReactProp(name = "estimatedItemSize", defaultDouble = -1.0)
  override fun setEstimatedItemSize(view: FlumeView, value: Double) = view.setEstimatedItemSize(value)

  @ReactProp(name = "initialScrollIndex", defaultInt = -1)
  override fun setInitialScrollIndex(view: FlumeView, value: Int) {
    view.initialScrollIndex = value
  }

  @ReactProp(name = "separatorSize")
  override fun setSeparatorSize(view: FlumeView, value: Double) = view.setSeparator(sizeDp = value)

  @ReactProp(name = "separatorColor", customType = "Color")
  override fun setSeparatorColor(view: FlumeView, value: Int?) = view.setSeparator(color = value ?: 0)

  @ReactProp(name = "separatorInsetStart")
  override fun setSeparatorInsetStart(view: FlumeView, value: Double) = view.setSeparator(insetStartDp = value)

  @ReactProp(name = "separatorInsetEnd")
  override fun setSeparatorInsetEnd(view: FlumeView, value: Double) = view.setSeparator(insetEndDp = value)

  @ReactProp(name = "scrollEventThrottle", defaultDouble = 16.0)
  override fun setScrollEventThrottle(view: FlumeView, value: Double) {
    view.scrollEventThrottleMs = value.toLong()
  }

  @ReactProp(name = "emitScrollEvents")
  override fun setEmitScrollEvents(view: FlumeView, value: Boolean) {
    view.emitScrollEvents = value
  }

  @ReactProp(name = "emitViewabilityEvents")
  override fun setEmitViewabilityEvents(view: FlumeView, value: Boolean) {
    view.emitViewabilityEvents = value
  }

  @ReactProp(name = "viewabilityConfig")
  override fun setViewabilityConfig(view: FlumeView, value: ReadableMap?) {
    view.setViewabilityConfig(
      ViewabilityConfig(
        itemVisiblePercentThreshold = value.doubleOrNull("itemVisiblePercentThreshold"),
        viewAreaCoveragePercentThreshold = value.doubleOrNull("viewAreaCoveragePercentThreshold"),
        minimumViewTime = value.doubleOrNull("minimumViewTime")?.toLong() ?: 0,
        waitForInteraction = value != null && value.hasKey("waitForInteraction") && !value.isNull("waitForInteraction") && value.getBoolean("waitForInteraction"),
      ),
    )
  }

  private fun ReadableMap?.doubleOrNull(key: String): Double? =
    if (this != null && hasKey(key) && !isNull(key)) getDouble(key) else null

  private fun ReadableMap?.boolOr(key: String, default: Boolean): Boolean =
    if (this != null && hasKey(key) && !isNull(key)) getBoolean(key) else default

  @ReactProp(name = "maintainVisibleContentPosition")
  override fun setMaintainVisibleContentPosition(view: FlumeView, value: ReadableMap?) {
    view.anchorConfig = AnchorConfig(
      disabled = value.boolOr("disabled", false),
      autoscrollToTopThresholdPx = value.doubleOrNull("autoscrollToTopThreshold")?.let { PixelUtil.toPixelFromDIP(it).toDouble() },
      autoscrollToBottomThreshold = value.doubleOrNull("autoscrollToBottomThreshold"),
      animateAutoScrollToBottom = value.boolOr("animateAutoScrollToBottom", true),
      startRenderingFromBottom = value.boolOr("startRenderingFromBottom", false),
    )
  }

  @ReactProp(name = "endReachedThreshold", defaultDouble = 2.0)
  override fun setEndReachedThreshold(view: FlumeView, value: Double) {
    view.endReachedThresholdViewports = value
  }

  @ReactProp(name = "startReachedThreshold", defaultDouble = 2.0)
  override fun setStartReachedThreshold(view: FlumeView, value: Double) {
    view.startReachedThresholdViewports = value
  }

  @ReactProp(name = "refreshEnabled")
  override fun setRefreshEnabled(view: FlumeView, value: Boolean) {
    view.refreshEnabled = value
  }

  @ReactProp(name = "refreshing")
  override fun setRefreshing(view: FlumeView, value: Boolean) {
    view.refreshing = value
  }

  @ReactProp(name = "refreshColors", customType = "ColorArray")
  override fun setRefreshColors(view: FlumeView, value: ReadableArray?) {
    view.setRefreshColors(value?.let { array -> IntArray(array.size()) { array.getInt(it) } })
  }

  @ReactProp(name = "refreshProgressViewOffset")
  override fun setRefreshProgressViewOffset(view: FlumeView, value: Double) = view.setRefreshProgressViewOffset(value)

  @ReactProp(name = "scrollEnabled", defaultBoolean = true)
  override fun setScrollEnabled(view: FlumeView, value: Boolean) {
    view.recycler.scrollEnabled = value
  }

  @ReactProp(name = "nestedScrollEnabled")
  override fun setNestedScrollEnabled(view: FlumeView, value: Boolean) {
    view.recycler.isNestedScrollingEnabled = value
  }

  @ReactProp(name = "overScrollMode")
  override fun setOverScrollMode(view: FlumeView, value: String?) = view.setListOverScrollMode(value)

  @ReactProp(name = "decelerationRate", defaultDouble = 0.998)
  override fun setDecelerationRate(view: FlumeView, value: Double) {
    view.decelerationRate = value
  }

  @ReactProp(name = "showsVerticalScrollIndicator", defaultBoolean = true)
  override fun setShowsVerticalScrollIndicator(view: FlumeView, value: Boolean) {
    view.recycler.isVerticalScrollBarEnabled = value
  }

  @ReactProp(name = "showsHorizontalScrollIndicator", defaultBoolean = true)
  override fun setShowsHorizontalScrollIndicator(view: FlumeView, value: Boolean) {
    view.recycler.isHorizontalScrollBarEnabled = value
  }

  // iOS only.
  override fun setContentInset(view: FlumeView, value: ReadableMap?) = Unit

  override fun setScrollIndicatorInsets(view: FlumeView, value: ReadableMap?) = Unit

  override fun setContentInsetAdjustmentBehavior(view: FlumeView, value: String?) = Unit

  override fun setBounces(view: FlumeView, value: Boolean) = Unit

  override fun setAlwaysBounceVertical(view: FlumeView, value: Boolean) = Unit

  override fun setScrollsToTop(view: FlumeView, value: Boolean) = Unit

  override fun setAlwaysBounceHorizontal(view: FlumeView, value: Boolean) = Unit

  override fun setIndicatorStyle(view: FlumeView, value: String?) = Unit

  override fun setDirectionalLockEnabled(view: FlumeView, value: Boolean) = Unit

  override fun setCanCancelContentTouches(view: FlumeView, value: Boolean) = Unit

  override fun setScrollToOverflowEnabled(view: FlumeView, value: Boolean) = Unit

  override fun setAutomaticallyAdjustsScrollIndicatorInsets(view: FlumeView, value: Boolean) = Unit

  override fun setPersistentScrollbar(view: FlumeView, value: Boolean) {
    view.recycler.isScrollbarFadingEnabled = !value
  }

  override fun setRefreshTintColor(view: FlumeView, value: Int?) = Unit

  @ReactProp(name = "keyboardDismissMode")
  override fun setKeyboardDismissMode(view: FlumeView, value: String?) {
    view.keyboardDismissOnDrag = value == "on-drag"
  }

  @ReactProp(name = "keyboardShouldPersistTaps")
  override fun setKeyboardShouldPersistTaps(view: FlumeView, value: String?) = view.setKeyboardShouldPersistTaps(value)

  @ReactProp(name = "rowsPressable")
  override fun setRowsPressable(view: FlumeView, value: Boolean) {
    view.rowsPressable = value
  }

  override fun scrollToIndex(
    view: FlumeView,
    requestId: Int,
    index: Int,
    animated: Boolean,
    viewPosition: Double,
    viewOffset: Double,
  ) = view.scrollToIndex(requestId, index, animated, viewPosition.toFloat(), viewOffset)

  override fun scrollToOffset(view: FlumeView, requestId: Int, offset: Double, animated: Boolean) =
    view.scrollToOffset(requestId, offset, animated)

  override fun scrollToEnd(view: FlumeView, requestId: Int, animated: Boolean) = view.scrollToEnd(requestId, animated)

  override fun scrollIndexIntoView(view: FlumeView, requestId: Int, index: Int, animated: Boolean, viewOffset: Double) =
    view.scrollIndexIntoView(requestId, index, animated, viewOffset)

  override fun scrollTo(view: FlumeView, x: Double, y: Double, animated: Boolean) = view.scrollToPoint(x, y, animated)

  override fun flashScrollIndicators(view: FlumeView) = view.flashScrollIndicators()

  override fun setNativeRefreshing(view: FlumeView, refreshing: Boolean) = view.setNativeRefreshing(refreshing)

  companion object {
    const val NAME = "FlumeView"
  }
}
