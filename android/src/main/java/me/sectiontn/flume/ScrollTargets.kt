package me.sectiontn.flume

import android.content.Context
import android.util.DisplayMetrics
import android.view.View
import androidx.recyclerview.widget.LinearSmoothScroller
import androidx.recyclerview.widget.RecyclerView
import kotlin.math.abs
import kotlin.math.min
import kotlin.math.roundToInt

// viewPosition is in content order: 0 puts the item's start edge at the list's start edge, 1 its end at the end.
object ScrollTargets {
  // An offset at or before the content start is the first row, which a position scroll reaches exactly; a further offset is an estimate.
  fun startRow(targetPx: Int): Int? = if (targetPx <= 0) 0 else null

  fun clampIndex(index: Int, count: Int): Int? = if (count <= 0) null else index.coerceIn(0, count - 1)

  // FlatList scrolls to the end for an initial index at or past the data.
  fun initialPosition(index: Int, rows: ListRows): Int =
    if (index < rows.dataCount) rows.position(maxOf(0, index)) else maxOf(0, rows.itemCount - 1)

  // The rows a smooth scroll crosses; in a grid several positions share a row.
  fun rowSpan(from: Int, to: Int, groupOf: (Int) -> Int): Int = abs(groupOf(to) - groupOf(from))

  // At viewPosition 0 the offset does not depend on the row's length.
  fun needsLength(viewPosition: Float): Boolean = viewPosition != 0f

  // Past two screens a smooth scroll passes over its target between frames, so it starts one screen before it.
  fun preJump(first: Int, target: Int, perScreen: Int, itemCount: Int): Int? {
    if (perScreen <= 0 || abs(target - first) <= 2 * perScreen) return null
    val near = if (target < first) target + perScreen else target - perScreen
    return near.coerceIn(0, itemCount - 1)
  }

  const val JUMP_MARGIN_ROWS = 2
  const val MAX_JUMP_ROWS = 48
  const val JUMP_WAIT_MS = 1000L

  // The positions a jump shows, placed as FlatList places the item; a zero-length position belongs to the row before it.
  fun jumpWindow(target: Int, viewPosition: Float, viewportPx: Int, itemCount: Int, lengthPx: (Int) -> Int): IntRange {
    if (itemCount <= 0) return IntRange.EMPTY
    val at = target.coerceIn(0, itemCount - 1)
    val before = (viewPosition * (viewportPx - lengthPx(at))).roundToInt().coerceAtLeast(0)
    var first = at
    var covered = 0
    while (first > 0 && covered < before) covered += lengthPx(--first)
    val after = viewportPx - before - lengthPx(at)
    var last = at
    covered = 0
    while (last < itemCount - 1 && (covered < after || lengthPx(last + 1) == 0)) covered += lengthPx(++last)
    return (first - JUMP_MARGIN_ROWS).coerceAtLeast(0)..(last + JUMP_MARGIN_ROWS).coerceAtMost(itemCount - 1)
  }

  // An animated jump shows its landing screen and every row it glides over; past the cap, the rows nearest the target.
  fun glideWindow(landing: IntRange, target: IntRange): IntRange {
    val span = minOf(landing.first, target.first)..maxOf(landing.last, target.last)
    if (span.last - span.first + 1 <= MAX_JUMP_ROWS) return span
    return if (landing.first < target.first) {
      (span.last - MAX_JUMP_ROWS + 1)..span.last
    } else {
      span.first..(span.first + MAX_JUMP_ROWS - 1)
    }
  }

  // A list that stacks from the end anchors relayouts on its last row, so even a start-aligned React target drifts.
  fun realigns(reactTarget: Boolean, viewPosition: Float, stacksFromEnd: Boolean): Boolean =
    reactTarget && (needsLength(viewPosition) || stacksFromEnd)

  // A ScrollView animates any distance in a fixed time; a long jump speeds up so it fits the budget.
  fun speedMsPerPx(defaultMsPerPx: Float, distancePx: Int, budgetMs: Int): Float =
    if (distancePx <= 0) defaultMsPerPx else min(defaultMsPerPx, budgetMs.toFloat() / distancePx)

  // The offset for scrollToPositionWithOffset, which already counts from the end under a reversed layout.
  fun offsetPx(viewPosition: Float, viewOffsetPx: Int, viewportPx: Int, itemPx: Int): Int =
    (viewPosition * (viewportPx - itemPx)).roundToInt() + viewOffsetPx

  // The delta for calculateDtToFit, in visual pixels: under a reversed layout the content start is the box end.
  fun smoothDt(viewStart: Int, viewEnd: Int, boxStart: Int, boxEnd: Int, viewPosition: Float, viewOffsetPx: Int, reverse: Boolean): Int {
    val slack = (boxEnd - boxStart) - (viewEnd - viewStart)
    val shift = (viewPosition * slack).roundToInt()
    return if (reverse) (boxEnd - shift - viewOffsetPx) - viewEnd else (boxStart + shift + viewOffsetPx) - viewStart
  }
}

// A React row is aligned with the length it has when the scroll starts, often the estimate, and the layout manager then
// anchors on the first visible row (the last under stackFromEnd): rows measuring between it and the target push it, and
// it may measure at another length itself.
data class PendingAlign(
  val position: Int,
  val viewPosition: Float,
  val viewOffsetPx: Int,
  val assumedLengthPx: Int,
  val corrections: Int = 0,
  val anchoredAtEnd: Boolean = false,
)

object AlignCorrection {
  // The offset to apply again when the target does not start where its length puts it (targetStartPx null: out of the
  // layout); null where it sits, and at viewPosition 0 when the anchor keeps it (not under stackFromEnd).
  fun offsetPx(pending: PendingAlign, targetReady: Boolean, targetLengthPx: Int, targetStartPx: Int?, viewportPx: Int): Int? {
    if (pending.viewPosition == 0f && !pending.anchoredAtEnd) return null
    val length = if (targetReady) targetLengthPx else pending.assumedLengthPx
    val desired = ScrollTargets.offsetPx(pending.viewPosition, pending.viewOffsetPx, viewportPx, length)
    return if (targetStartPx == desired) null else desired
  }
}

class FlumeSmoothScroller(
  context: Context,
  private val viewPosition: Float,
  private val viewOffsetPx: Int,
  private val reverse: Boolean,
  private val msPerPx: Float? = null,
) : LinearSmoothScroller(context) {
  var onTarget: ((View) -> Unit)? = null

  override fun calculateSpeedPerPixel(displayMetrics: DisplayMetrics): Float = msPerPx ?: super.calculateSpeedPerPixel(displayMetrics)

  override fun onTargetFound(targetView: View, state: RecyclerView.State, action: Action) {
    onTarget?.invoke(targetView)
    super.onTargetFound(targetView, state, action)
  }

  override fun calculateDtToFit(viewStart: Int, viewEnd: Int, boxStart: Int, boxEnd: Int, snapPreference: Int): Int =
    ScrollTargets.smoothDt(viewStart, viewEnd, boxStart, boxEnd, viewPosition, viewOffsetPx, reverse)

  override fun getVerticalSnapPreference(): Int = SNAP_TO_START

  override fun getHorizontalSnapPreference(): Int = SNAP_TO_START
}
