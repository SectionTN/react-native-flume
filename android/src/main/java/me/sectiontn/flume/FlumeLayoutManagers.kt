package me.sectiontn.flume

import android.content.Context
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

// -1 is one viewport; the space is laid out on both sides so cells are bound ahead in either direction.
object ExtraLayoutSpace {
  fun of(drawDistancePx: Int, viewportPx: Int): Int = if (drawDistancePx < 0) viewportPx else drawDistancePx
}

private fun LinearLayoutManager.viewportPx(): Int =
  if (orientation == RecyclerView.VERTICAL) height - paddingTop - paddingBottom else width - paddingLeft - paddingRight

private fun LinearLayoutManager.fillExtraSpace(drawDistancePx: Int, extraLayoutSpace: IntArray) {
  val extra = ExtraLayoutSpace.of(drawDistancePx, viewportPx())
  extraLayoutSpace[0] = extra
  extraLayoutSpace[1] = extra
}

class FlumeLinearLayoutManager(context: Context, orientation: Int, reverse: Boolean) :
  LinearLayoutManager(context, orientation, reverse) {
  var drawDistancePx = -1
  var onLaidOut: (() -> Unit)? = null
  var justify = ContentJustify.Start

  override fun onLayoutChildren(recycler: RecyclerView.Recycler, state: RecyclerView.State) {
    super.onLayoutChildren(recycler, state)
    if (justify != ContentJustify.Start) placeShortContent(this, state, justify)
  }

  override fun calculateExtraLayoutSpace(state: RecyclerView.State, extraLayoutSpace: IntArray) =
    fillExtraSpace(drawDistancePx, extraLayoutSpace)

  override fun onLayoutCompleted(state: RecyclerView.State?) {
    super.onLayoutCompleted(state)
    onLaidOut?.invoke()
  }
}

class FlumeGridLayoutManager(context: Context, spanCount: Int, orientation: Int, reverse: Boolean) :
  GridLayoutManager(context, spanCount, orientation, reverse) {
  var drawDistancePx = -1
  var onLaidOut: (() -> Unit)? = null
  var justify = ContentJustify.Start

  override fun onLayoutChildren(recycler: RecyclerView.Recycler, state: RecyclerView.State) {
    super.onLayoutChildren(recycler, state)
    if (justify != ContentJustify.Start) placeShortContent(this, state, justify)
  }
  val fullSpanLookup = FlumeSpanSizeLookup { this.spanCount }

  init {
    spanSizeLookup = fullSpanLookup
    // A stretched cell keeps its row's height as its measure, so a row that lost its tallest cell must measure again.
    isMeasurementCacheEnabled = false
  }

  override fun calculateExtraLayoutSpace(state: RecyclerView.State, extraLayoutSpace: IntArray) =
    fillExtraSpace(drawDistancePx, extraLayoutSpace)

  override fun onLayoutCompleted(state: RecyclerView.State?) {
    super.onLayoutCompleted(state)
    onLaidOut?.invoke()
  }
}

// The positions that take every span of a grid row, the sticky headers; rows and columns follow from them in O(log n).
class FullSpans(positions: IntArray) {
  private val sorted = positions.filter { it >= 0 }.distinct().sorted().toIntArray()
  private var rowsSpanCount = 0
  private var rows = IntArray(0)

  fun contains(position: Int): Boolean = sorted.binarySearch(position) >= 0

  // Columns restart after each full-span row, as GridLayoutManager places them.
  fun spanIndex(position: Int, spanCount: Int): Int {
    val found = sorted.binarySearch(position)
    return if (found >= 0) 0 else (position - runStart(-found - 2)) % spanCount
  }

  fun groupIndex(position: Int, spanCount: Int): Int {
    val rows = rowsFor(spanCount)
    val found = sorted.binarySearch(position)
    if (found >= 0) return rows[found]
    val previous = -found - 2
    return (if (previous >= 0) rows[previous] + 1 else 0) + (position - runStart(previous)) / spanCount
  }

  private fun runStart(previousFullSpan: Int) = if (previousFullSpan >= 0) sorted[previousFullSpan] + 1 else 0

  // The row of each full-span position: the previous one's row, then the rows its run of single spans fills.
  private fun rowsFor(spanCount: Int): IntArray {
    if (spanCount == rowsSpanCount) return rows
    val next = IntArray(sorted.size)
    for (i in sorted.indices) {
      val run = sorted[i] - runStart(i - 1)
      next[i] = (if (i > 0) next[i - 1] + 1 else 0) + (run + spanCount - 1) / spanCount
    }
    rows = next
    rowsSpanCount = spanCount
    return next
  }
}

