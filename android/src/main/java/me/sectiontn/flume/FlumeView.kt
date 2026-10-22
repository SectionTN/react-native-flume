package me.sectiontn.flume

import android.content.Context
import android.view.MotionEvent
import android.view.ViewConfiguration
import android.content.res.Configuration
import android.os.SystemClock
import android.widget.FrameLayout
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import com.facebook.react.bridge.ReactContext
import com.facebook.react.uimanager.UIManagerHelper
import com.facebook.react.uimanager.events.Event
import kotlin.math.roundToInt

class FlumeView(context: Context) : FrameLayout(context) {
  private companion object {
    const val SMOOTH_SCROLL_BUDGET_MS = 400
  }

  private val measurer = FlumeTextMeasurer(context)
  private val density = context.resources.displayMetrics.density
  val recycler = FlumeRecyclerView(context).apply { isNestedScrollingEnabled = false }
  private val refreshLayout = SwipeRefreshLayout(context).apply { isEnabled = false }
  private var didLayout = false
  private var refreshProgressViewOffsetDp = 0.0
  val slots = FlumeSlots(this)
  private val adapter = FlumeAdapter(context, measurer, recycler, slots)
  private var layoutManager: LinearLayoutManager = FlumeLinearLayoutManager(context, RecyclerView.VERTICAL, false)
  private val paddingPx = IntArray(4)
  private var initialScrollPending = false
  private var separator: SeparatorDecoration? = null
  private val sticky = StickyHeaderHost(this, FlumeCellView(context, measurer), adapter.allocateHolderId())
  private var stickySet: Set<Int> = emptySet()
  private val measureHolderId = adapter.allocateHolderId()
  private var throttle = ScrollThrottle(16)
  private var scrollState = RecyclerView.SCROLL_STATE_IDLE
  var emitScrollEvents = false
  private val viewability = ViewabilityTracker(ViewabilityConfig())
  var emitViewabilityEvents = false
  private val edges = EdgeTracker()
  var endReachedThresholdViewports = 2.0
  var startReachedThresholdViewports = 2.0
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

  var keyboardDismissOnDrag = false

  var rowsPressable = false
    set(value) {
      field = value
      adapter.rowsPressable = value
      sticky.pinnedView.rowsPressable = value
      for (i in 0 until recycler.childCount) (recycler.getChildAt(i) as? FlumeCellView)?.rowsPressable = value
    }

  var anchorConfig = AnchorConfig()
    set(value) {
      val rebuild = value.startRenderingFromBottom != field.startRenderingFromBottom
      field = value
      adapter.anchorConfig = value
      if (rebuild) rebuildLayoutManager()
    }

  var decelerationRate = 0.998
    set(value) {
      field = value
      recycler.flingFactor = FlingFactor.of(value)
    }

  var refreshEnabled = false
    set(value) {
      field = value
      refreshLayout.isEnabled = value
    }

