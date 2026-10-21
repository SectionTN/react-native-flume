package me.sectiontn.flume

import android.content.Context
import android.content.res.Configuration
import android.os.SystemClock
import android.widget.FrameLayout
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.facebook.react.bridge.ReactContext
import com.facebook.react.uimanager.UIManagerHelper
import com.facebook.react.uimanager.events.Event
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
  private val sticky = StickyHeaderHost(this, FlumeCellView(context, measurer), adapter.allocateHolderId())
  private var stickySet: Set<Int> = emptySet()
  private var throttle = ScrollThrottle(16)
  private var scrollState = RecyclerView.SCROLL_STATE_IDLE
  var emitScrollEvents = false
  var onScrollState: ((Int) -> Unit)? = null
  var scrollEventThrottleMs = 16L
    set(value) {
      field = value
      throttle = ScrollThrottle(value)
    }
  val geometry get() = adapter.geometry
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
    addView(sticky.pinnedView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    adapter.onDataChanged = { sticky.refresh(force = true) }
    recycler.addOnScrollListener(object : RecyclerView.OnScrollListener() {
      override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
        sticky.refresh()
        if (emitScrollEvents && throttle.shouldEmit(SystemClock.uptimeMillis())) emitScroll(ScrollKind.SCROLL)
      }

      // Drag and momentum events are per gesture and always sent; a suppressed scroll event is flushed on idle.
      override fun onScrollStateChanged(recyclerView: RecyclerView, newState: Int) {
        for (kind in ScrollStateMachine.transition(scrollState, newState)) emitScroll(kind)
        scrollState = newState
        if (newState == RecyclerView.SCROLL_STATE_IDLE && throttle.flush() && emitScrollEvents) emitScroll(ScrollKind.SCROLL)
        onScrollState?.invoke(newState)
      }
    })
    hookLayoutManager()
  }

  // Layout completion drives the pinned header and, later, the scroll targets; a grid also needs the span lookup.
  private fun hookLayoutManager() {
    when (val lm = layoutManager) {
      is FlumeLinearLayoutManager -> lm.onLaidOut = { sticky.refresh() }
      is FlumeGridLayoutManager -> lm.onLaidOut = { sticky.refresh() }
    }
    applyFullSpan()
  }

  var stickyHeaderIndices = IntArray(0)
    set(value) {
      field = value
      val previous = stickySet
      stickySet = value.toHashSet()
      adapter.isFullSpan = { stickySet.contains(it) }
      applyFullSpan()
      sticky.indices = value
      adapter.onStickyChanged(previous, stickySet)
      sticky.refresh(force = true)
    }

  private fun applyFullSpan() {
    (layoutManager as? FlumeGridLayoutManager)?.fullSpanLookup?.isFullSpan = adapter.isFullSpan
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
    hookLayoutManager()
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

  fun emitPress(index: Int, key: String, pressId: String?, long: Boolean) =
    dispatch(FlumePressEvent(UIManagerHelper.getSurfaceId(this), id, long, index, key, pressId))

  private fun emitScroll(kind: ScrollKind) {
    val horizontal = geometry.horizontal
    val offsetPx = if (horizontal) recycler.computeHorizontalScrollOffset() else recycler.computeVerticalScrollOffset()
    val rangePx = if (horizontal) recycler.computeHorizontalScrollRange() else recycler.computeVerticalScrollRange()
    val extentPx = if (horizontal) recycler.computeHorizontalScrollExtent() else recycler.computeVerticalScrollExtent()
    val offsetDp = ScrollMetrics.fromContentStart(offsetPx, rangePx, extentPx, inverted) / density
    val contentWidthDp = (if (horizontal) rangePx else geometry.innerWidthPx) / density
    val contentHeightDp = (if (horizontal) geometry.innerHeightPx else rangePx) / density
    dispatch(FlumeScrollEvent(UIManagerHelper.getSurfaceId(this), id, kind, horizontal, offsetDp, contentWidthDp, contentHeightDp, recycler.width / density, recycler.height / density))
  }

  fun dispatch(event: Event<*>) {
    val reactContext = context as? ReactContext ?: return
    UIManagerHelper.getEventDispatcherForReactTag(reactContext, id)?.dispatchEvent(event)
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