// Sticky header rows take every span; everything else takes one.
class FlumeSpanSizeLookup(private val spanCount: () -> Int) : GridLayoutManager.SpanSizeLookup() {
  var fullSpans = FullSpans(IntArray(0))

  override fun getSpanSize(position: Int): Int = if (fullSpans.contains(position)) spanCount() else 1

  // RecyclerView's own versions walk every earlier position, uncached by default.
  override fun getSpanIndex(position: Int, spanCount: Int): Int = fullSpans.spanIndex(position, spanCount)

  override fun getSpanGroupIndex(adapterPosition: Int, spanCount: Int): Int = fullSpans.groupIndex(adapterPosition, spanCount)
}

// inverted only reverses: stacking from the end on a reversed layout would start the list at its oldest end.
object LayoutFlags {
  fun stackFromEnd(inverted: Boolean, startRenderingFromBottom: Boolean): Boolean = !inverted && startRenderingFromBottom
}

// What the layout manager is built from. Replacing it drops every row and starts over at the first, and Fabric can deliver
// a prop again with the value it has, so only a new shape rebuilds it; stackFromEnd applies to a single column.
data class LayoutShape(val horizontal: Boolean, val numColumns: Int, val inverted: Boolean, val stackFromEnd: Boolean) {
  companion object {
    fun of(horizontal: Boolean, numColumns: Int, inverted: Boolean, startRenderingFromBottom: Boolean) =
      LayoutShape(horizontal, numColumns, inverted, numColumns == 1 && LayoutFlags.stackFromEnd(inverted, startRenderingFromBottom))

    // A column or axis change keeps the first visible item; a new direction or stacking opens at its edge instead.
    fun keepsPosition(previous: LayoutShape?, next: LayoutShape): Boolean =
      previous != null && previous.inverted == next.inverted && previous.stackFromEnd == next.stackFromEnd
  }
}

// FlatList's flexGrow content with justifyContent: short upright content moves to its place after every layout.
internal fun placeShortContent(lm: LinearLayoutManager, state: RecyclerView.State, justify: ContentJustify) {
  if (state.isPreLayout || lm.childCount == 0 || lm.reverseLayout) return
  val horizontal = lm.orientation == RecyclerView.HORIZONTAL
  var first = Int.MAX_VALUE
  var last = Int.MIN_VALUE
  var hasFirst = false
  var hasLast = false
  for (i in 0 until lm.childCount) {
    val child = lm.getChildAt(i) ?: continue
    first = minOf(first, if (horizontal) lm.getDecoratedLeft(child) else lm.getDecoratedTop(child))
    last = maxOf(last, if (horizontal) lm.getDecoratedRight(child) else lm.getDecoratedBottom(child))
    val position = lm.getPosition(child)
    if (position == 0) hasFirst = true
    if (position == state.itemCount - 1) hasLast = true
  }
  if (!hasFirst || !hasLast) return
  val start = if (horizontal) lm.paddingLeft else lm.paddingTop
  val end = if (horizontal) lm.width - lm.paddingRight else lm.height - lm.paddingBottom
  val shift = ContentPlacement.shift(justify, first, last, start, end)
  if (shift == 0) return
  if (horizontal) lm.offsetChildrenHorizontal(shift) else lm.offsetChildrenVertical(shift)
}

// contentContainerStyle justifyContent with flexGrow, as the share of the free space that goes before short content.
enum class ContentJustify(val lead: Float) {
  Start(0f),
  Center(0.5f),
  End(1f);

  companion object {
    fun of(value: String?): ContentJustify = when (value) {
      "center" -> Center
      "end" -> End
      else -> Start
    }
  }
}

object ContentPlacement {
  // The move that puts short content at its place in the viewport, 0 once it is there or when it fills the viewport.
  fun shift(justify: ContentJustify, firstStart: Int, lastEnd: Int, viewportStart: Int, viewportEnd: Int): Int {
    val content = lastEnd - firstStart
    val viewport = viewportEnd - viewportStart
    if (content >= viewport) return 0
    return viewportStart + ((viewport - content) * justify.lead).toInt() - firstStart
  }

  // A growing footer takes what the rows above it leave of the viewport.
  fun footerMinPx(contentStart: Int, footerStart: Int, viewportPx: Int): Int = growSharePx(viewportPx, footerStart - contentStart, 1)

  // The empty row and a growing footer split what the other rows leave evenly, as two flex: 1 rows.
  fun growSharePx(viewportPx: Int, fixedPx: Int, growers: Int): Int = maxOf(0, viewportPx - fixedPx) / maxOf(1, growers)
}