  // The circle needs a measured layout, as React Native waits for one too.
  var refreshing = false
    set(value) {
      field = value
      if (didLayout) refreshLayout.isRefreshing = value
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
    adapter.onPress = { position, pressId, long ->
      val index = rows.dataIndex(position)
      if (index >= 0) emitPress(index, FlumeJni.keyAt(storeId, index), pressId, long)
    }
    adapter.onFirstData = { applyInitialScroll() }
    adapter.onAutoscroll = { position, viewPosition, animated -> scrollToPosition(position, animated, viewPosition, 0.0) }
    refreshLayout.addView(recycler, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
    refreshLayout.setOnRefreshListener { dispatch(FlumeRefreshEvent(UIManagerHelper.getSurfaceId(this), id)) }
    addView(slots.parking, LayoutParams(0, 0))
    addView(refreshLayout, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
    addView(sticky.pinnedView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    adapter.onDataChanged = { reset ->
      viewability.invalidate()
      if (reset) recycler.stopScroll()
      applySpans()
    }
    recycler.addOnScrollListener(object : RecyclerView.OnScrollListener() {
      override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
        sticky.refresh()
        if (emitScrollEvents && throttle.shouldEmit(SystemClock.uptimeMillis())) emitScroll(ScrollKind.SCROLL)
        evaluateViewability()
        evaluateEdges()
      }

      // Drag and momentum events are per gesture and always sent; a suppressed scroll event is flushed on idle.
      override fun onScrollStateChanged(recyclerView: RecyclerView, newState: Int) {
        for (kind in ScrollStateMachine.transition(scrollState, newState)) emitScroll(kind)
        scrollState = newState
        if (newState == RecyclerView.SCROLL_STATE_DRAGGING) {
          viewability.interacted = true
          evaluateViewability()
          if (keyboardDismissOnDrag) recycler.dismissKeyboard()
        }
        if (newState == RecyclerView.SCROLL_STATE_IDLE && throttle.flush() && emitScrollEvents) emitScroll(ScrollKind.SCROLL)
        onScrollState?.invoke(newState)
      }
    })
    hookLayoutManager()
  }

  // Layout completion drives the pinned header and, later, the scroll targets; a grid also needs the span lookup.
  private fun hookLayoutManager() {
    when (val lm = layoutManager) {
      is FlumeLinearLayoutManager -> lm.onLaidOut = { onLaidOut() }
      is FlumeGridLayoutManager -> lm.onLaidOut = { onLaidOut() }
    }
    applyFullSpan()
  }

  val rows: ListRows get() = adapter.rows

  // Data indices; the pinned copy and the grid work in adapter positions.
  var stickyHeaderIndices = IntArray(0)
    set(value) {
      field = value
      applySpans()
    }

  var listHeader = false
    set(value) {
      field = value
      applyListComponents()
    }
  var listFooter = false
    set(value) {
      field = value
      applyListComponents()
    }
  var listEmpty = false
    set(value) {
      field = value
      applyListComponents()
    }

  private fun applyListComponents() {
    adapter.setListComponents(listHeader, listFooter, listEmpty)
    applySpans()
  }

  // Sticky rows and the list components take the full span; their positions move with the data and the components.
  private fun applySpans() {
    val positions = stickyHeaderIndices.filter { it in 0 until rows.dataCount }.map(rows::position).sorted().toIntArray()
    val previous = stickySet
    stickySet = positions.toHashSet()
    adapter.fullSpans = FullSpans(rows.fullSpanPositions(stickyHeaderIndices))
    applyFullSpan()
    sticky.indices = positions
    adapter.onStickyChanged(previous, stickySet)
    sticky.refresh(force = true)
  }

  private fun onLaidOut() {
    sticky.refresh()
    updateEmptyLength()
    evaluateViewability()
    evaluateEdges()
  }

  // Offset, range and extent along the scroll axis, as RecyclerView estimates them from the laid-out rows.
  private fun scrollMetrics(): ScrollRange = if (geometry.horizontal) {
    ScrollRange(recycler.computeHorizontalScrollOffset(), recycler.computeHorizontalScrollRange(), recycler.computeHorizontalScrollExtent())
  } else {
    ScrollRange(recycler.computeVerticalScrollOffset(), recycler.computeVerticalScrollRange(), recycler.computeVerticalScrollExtent())
  }

  private fun evaluateEdges() {
    if (storeId == 0 || !geometry.ready) return
    val visible = if (geometry.horizontal) geometry.innerWidthPx else geometry.innerHeightPx
    val hits = edges.update(scrollMetrics(), rows.dataCount, inverted, visible, startReachedThresholdViewports, endReachedThresholdViewports)
    if (hits.end) dispatch(FlumeEdgeEvent(UIManagerHelper.getSurfaceId(this), id, true, hits.distances.fromEndPx / density))
    if (hits.start) dispatch(FlumeEdgeEvent(UIManagerHelper.getSurfaceId(this), id, false, hits.distances.fromStartPx / density))
  }

  // The laid-out rows along the scroll axis, in px from the padded leading edge, translation included.
  fun childFrames(): List<ChildFrame> {
    val lm = recycler.layoutManager as? LinearLayoutManager ?: return emptyList()
    val horizontal = lm.orientation == RecyclerView.HORIZONTAL
    return (0 until lm.childCount).mapNotNull { i ->
      val child = lm.getChildAt(i) ?: return@mapNotNull null
      val position = lm.getPosition(child)
      if (position == RecyclerView.NO_POSITION) return@mapNotNull null
      if (horizontal) {
        val tx = child.translationX.toInt()
        ChildFrame(position, lm.getDecoratedLeft(child) - recycler.paddingLeft + tx, lm.getDecoratedRight(child) - recycler.paddingLeft + tx)
      } else {
        val ty = child.translationY.toInt()
        ChildFrame(position, lm.getDecoratedTop(child) - recycler.paddingTop + ty, lm.getDecoratedBottom(child) - recycler.paddingTop + ty)
      }
    }
  }

  // index counts data items, as FlatList's does; the header and footer are reached by offset and scrollToEnd.
  fun scrollToIndex(index: Int, animated: Boolean, viewPosition: Float, viewOffsetDp: Double) {
    val target = ScrollTargets.clampIndex(index, rows.dataCount) ?: return
    scrollToPosition(rows.position(target), animated, viewPosition, viewOffsetDp)
  }

  private fun scrollToPosition(position: Int, animated: Boolean, viewPosition: Float, viewOffsetDp: Double) {
    val target = ScrollTargets.clampIndex(position, rows.itemCount) ?: return
    viewability.interacted = true
    recycler.stopScroll()
    val viewOffsetPx = (viewOffsetDp * density).roundToInt()
    if (animated) {
      val frames = childFrames()
      val first = layoutManager.findFirstVisibleItemPosition()
      val rowPx = if (frames.isEmpty()) 0 else frames.sumOf { it.end - it.start } / frames.size
      val spanCount = (layoutManager as? GridLayoutManager)?.spanCount ?: 1
      val rowsCrossed = ScrollTargets.rowSpan(first, target) { adapter.fullSpans.groupIndex(it, spanCount) }
      val distancePx = if (first == RecyclerView.NO_POSITION) 0 else rowsCrossed * rowPx
      val speed = ScrollTargets.speedMsPerPx(25f / resources.displayMetrics.densityDpi, distancePx, SMOOTH_SCROLL_BUDGET_MS)
      val scroller = FlumeSmoothScroller(context, viewPosition, viewOffsetPx, inverted, speed)
      scroller.targetPosition = target
      layoutManager.startSmoothScroll(scroller)
      return
    }
    val viewportPx = if (geometry.horizontal) geometry.innerWidthPx else geometry.innerHeightPx
    val itemPx = if (ScrollTargets.needsLength(viewPosition)) rowLengthPx(target) else 0
    layoutManager.scrollToPositionWithOffset(target, ScrollTargets.offsetPx(viewPosition, viewOffsetPx, viewportPx, itemPx))
  }

  // A worklet row binds into a scratch holder so it lands exactly; a React row has its measured length, else the estimate.
  private fun rowLengthPx(position: Int): Int {
    if (storeId == 0 || !geometry.ready) return 0
    val viewType = adapter.getItemViewType(position)
    if (ViewTypes.isReact(viewType)) {
      val holder = (recycler.findViewHolderForAdapterPosition(position) as? FlumeAdapter.Holder)?.slot
      val view = holder?.view
      if (holder != null && view != null && holder.ready) return view.lengthPx(geometry.horizontal)
      return slots.estimator.estimatePx(SlotGroup(viewType))
    }
    val index = rows.dataIndex(position).takeIf { it >= 0 } ?: return 0
    val batch = FlumeJni.bind(storeId, measureHolderId, index, geometry.cellWidthDp(adapter.fullSpans.contains(position)), geometry.cellHeightDp()) ?: return 0
    return ((if (geometry.horizontal) batch.rootWidth else batch.rootHeight) * density).roundToInt()
  }

  // The empty row fills what the header and footer leave of the viewport, so flex content centers in the visible area.
  private fun updateEmptyLength() {
    if (!rows.hasEmptyRow || !geometry.ready) return
    val holder = (recycler.findViewHolderForAdapterPosition(rows.headerCount) as? FlumeAdapter.Holder)?.slot ?: return
    val viewport = if (geometry.horizontal) geometry.innerWidthPx else geometry.innerHeightPx
    var usedPx = 0
    if (rows.header) usedPx += rowLengthPx(0)
    if (rows.footer) usedPx += rowLengthPx(rows.itemCount - 1)
    slots.setMinMain(holder, maxOf(0, viewport - usedPx) / density)
  }

  // Offsets count from the content start; the visual delta flips for an inverted list.
  fun scrollToOffset(offsetDp: Double, animated: Boolean) {
    viewability.interacted = true
    recycler.stopScroll()
    val targetPx = (offsetDp * density).roundToInt()
    ScrollTargets.startRow(targetPx)?.let { row ->
      scrollToPosition(row, animated, 0f, 0.0)
      return
    }
    val (offsetPx, rangePx, extentPx) = scrollMetrics()
    val delta = targetPx - ScrollMetrics.fromContentStart(offsetPx, rangePx, extentPx, inverted)
    val visual = if (inverted) -delta else delta
    val dx = if (geometry.horizontal) visual else 0
    val dy = if (geometry.horizontal) 0 else visual
    if (animated) recycler.smoothScrollBy(dx, dy) else recycler.scrollBy(dx, dy)
  }

  fun scrollToEnd(animated: Boolean) = scrollToPosition(rows.itemCount - 1, animated, 1f, 0.0)

  fun setListOverScrollMode(value: String?) {
    recycler.overScrollMode = OverScroll.mode(value)
  }

  fun setKeyboardShouldPersistTaps(value: String?) {
    recycler.keyboardShouldPersistTaps = KeyboardTapPolicy.mode(value)
  }

  fun setRefreshColors(colors: IntArray?) {
    if (colors == null || colors.isEmpty()) refreshLayout.setColorSchemeColors() else refreshLayout.setColorSchemeColors(*colors)
  }

  fun setRefreshProgressViewOffset(offsetDp: Double) {
    refreshProgressViewOffsetDp = offsetDp
    if (didLayout) applyRefreshOffset()
  }

  private fun applyRefreshOffset() {
    val (start, end) = RefreshOffsets.of(refreshProgressViewOffsetDp, density, refreshLayout.progressCircleDiameter)
    refreshLayout.setProgressViewOffset(false, start, end)
  }

  override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
    super.onLayout(changed, left, top, right, bottom)
    if (didLayout) return
    didLayout = true
    if (refreshProgressViewOffsetDp != 0.0) applyRefreshOffset()
    if (refreshing) refreshLayout.isRefreshing = true
  }

  fun setViewabilityConfig(config: ViewabilityConfig) {
    viewability.config = config
  }

  // A changed set is reported after minimumViewTime with the rows still in view then, the way FlatList does.
  private fun evaluateViewability() {
    if (!emitViewabilityEvents || storeId == 0) return
    val viewport = if (geometry.horizontal) geometry.innerWidthPx else geometry.innerHeightPx
    val frames = childFrames().mapNotNull { frame -> rows.dataIndex(frame.position).takeIf { it >= 0 }?.let { frame.copy(position = it) } }
    val tokens = viewability.keyed(viewability.compute(viewport, frames)) { FlumeJni.keyAt(storeId, it) } ?: return
    val delay = viewability.config.minimumViewTime
    if (delay > 0) postDelayed({ emitViewability(viewability.reportDelayed(tokens)) }, delay) else emitViewability(viewability.report(tokens))
  }

  private fun emitViewability(report: ViewabilityReport?) {
    if (report == null) return
    dispatch(FlumeViewabilityEvent(UIManagerHelper.getSurfaceId(this), id, report.viewable, report.changed))
  }

  private fun applyFullSpan() {
    (layoutManager as? FlumeGridLayoutManager)?.fullSpanLookup?.fullSpans = adapter.fullSpans
  }

  fun setSeparator(sizeDp: Double? = null, color: Int? = null, insetStartDp: Double? = null, insetEndDp: Double? = null) {
    if (sizeDp != null) separatorSizeDp = sizeDp
    if (color != null) separatorColor = color
    if (insetStartDp != null) separatorInsetStartDp = insetStartDp
    if (insetEndDp != null) separatorInsetEndDp = insetEndDp
    separator?.let { recycler.removeItemDecoration(it) }
    val sizePx = (separatorSizeDp * density).roundToInt()
    separator = if (sizePx > 0) {
      SeparatorDecoration(sizePx, separatorColor, (separatorInsetStartDp * density).roundToInt(), (separatorInsetEndDp * density).roundToInt()) { rows }
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
      FlumeLinearLayoutManager(context, orientation, inverted).apply {
        stackFromEnd = LayoutFlags.stackFromEnd(inverted, anchorConfig.startRenderingFromBottom)
      }
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

  // Applied once, when the first data lands; an index at or past the data scrolls to the end, as FlatList does.
  private fun applyInitialScroll() {
    if (!initialScrollPending) return
    initialScrollPending = false
    if (initialScrollIndex < rows.dataCount) {
      layoutManager.scrollToPositionWithOffset(ScrollTargets.initialPosition(initialScrollIndex, rows), 0)
    } else {
      scrollToPosition(ScrollTargets.initialPosition(initialScrollIndex, rows), false, 1f, 0.0)
    }
  }

  fun emitPress(index: Int, key: String, pressId: String?, long: Boolean) =
    dispatch(FlumePressEvent(UIManagerHelper.getSurfaceId(this), id, long, index, key, pressId))

  private fun emitScroll(kind: ScrollKind) {
    val horizontal = geometry.horizontal
    val (offsetPx, rangePx, extentPx) = scrollMetrics()
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

  private val touchSlop = ViewConfiguration.get(context).scaledTouchSlop
  private var downOnPinned = false
  private var downX = 0f
  private var downY = 0f
  private var forwarding = false

  // A drag that starts on the pinned header scrolls the list, as it does on a ScrollView's sticky header; a tap stays with the header.
  override fun onInterceptTouchEvent(e: MotionEvent): Boolean {
    when (e.actionMasked) {
      MotionEvent.ACTION_DOWN -> {
        downOnPinned = sticky.contains(e.x, e.y)
        downX = e.x
        downY = e.y
      }
      MotionEvent.ACTION_MOVE -> if (PinnedDrag.takesOver(downOnPinned, e.x - downX, e.y - downY, touchSlop)) {
        downOnPinned = false
        forwarding = true
        val down = MotionEvent.obtain(e.downTime, e.eventTime, MotionEvent.ACTION_DOWN, downX, downY, e.metaState)
        refreshLayout.dispatchTouchEvent(down)
        down.recycle()
        return true
      }
    }
    return false
  }

  override fun onTouchEvent(e: MotionEvent): Boolean {
    if (!forwarding) return super.onTouchEvent(e)
    if (e.actionMasked == MotionEvent.ACTION_UP || e.actionMasked == MotionEvent.ACTION_CANCEL) forwarding = false
    return refreshLayout.dispatchTouchEvent(e)
  }

  override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
    super.onSizeChanged(w, h, oldw, oldh)
    if (adapter.onViewportChanged(w, h, resources.configuration.fontScale)) sticky.refresh(force = true)
  }

  override fun onConfigurationChanged(newConfig: Configuration) {
    super.onConfigurationChanged(newConfig)
    if (adapter.onViewportChanged(width, height, newConfig.fontScale)) sticky.refresh(force = true)
  }

  override fun onDetachedFromWindow() {
    adapter.detach()
    super.onDetachedFromWindow()
  }

  // Fabric never runs a second Android layout pass, so a RecyclerView relayout is driven here: one pending pass
  // however many slots resize in a mount batch. No flag: the super constructor calls this before fields exist.
  override fun requestLayout() {
    super.requestLayout()
    removeCallbacks(measureAndLayout)
    post(measureAndLayout)
  }

  fun setEstimatedItemSize(dp: Double) {
    slots.estimator.estimatedPx = if (dp < 0) -1 else (dp * density).roundToInt()
  }
}
