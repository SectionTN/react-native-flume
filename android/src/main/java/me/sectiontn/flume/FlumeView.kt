package me.sectiontn.flume

import android.content.Context
import android.graphics.Canvas
import android.view.MotionEvent
import android.view.View
import android.view.VelocityTracker
import android.view.ViewConfiguration
import android.content.res.Configuration
import android.os.SystemClock
import android.widget.FrameLayout
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import com.facebook.react.bridge.ReactContext
import com.facebook.react.bridge.UIManager
import com.facebook.react.bridge.UIManagerListener
import com.facebook.react.common.annotations.UnstableReactNativeAPI
import com.facebook.react.uimanager.StateWrapper
import com.facebook.react.uimanager.UIManagerHelper
import com.facebook.react.uimanager.events.Event
import com.facebook.react.uimanager.events.NativeGestureUtil
import com.facebook.react.uimanager.common.UIManagerType
import kotlin.math.roundToInt

class FlumeView(context: Context) : FrameLayout(context) {
  private companion object {
    const val SMOOTH_SCROLL_BUDGET_MS = 400
    // Rows measuring in later frames can push the target again; past this many corrections it stays where it is.
    const val MAX_REALIGNS = 6
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
  // Null until the first build replaces the unhooked layout manager above.
  private var shape: LayoutShape? = null
  private val paddingPx = IntArray(4)
  private val paddingPercent = FloatArray(4)
  private var initialScrollPending = false
  private var separator: SeparatorDecoration? = null
  private val gapsPx = IntArray(5)
  private var gaps: GapDecoration? = null
  private val sticky = StickyHeaderHost(
    this,
    FlumeCellView(context, measurer),
    adapter.allocateHolderId(),
    FlumeSlotHolder(context, slots.parking).apply { owner = adapter.allocateHolderId() },
  )
  private var stickySet: Set<Int> = emptySet()
  private val measureHolderId = adapter.allocateHolderId()
  private var throttle = ScrollThrottle(16)
  private val velocity = ScrollVelocity()
  private var finger: VelocityTracker? = null
  private var scrollState = RecyclerView.SCROLL_STATE_IDLE
  private var pendingAlign: PendingAlign? = null
  private val origins = ShadowOrigins()
  private val writeOrigins = Runnable { if (pressCanStart) writeOrigins() }
  // A touch stops a fling and cancels the JS touch, but a list the user cannot scroll keeps gliding under a press.
  private val pressCanStart: Boolean
    get() = scrollState == RecyclerView.SCROLL_STATE_IDLE || !recycler.scrollEnabled

  // A jump onto React rows waits for the rows it shows; its retained slots are lent to their binds when it runs.
  private data class Jump(val target: JumpTarget, val animated: Boolean, val viewPosition: Float, val viewOffsetDp: Double, val owner: Int)

  private val jumps = JumpGate<Jump>()
  private val jumpDue = Runnable { jumps.due(SystemClock.uptimeMillis())?.let { runJump(it) } }
  private val commands = ScrollCommands()
  // The jump of the command in flight once it has run, which new data sends again at its target.
  private var landing: Jump? = null
  // An instant scroll lands with the next layout pass, an offset glide at its first idle state after it moved.
  private var landOnLayout = false
  private var landOnIdle = false
  private var movedSinceScroll = false
  // Bumped by every scroll the view starts, so a stopped smooth scroller of an earlier one lands nothing.
  private var scrollToken = 0
  private var reclaimOnLayout = false
  private var reclaimOnIdle = false
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

  // A re-attach applies pending data without reporting it, so the same id arriving again is ignored.
  var storeId = 0
    set(value) {
      if (value == field) return
      field = value
      if (isAttachedToWindow && value != 0) adapter.attach(value)
    }

  // Change animations stay off so a rebind never flickers; false removes the animator entirely.
  private val animator = FlumeItemAnimator().apply { supportsChangeAnimations = false }
  var itemAnimations = true
    set(value) {
      if (value == field) return
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
      field = value
      adapter.anchorConfig = value
      rebuildLayoutManager()
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

  // A pull starts the circle without a prop change; the wrapper sends the controlled value back.
  fun setNativeRefreshing(value: Boolean) {
    refreshLayout.isRefreshing = value
  }

  // The same value arriving again must not re-arm the scroll for the next empty-to-filled change.
  var initialScrollIndex = -1
    set(value) {
      if (value == field) return
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
    slots.onRendered = { slot -> jumps.rendered(slot)?.let { runJump(it) } }
    adapter.onRebindAll = { animator.still = true }
    recycler.layoutManager = layoutManager
    recycler.adapter = adapter
    recycler.itemAnimator = animator
    recycler.clipToPadding = false
    // A row composed with its separator draws its shadow from a child view, which a clipping cell would cut.
    recycler.clipChildren = false
    adapter.onPress = { position, pressId, long ->
      val index = rows.dataIndex(position)
      if (index >= 0) emitPress(index, FlumeJni.keyAt(storeId, index), pressId, long)
    }
    adapter.onFirstData = { applyInitialScroll() }
    adapter.onAutoscroll = { position, viewPosition, animated ->
      endScrollCommand()
      requestJump(JumpTarget.Position(position), animated, viewPosition, 0.0)
    }
    refreshLayout.addView(recycler, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
    refreshLayout.setOnRefreshListener { dispatch(FlumeRefreshEvent(UIManagerHelper.getSurfaceId(this), id)) }
    addView(slots.parking, LayoutParams(0, 0))
    addView(refreshLayout, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
    addView(sticky.pinnedView, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    addView(sticky.reactPinned, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
    adapter.onDataChanged = { reset ->
      viewability.invalidate()
      // A command still on its way lands on its target in the new rows, not where anchoring keeps the old ones.
      val again = landing?.takeIf { commands.pending != 0 && jumps.pending == null }
      if (again != null) {
        scrollToken++
        pendingAlign = null
      }
      if (reset) {
        recycler.stopScroll()
        pendingAlign = null
      }
      applySpans()
      if (again != null) {
        commands.begin(commands.pending)
        requestJump(again.target, again.animated, again.viewPosition, again.viewOffsetDp)
      } else {
        retargetJump()
      }
    }
    adapter.onStickyIndicesChanged = { applySpans() }
    recycler.addOnScrollListener(object : RecyclerView.OnScrollListener() {
      override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
        origins.scrolled(dx, dy)
        if (pressCanStart) requestOrigins()
        sticky.refresh()
        if (emitScrollEvents) {
          // ScrollView samples its velocity on every change, before the throttle.
          val now = SystemClock.uptimeMillis()
          val metrics = scrollMetrics()
          velocity.scrolled(ScrollMetrics.fromContentStart(metrics.offsetPx, metrics.rangePx, metrics.extentPx, inverted), now)
          if (throttle.shouldEmit(now)) emitScroll(ScrollKind.SCROLL, metrics = metrics)
        }
        evaluateViewability()
        evaluateEdges()
      }

      // Drag and momentum events are per gesture and always sent; a suppressed scroll event is flushed on idle.
      override fun onScrollStateChanged(recyclerView: RecyclerView, newState: Int) {
        for (kind in ScrollStateMachine.transition(scrollState, newState)) {
          emitScroll(kind, afterDrag = scrollState == RecyclerView.SCROLL_STATE_DRAGGING)
        }
        scrollState = newState
        if (newState == RecyclerView.SCROLL_STATE_SETTLING) movedSinceScroll = true
        if (newState == RecyclerView.SCROLL_STATE_DRAGGING) {
          pendingAlign = null
          jumps.cancel()?.let { dropJump(it) }
          endScrollCommand()
          viewability.interacted = true
          evaluateViewability()
          if (keyboardDismissOnDrag) recycler.dismissKeyboard()
        }
        if (newState == RecyclerView.SCROLL_STATE_IDLE && throttle.flush() && emitScrollEvents) emitScroll(ScrollKind.SCROLL)
        if (newState == RecyclerView.SCROLL_STATE_IDLE) {
          realign()
          // Inline, so the state is queued before a touch the next looper turn delivers.
          writeOrigins()
          if (landOnIdle && movedSinceScroll) {
            landOnIdle = false
            commands.land()
          }
          settleCommand()
        }
        if (newState == RecyclerView.SCROLL_STATE_IDLE && reclaimOnIdle) {
          reclaimOnIdle = false
          slots.reclaim()
        }
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
    val positions = adapter.stickyIndices.filter { it in 0 until rows.dataCount }.map(rows::position).sorted().toIntArray()
    val previous = stickySet
    stickySet = positions.toHashSet()
    adapter.fullSpans = FullSpans(rows.fullSpanPositions(adapter.stickyIndices))
    applyFullSpan()
    sticky.indices = positions
    adapter.onStickyChanged(previous, stickySet)
    sticky.refresh(force = true)
  }

  private fun onLaidOut() {
    animator.still = false
    if (reclaimOnLayout) {
      reclaimOnLayout = false
      slots.reclaim()
    }
    if (pendingAlign != null) {
      removeCallbacks(realignAfterLayout)
      post(realignAfterLayout)
    }
    if (landOnLayout) {
      landOnLayout = false
      commands.land()
    }
    settleCommand()
    sticky.refresh()
    updateEmptyLength()
    updateFooterLength()
    evaluateViewability()
    evaluateEdges()
    requestOrigins()
  }

  // Every state commit brings a new wrapper, and only the first one needs a pass.
  fun setStateWrapper(wrapper: StateWrapper?) {
    val first = origins.viewState == null
    origins.viewState = wrapper
    if (first && wrapper != null) requestOrigins()
  }

  fun requestOrigins() {
    removeCallbacks(writeOrigins)
    post(writeOrigins)
  }

  // Positions use the laid-out top, not the translation an item animation starts from.
  private fun writeOrigins() {
    if (slots.count == 0) return
    origins.writeView()
    val left = refreshLayout.left + recycler.left
    val top = refreshLayout.top + recycler.top
    for (i in 0 until recycler.childCount) {
      val holder = recycler.getChildAt(i) as? FlumeSlotHolder ?: continue
      // A disappearing holder can still point at a view another holder took.
      val view = holder.view?.takeIf { it.measured && it.parent === holder } ?: continue
      origins.writeSlot(view, left + holder.left, top + holder.top)
    }
    val pinned = sticky.reactPinned
    val view = pinned.view?.takeIf { it.measured && pinned.visibility == VISIBLE } ?: return
    origins.writeSlot(view, pinned.left + pinned.translationX.roundToInt(), pinned.top + pinned.translationY.roundToInt())
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

  fun viewTypeAt(position: Int): Int = adapter.getItemViewType(position)

  fun isReactRow(position: Int): Boolean = ViewTypes.isReact(viewTypeAt(position))

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
  fun scrollToIndex(id: Int, index: Int, animated: Boolean, viewPosition: Float, viewOffsetDp: Double) {
    commands.begin(id)
    requestJump(JumpTarget.Index(index), animated, viewPosition, viewOffsetDp)
  }

  // A row already fully in view ends the command where the list is. An index past the data lands at the end edge.
  fun scrollIndexIntoView(id: Int, index: Int, animated: Boolean, viewOffsetDp: Double) {
    commands.begin(id)
    val target = JumpTarget.Index(index)
    val position = target.position(rows)
    val first = layoutManager.findFirstCompletelyVisibleItemPosition()
    val last = layoutManager.findLastCompletelyVisibleItemPosition()
    val viewPosition = when {
      position == null || target.waitsForData(rows) -> 1f
      else -> IntoView.target(position, minOf(first, last), maxOf(first, last))
    }
    if (viewPosition == null) {
      jumps.cancel()?.let { dropJump(it) }
      forgetLanding()
      landCommand()
      return
    }
    requestJump(target, animated, viewPosition, viewOffsetDp)
  }

  // React rows paint a frame or more after their bind, so a jump first has the rows it will show rendered.
  private fun requestJump(target: JumpTarget, animated: Boolean, viewPosition: Float, viewOffsetDp: Double) {
    jumps.cancel()?.let { dropJump(it) }
    forgetLanding()
    if (target.position(rows) == null && !target.waitsForData(rows)) {
      landCommand()
      return
    }
    val jump = Jump(target, animated, viewPosition, viewOffsetDp, adapter.allocateHolderId())
    val waiting = reserveJump(jump)
    if (waiting.isEmpty() && !target.waitsForData(rows)) {
      runJump(jump)
      return
    }
    jumps.request(jump, waiting, SystemClock.uptimeMillis())
    removeCallbacks(jumpDue)
    postDelayed(jumpDue, ScrollTargets.JUMP_WAIT_MS)
  }

  // The slots of the React rows the jump shows that have not rendered yet.
  private fun reserveJump(jump: Jump): Set<Int> {
    val position = jump.target.position(rows) ?: return emptySet()
    val window = jumpWindow(position, jump.animated, jump.viewPosition)
    val requests = window.filter { isReactRow(it) }.map { adapter.slotRequest(it, viewTypeAt(it)) }
    return slots.reserve(jump.owner, requests)
  }

  // New data moves a waiting jump's target, so it waits for the new rows it shows, before the same deadline.
  private fun retargetJump() {
    val waiting = jumps.pending ?: return
    slots.dropReserved(waiting.owner)
    val jump = waiting.copy(owner = adapter.allocateHolderId())
    val slotsLeft = reserveJump(jump)
    jumps.retarget(jump, slotsLeft)
    if (slotsLeft.isEmpty() && !jump.target.waitsForData(rows)) jumps.cancel()?.let { runJump(it) }
  }

  private fun runJump(jump: Jump) {
    removeCallbacks(jumpDue)
    slots.lend(jump.owner)
    val position = jump.target.position(rows) ?: return landCommand()
    if (jump.animated) reclaimOnIdle = true else reclaimOnLayout = true
    landing = jump
    startScroll(position, jump.animated, jump.viewPosition, jump.viewOffsetDp)
  }

  // A new scroll replaces the landing of the one before it.
  private fun forgetLanding() {
    landing = null
    landOnLayout = false
    landOnIdle = false
    scrollToken++
  }

  private fun landCommand() {
    commands.land()
    settleCommand()
  }

  private fun settleCommand() {
    val ended = commands.settle(
      ScrollCommands.Rest(
        moving = scrollState == RecyclerView.SCROLL_STATE_SETTLING,
        dragging = scrollState == RecyclerView.SCROLL_STATE_DRAGGING,
        jumpWaiting = jumps.pending != null,
        realignPending = pendingAlign != null,
      ),
    )
    if (ended != 0) {
      landing = null
      dispatch(FlumeScrollCommandEndEvent(UIManagerHelper.getSurfaceId(this), id, ended))
    }
  }

  // A drag, leaving the window, anchoring's own scroll or a dropped view ends the command in flight.
  fun endScrollCommand() {
    landing = null
    landOnLayout = false
    landOnIdle = false
    val ended = commands.end()
    if (ended != 0) dispatch(FlumeScrollCommandEndEvent(UIManagerHelper.getSurfaceId(this), id, ended))
  }

  private fun dropJump(jump: Jump) {
    removeCallbacks(jumpDue)
    slots.dropReserved(jump.owner)
  }

  // An instant jump shows one screen around its target; an animated one also its landing screen and the rows it glides over.
  private fun jumpWindow(target: Int, animated: Boolean, viewPosition: Float): IntRange {
    val viewportPx = if (geometry.horizontal) geometry.innerWidthPx else geometry.innerHeightPx
    val window = ScrollTargets.jumpWindow(target, viewPosition, viewportPx, rows.itemCount, ::windowLengthPx)
    if (!animated) return window
    val first = layoutManager.findFirstVisibleItemPosition()
    if (first == RecyclerView.NO_POSITION) return window
    val last = layoutManager.findLastVisibleItemPosition()
    val near = ScrollTargets.preJump(first, target, last - first + 1, rows.itemCount)
    val landing = if (near == null) first..last else ScrollTargets.jumpWindow(near, 0f, viewportPx, rows.itemCount, ::windowLengthPx)
    return ScrollTargets.glideWindow(landing, window)
  }

  // A laid-out row's length, else its group's estimate; 0 for a grid position that does not start its row.
  private fun windowLengthPx(position: Int): Int {
    val spanCount = (layoutManager as? GridLayoutManager)?.spanCount ?: 1
    val fullSpans = adapter.fullSpans
    if (spanCount > 1 && position > 0 && fullSpans.groupIndex(position, spanCount) == fullSpans.groupIndex(position - 1, spanCount)) {
      return 0
    }
    layoutManager.findViewByPosition(position)?.let { return if (geometry.horizontal) it.width else it.height }
    return slots.estimator.estimatePx(SlotGroup(viewTypeAt(position)))
  }

  private fun startScroll(position: Int, animated: Boolean, viewPosition: Float, viewOffsetDp: Double) {
    val target = ScrollTargets.clampIndex(position, rows.itemCount) ?: return landCommand()
    val token = scrollToken
    viewability.interacted = true
    recycler.stopScroll()
    val viewOffsetPx = (viewOffsetDp * density).roundToInt()
    val anchoredAtEnd = layoutManager.stackFromEnd
    val realigns = ScrollTargets.realigns(isReactRow(target), viewPosition, anchoredAtEnd)
    pendingAlign = null
    if (animated) {
      val frames = childFrames()
      val visible = layoutManager.findFirstVisibleItemPosition()
      val perScreen = if (visible == RecyclerView.NO_POSITION) 0 else layoutManager.findLastVisibleItemPosition() - visible + 1
      val near = ScrollTargets.preJump(visible, target, perScreen, rows.itemCount)
      if (near != null) layoutManager.scrollToPositionWithOffset(near, 0)
      val first = near ?: visible
      val rowPx = if (frames.isEmpty()) 0 else frames.sumOf { it.end - it.start } / frames.size
      val spanCount = (layoutManager as? GridLayoutManager)?.spanCount ?: 1
      val rowsCrossed = ScrollTargets.rowSpan(first, target) { adapter.fullSpans.groupIndex(it, spanCount) }
      val distancePx = if (first == RecyclerView.NO_POSITION) 0 else rowsCrossed * rowPx
      val speed = ScrollTargets.speedMsPerPx(25f / resources.displayMetrics.densityDpi, distancePx, SMOOTH_SCROLL_BUDGET_MS)
      val scroller = FlumeSmoothScroller(context, viewPosition, viewOffsetPx, inverted, speed)
      scroller.targetPosition = target
      if (realigns) {
        scroller.onTarget = { view ->
          pendingAlign = PendingAlign(
            target,
            viewPosition,
            viewOffsetPx,
            if (geometry.horizontal) view.width else view.height,
            anchoredAtEnd = anchoredAtEnd,
          )
        }
      }
      scroller.onStopped = { if (token == scrollToken) landCommand() }
      layoutManager.startSmoothScroll(scroller)
      return
    }
    val viewportPx = if (geometry.horizontal) geometry.innerWidthPx else geometry.innerHeightPx
    val itemPx = if (ScrollTargets.needsLength(viewPosition)) rowLengthPx(target) else 0
    if (realigns) pendingAlign = PendingAlign(target, viewPosition, viewOffsetPx, itemPx, anchoredAtEnd = anchoredAtEnd)
    layoutManager.scrollToPositionWithOffset(target, ScrollTargets.offsetPx(viewPosition, viewOffsetPx, viewportPx, itemPx))
    landOnLayout = true
  }

  // RecyclerView drops a layout request made inside its own layout pass, so a correction found there waits for the pass to end.
  private val realignAfterLayout = Runnable { realign() }

  // While the list rests, a React target is put back where its alignment says until it and the rows in view have measured.
  private fun realign() {
    val pending = pendingAlign ?: return
    if (scrollState != RecyclerView.SCROLL_STATE_IDLE) return
    val item = recycler.findViewHolderForAdapterPosition(pending.position) as? FlumeAdapter.Holder
    val target = item?.slot?.let { slot -> slot.view?.takeIf { slot.ready } }
    val horizontal = geometry.horizontal
    val viewportPx = if (horizontal) geometry.innerWidthPx else geometry.innerHeightPx
    val startPx = item?.let { alignedStartPx(it.itemView) }
    val offset = AlignCorrection.offsetPx(pending, target != null, target?.lengthPx(horizontal) ?: 0, startPx, viewportPx)
    if (offset != null && pending.corrections < MAX_REALIGNS) {
      pendingAlign = pending.copy(corrections = pending.corrections + 1)
      layoutManager.scrollToPositionWithOffset(pending.position, offset)
    } else if (offset != null || (target != null && reactRowsReady())) {
      pendingAlign = null
    }
    settleCommand()
  }

  // The start as scrollToPositionWithOffset counts it: from the padded start edge, or from the end under a reversed layout.
  private fun alignedStartPx(child: View): Int {
    val lm = layoutManager
    return if (geometry.horizontal) {
      if (lm.reverseLayout) recycler.width - recycler.paddingRight - lm.getDecoratedRight(child) else lm.getDecoratedLeft(child) - recycler.paddingLeft
    } else {
      if (lm.reverseLayout) recycler.height - recycler.paddingBottom - lm.getDecoratedBottom(child) else lm.getDecoratedTop(child) - recycler.paddingTop
    }
  }

  private fun reactRowsReady(): Boolean = (0 until recycler.childCount).all { i ->
    val child = recycler.getChildAt(i) ?: return@all true
    (recycler.getChildViewHolder(child) as? FlumeAdapter.Holder)?.slot?.ready != false
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


  // contentContainerStyle flexGrow with justifyContent places short content and leaves the opening edge alone.
  var contentJustify = ContentJustify.Start
    set(value) {
      if (field == value) return
      field = value
      applyDrawDistance()
      recycler.requestLayout()
    }

  var growFooter = false
    set(value) {
      if (field == value) return
      field = value
      updateFooterLength()
    }

  // A growing footer takes what the laid-out rows above it leave of the viewport, an upright list only.
  private fun updateFooterLength() {
    if (!rows.footer || !geometry.ready) return
    val footer = recycler.findViewHolderForAdapterPosition(rows.itemCount - 1) as? FlumeAdapter.Holder ?: return
    val slot = footer.slot ?: return
    val px = when {
      !footerGrows() -> 0
      rows.hasEmptyRow -> emptyListGrowSharePx()
      else -> {
        val first = recycler.findViewHolderForAdapterPosition(0)?.itemView
        val lm = layoutManager
        val horizontal = geometry.horizontal
        if (first == null) {
          0
        } else {
          val start = if (horizontal) lm.getDecoratedLeft(first) else lm.getDecoratedTop(first)
          val footerStart = if (horizontal) lm.getDecoratedLeft(footer.itemView) else lm.getDecoratedTop(footer.itemView)
          ContentPlacement.footerMinPx(start, footerStart, if (horizontal) geometry.innerWidthPx else geometry.innerHeightPx)
        }
      }
    }
    slots.setMinMain(slot, px / density)
  }

  private fun footerGrows() = growFooter && rows.footer && !inverted

  // The empty row and a growing footer reserve the same rows and gaps, so together they fill the viewport.
  private fun emptyListGrowSharePx(): Int =
    ContentPlacement.emptyListGrowSharePx(
      viewportPx = if (geometry.horizontal) geometry.innerWidthPx else geometry.innerHeightPx,
      headerPx = if (rows.header) rowLengthPx(0) else 0,
      footerPx = if (rows.footer) rowLengthPx(rows.itemCount - 1) else 0,
      footerGrows = footerGrows(),
      rowCount = rows.itemCount,
      mainGapPx = gapsPx[0],
    )

  // The empty row fills what the header and footer leave of the viewport, so flex content centers in the visible area.
  private fun updateEmptyLength() {
    if (!rows.hasEmptyRow || !geometry.ready) return
    val holder = (recycler.findViewHolderForAdapterPosition(rows.headerCount) as? FlumeAdapter.Holder)?.slot ?: return
    slots.setMinMain(holder, emptyListGrowSharePx() / density)
  }

  // Offsets count from the content start; the visual delta flips for an inverted list.
  fun scrollToOffset(id: Int, offsetDp: Double, animated: Boolean) {
    commands.begin(id)
    viewability.interacted = true
    // Before stopScroll, whose idle state would run an older command's realign.
    pendingAlign = null
    jumps.cancel()?.let { dropJump(it) }
    forgetLanding()
    recycler.stopScroll()
    val targetPx = (offsetDp * density).roundToInt()
    ScrollTargets.startRow(targetPx)?.let { row ->
      requestJump(JumpTarget.Position(row), animated, 0f, 0.0)
      return
    }
    val (offsetPx, rangePx, extentPx) = scrollMetrics()
    val delta = targetPx - ScrollMetrics.fromContentStart(offsetPx, rangePx, extentPx, inverted)
    val visual = if (inverted) -delta else delta
    val dx = if (geometry.horizontal) visual else 0
    val dy = if (geometry.horizontal) 0 else visual
    if (visual != 0 && animated) {
      movedSinceScroll = false
      landOnIdle = true
      recycler.smoothScrollBy(dx, dy)
      return
    }
    if (visual != 0) recycler.scrollBy(dx, dy)
    landCommand()
  }

  fun scrollToEnd(id: Int, animated: Boolean) {
    commands.begin(id)
    requestJump(JumpTarget.End, animated, 1f, 0.0)
  }

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
    applyPadding()
  }

  // Edges as above, a fraction of the list width as Yoga resolves a percent on every edge.
  fun setPaddingPercent(edge: Int, fraction: Double) {
    if (paddingPercent[edge] == fraction.toFloat()) return
    paddingPercent[edge] = fraction.toFloat()
    applyPadding()
  }

  private fun resolvedPaddingPx() = ListGeometry.paddingPx(paddingPx, paddingPercent, width)

  private fun applyPadding() {
    val resolved = resolvedPaddingPx()
    recycler.setPadding(resolved[0], resolved[1], resolved[2], resolved[3])
    adapter.configure(horizontal, numColumns, inverted, resolved, lanes())
    sticky.refresh(force = true)
  }

  // Kinds are 0 main, 1 grid row, 2 column, 3 lane inset start and 4 lane inset end, in dp.
  fun setGap(kind: Int, dp: Double) {
    val px = (dp.coerceAtLeast(0.0) * density).roundToInt()
    if (gapsPx[kind] == px) return
    gapsPx[kind] = px
    if (kind >= 2) adapter.configure(horizontal, numColumns, inverted, resolvedPaddingPx(), lanes())
    val on = gapsPx.any { it > 0 }
    gaps?.takeIf { !on }?.let { recycler.removeItemDecoration(it) }
    gaps = if (on) gaps ?: GapDecoration(gapsPx) { rows }.also { recycler.addItemDecoration(it) } else null
    recycler.invalidateItemDecorations()
    sticky.refresh(force = true)
  }

  private fun lanes() = Lanes(insetStartPx = gapsPx[3], insetEndPx = gapsPx[4], gapPx = gapsPx[2])

  // A grid only reverses; stackFromEnd is unsupported there, so an inverted grid with little content starts at the top.
  private fun rebuildLayoutManager() {
    val next = LayoutShape.of(horizontal, numColumns, inverted, anchorConfig.startRenderingFromBottom)
    if (next == shape) return
    val kept = if (LayoutShape.keepsPosition(shape, next)) adapter.anchorState() else null
    shape = next
    val orientation = if (horizontal) RecyclerView.HORIZONTAL else RecyclerView.VERTICAL
    layoutManager = if (numColumns > 1) {
      FlumeGridLayoutManager(context, numColumns, orientation, inverted)
    } else {
      FlumeLinearLayoutManager(context, orientation, inverted).apply { stackFromEnd = next.stackFromEnd }
    }
    applyDrawDistance()
    hookLayoutManager()
    recycler.layoutManager = layoutManager
    kept?.let { layoutManager.scrollToPositionWithOffset(it.firstVisiblePosition, it.firstVisibleOffsetPx) }
    adapter.configure(horizontal, numColumns, inverted, resolvedPaddingPx(), lanes())
    sticky.refresh(force = true)
    layoutWithProps = true
  }

  // A new layout manager has no rows until it lays out, so it does in the frame of the props that made it.
  private var layoutWithProps = false

  fun afterProps() {
    if (!layoutWithProps) return
    layoutWithProps = false
    if (width == 0 || height == 0) return
    removeCallbacks(measureAndLayout)
    measureAndLayout.run()
  }

  private fun applyDrawDistance() {
    val px = if (drawDistanceDp < 0) -1 else (drawDistanceDp * density).roundToInt()
    when (val lm = layoutManager) {
      is FlumeLinearLayoutManager -> {
        lm.drawDistancePx = px
        lm.justify = contentJustify
      }
      is FlumeGridLayoutManager -> {
        lm.drawDistancePx = px
        lm.justify = contentJustify
      }
    }
  }

  // Applied once, when the first data lands; an index at or past the data scrolls to the end, as FlatList does.
  private fun applyInitialScroll() {
    if (!initialScrollPending) return
    initialScrollPending = false
    if (initialScrollIndex < rows.dataCount) {
      layoutManager.scrollToPositionWithOffset(ScrollTargets.initialPosition(initialScrollIndex, rows), 0)
    } else {
      requestJump(JumpTarget.Position(ScrollTargets.initialPosition(initialScrollIndex, rows)), false, 1f, 0.0)
    }
  }

  fun emitPress(index: Int, key: String, pressId: String?, long: Boolean) =
    dispatch(FlumePressEvent(UIManagerHelper.getSurfaceId(this), id, long, index, key, pressId))

  private fun emitScroll(kind: ScrollKind, afterDrag: Boolean = false, metrics: ScrollRange = scrollMetrics()) {
    val horizontal = geometry.horizontal
    val (offsetPx, rangePx, extentPx) = metrics
    val offsetDp = ScrollMetrics.fromContentStart(offsetPx, rangePx, extentPx, inverted) / density
    val (contentWidthPx, contentHeightPx) = ScrollMetrics.contentSizePx(horizontal, rangePx, geometry.paddingPx, recycler.width, recycler.height)
    val (velocityX, velocityY) = velocity.of(kind, horizontal, afterDrag)
    dispatch(FlumeScrollEvent(UIManagerHelper.getSurfaceId(this), id, kind, horizontal, offsetDp, contentWidthPx / density, contentHeightPx / density, recycler.width / density, recycler.height / density, velocityX / density, velocityY / density))
  }

  // ScrollView's drag end carries the finger's velocity at release, read before RecyclerView ends the drag inside super.
  override fun dispatchTouchEvent(e: MotionEvent): Boolean {
    val tracker = finger ?: VelocityTracker.obtain().also { finger = it }
    if (e.actionMasked == MotionEvent.ACTION_DOWN) {
      velocity.touchStarted()
      tracker.clear()
    }
    tracker.addMovement(e)
    if (e.actionMasked == MotionEvent.ACTION_UP || e.actionMasked == MotionEvent.ACTION_CANCEL) {
      tracker.computeCurrentVelocity(1)
      velocity.released(tracker.xVelocity, tracker.yVelocity, geometry.horizontal, inverted)
      tracker.recycle()
      finger = null
    }
    return super.dispatchTouchEvent(e)
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
  private val pinnedTap = KeyboardGesture()
  private var swallowingPinned = false

  // A drag that starts on the pinned header scrolls the list, as it does on a ScrollView's sticky header; a tap stays with the
  // header, through the list's keyboard policy: under never it only closes the keyboard, and React hears it was taken.
  override fun onInterceptTouchEvent(e: MotionEvent): Boolean {
    when (e.actionMasked) {
      MotionEvent.ACTION_DOWN -> {
        downOnPinned = sticky.contains(e.x, e.y)
        downX = e.x
        downY = e.y
        pinnedTap.down(if (downOnPinned) recycler.keyboardDecision(sticky.pressableAt(e.x, e.y), sticky.textInputAt(e.x, e.y)) else TapDecision(dismiss = false, swallow = false))
        if (pinnedTap.swallowing) {
          swallowingPinned = true
          NativeGestureUtil.notifyNativeGestureStarted(this, e)
          return true
        }
      }
      MotionEvent.ACTION_MOVE -> if (PinnedDrag.takesOver(downOnPinned, e.x - downX, e.y - downY, touchSlop)) {
        forwardToList(e)
        return true
      }
      MotionEvent.ACTION_UP -> if (downOnPinned && pinnedTap.release()) recycler.dismissKeyboard()
      MotionEvent.ACTION_CANCEL -> pinnedTap.release()
    }
    return false
  }

  override fun onTouchEvent(e: MotionEvent): Boolean {
    if (swallowingPinned) {
      when (e.actionMasked) {
        MotionEvent.ACTION_MOVE -> if (PinnedDrag.takesOver(true, e.x - downX, e.y - downY, touchSlop)) forwardToList(e)
        MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
          swallowingPinned = false
          NativeGestureUtil.notifyNativeGestureEnded(this, e)
          if (pinnedTap.release() && e.actionMasked == MotionEvent.ACTION_UP) recycler.dismissKeyboard()
        }
      }
      return true
    }
    if (!forwarding) return super.onTouchEvent(e)
    if (e.actionMasked == MotionEvent.ACTION_UP || e.actionMasked == MotionEvent.ACTION_CANCEL) forwarding = false
    return refreshLayout.dispatchTouchEvent(e)
  }

  // The list gets the gesture from its start; it reports the drag to React itself, and a drag closes nothing.
  private fun forwardToList(e: MotionEvent) {
    downOnPinned = false
    swallowingPinned = false
    pinnedTap.scrolled()
    forwarding = true
    val down = MotionEvent.obtain(e.downTime, e.eventTime, MotionEvent.ACTION_DOWN, downX, downY, e.metaState)
    refreshLayout.dispatchTouchEvent(down)
    down.recycle()
  }

  // A list clips to its bounds like a ScrollView, so a pinned copy pushed past the edge leaves the screen with it.
  override fun dispatchDraw(canvas: Canvas) {
    val saved = canvas.save()
    canvas.clipRect(0, 0, width, height)
    super.dispatchDraw(canvas)
    canvas.restoreToCount(saved)
  }

  override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
    super.onSizeChanged(w, h, oldw, oldh)
    if (w != oldw && paddingPercent.any { it != 0f }) applyPadding()
    if (adapter.onViewportChanged(w, h, resources.configuration.fontScale)) sticky.refresh(force = true)
  }

  override fun onConfigurationChanged(newConfig: Configuration) {
    super.onConfigurationChanged(newConfig)
    if (adapter.onViewportChanged(width, height, newConfig.fontScale)) sticky.refresh(force = true)
  }

  override fun onDetachedFromWindow() {
    dropMountListener()
    adapter.detach()
    // As on iOS, leaving the window ends a waiting jump and the command in flight.
    jumps.cancel()?.let { dropJump(it) }
    endScrollCommand()
    super.onDetachedFromWindow()
  }

  // Fabric never runs a second Android layout pass, so a RecyclerView relayout is driven here: one pending pass
  // however many slots resize in a mount batch. No flag: the super constructor calls this before fields exist.
  override fun requestLayout() {
    super.requestLayout()
    removeCallbacks(measureAndLayout)
    post(measureAndLayout)
  }

  private val dataRefresh = Runnable { if (storeId != 0) FlumeJni.refresh(storeId) }
  @OptIn(UnstableReactNativeAPI::class) private var mountListener: UIManagerListener? = null

  // Data JS deferred for its pre-rendered cells applies right after the mount batch that registers them, in the frame
  // that shows the rows it re-rendered with them. The post only covers a batch Fabric never reports.
  @OptIn(UnstableReactNativeAPI::class)
  fun refreshData() {
    removeCallbacks(dataRefresh)
    post(dataRefresh)
    if (mountListener != null) return
    val uiManager = UIManagerHelper.getUIManager(context as ReactContext, UIManagerType.FABRIC) ?: return
    val listener = object : UIManagerListener {
      override fun willDispatchViewUpdates(uiManager: UIManager) = Unit
      override fun willMountItems(uiManager: UIManager) = Unit
      override fun didDispatchMountItems(uiManager: UIManager) = Unit
      override fun didScheduleMountItems(uiManager: UIManager) = Unit
      // A batch that lands before the deferred data keeps the listener and the posted fallback.
      override fun didMountItems(uiManager: UIManager) {
        if (storeId == 0 || !FlumeJni.applyNow(storeId)) return
        dropMountListener()
        removeCallbacks(dataRefresh)
      }
    }
    mountListener = listener
    uiManager.addUIManagerEventListener(listener)
  }

  @OptIn(UnstableReactNativeAPI::class)
  fun dropMountListener() {
    val listener = mountListener ?: return
    mountListener = null
    UIManagerHelper.getUIManager(context as ReactContext, UIManagerType.FABRIC)?.removeUIManagerEventListener(listener)
  }

  fun setEstimatedItemSize(dp: Double) {
    slots.estimator.estimatedPx = if (dp < 0) -1 else (dp * density).roundToInt()
  }
}
