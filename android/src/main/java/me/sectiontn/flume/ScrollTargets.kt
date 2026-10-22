package me.sectiontn.flume

import android.content.Context
import androidx.recyclerview.widget.LinearSmoothScroller
import android.util.DisplayMetrics
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

class FlumeSmoothScroller(
  context: Context,
  private val viewPosition: Float,
  private val viewOffsetPx: Int,
  private val reverse: Boolean,
  private val msPerPx: Float? = null,
) : LinearSmoothScroller(context) {
  override fun calculateSpeedPerPixel(displayMetrics: DisplayMetrics): Float = msPerPx ?: super.calculateSpeedPerPixel(displayMetrics)

  override fun calculateDtToFit(viewStart: Int, viewEnd: Int, boxStart: Int, boxEnd: Int, snapPreference: Int): Int =
    ScrollTargets.smoothDt(viewStart, viewEnd, boxStart, boxEnd, viewPosition, viewOffsetPx, reverse)

  override fun getVerticalSnapPreference(): Int = SNAP_TO_START

  override fun getHorizontalSnapPreference(): Int = SNAP_TO_START
}
