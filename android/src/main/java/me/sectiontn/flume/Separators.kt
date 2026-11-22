package me.sectiontn.flume

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.view.View
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

enum class Edge { TOP, BOTTOM, LEFT, RIGHT }

// The gap belongs to the row before it in content order, so the last row carries none.
object SeparatorGeometry {
  fun edge(row: Int, lastRow: Int, horizontal: Boolean, inverted: Boolean): Edge? {
    if (row >= lastRow) return null
    return if (horizontal) (if (inverted) Edge.LEFT else Edge.RIGHT) else (if (inverted) Edge.TOP else Edge.BOTTOM)
  }

  // Separators run between data rows only; rowOf gives a position's row, its span group in a grid.
  fun edgeAt(position: Int, rows: ListRows, rowOf: (Int) -> Int, horizontal: Boolean, inverted: Boolean): Edge? {
    if (rows.kind(position) != RowKind.DATA) return null
    return edge(rowOf(position), rowOf(rows.position(rows.dataCount - 1)), horizontal, inverted)
  }

  // Left, top, right and bottom: next to the row on its edge, across the decorated cell inside the clip.
  fun line(edge: Edge, child: IntArray, decorated: IntArray, sizePx: Int, clipStart: Int, clipEnd: Int, out: IntArray = IntArray(4)): IntArray {
    when (edge) {
      Edge.BOTTOM, Edge.TOP -> {
        out[0] = max(decorated[0], clipStart)
        out[2] = min(decorated[2], clipEnd)
        out[1] = if (edge == Edge.BOTTOM) child[3] else child[1] - sizePx
        out[3] = out[1] + sizePx
      }
      Edge.RIGHT, Edge.LEFT -> {
        out[1] = max(decorated[1], clipStart)
        out[3] = min(decorated[3], clipEnd)
        out[0] = if (edge == Edge.RIGHT) child[2] else child[0] - sizePx
        out[2] = out[0] + sizePx
      }
    }
    return out
  }
}

// Along the scroll axis a row's line comes first, then its gaps.
object GapGeometry {
  fun after(row: Int, lastRow: Int, dataGap: Boolean, mainGapPx: Int, gridRowGapPx: Int): Int =
    (if (row < lastRow) mainGapPx else 0) + (if (dataGap) gridRowGapPx else 0)

  // Left and right offsets that put lane k at insetStart + k * (width + gap) in its equal share of the inner width.
  fun laneOffsets(lane: Int, lanes: Int, innerPx: Int, insetStartPx: Int, insetEndPx: Int, gapPx: Int): IntArray {
    val share = innerPx.toFloat() / lanes
    val width = (innerPx - insetStartPx - insetEndPx - gapPx * (lanes - 1)).toFloat() / lanes
    val sum = (share - width).roundToInt()
    val left = (insetStartPx + lane * (width + gapPx) - lane * share).roundToInt()
    return intArrayOf(left, sum - left)
  }
}

// Spaces rows after their line and places a grid's columns inside the lane insets. gapsPx is main, grid row, column, inset start, inset end.
class GapDecoration(private val gapsPx: IntArray, private val rows: () -> ListRows) : RecyclerView.ItemDecoration() {
  override fun getItemOffsets(outRect: Rect, view: View, parent: RecyclerView, state: RecyclerView.State) {
    outRect.setEmpty()
    val lm = parent.layoutManager as? LinearLayoutManager ?: return
    val position = parent.getChildAdapterPosition(view)
    val rows = rows()
    if (position == RecyclerView.NO_POSITION || rows.itemCount == 0) return
    val grid = lm as? GridLayoutManager
    val rowOf: (Int) -> Int = if (grid != null) { p -> grid.spanSizeLookup.getSpanGroupIndex(p, grid.spanCount) } else { p -> p }
    val horizontal = lm.orientation == RecyclerView.HORIZONTAL
    val inverted = lm.reverseLayout
    val dataGap = SeparatorGeometry.edgeAt(position, rows, rowOf, horizontal, inverted) != null
    val main = GapGeometry.after(rowOf(position), rowOf(rows.itemCount - 1), dataGap, gapsPx[0], gapsPx[1])
    when {
      horizontal && inverted -> outRect.left = main
      horizontal -> outRect.right = main
      inverted -> outRect.top = main
      else -> outRect.bottom = main
    }
    if (grid == null || horizontal || rows.kind(position) != RowKind.DATA) return
    if (grid.spanSizeLookup.getSpanSize(position) >= grid.spanCount) {
      outRect.left = gapsPx[3]
      outRect.right = gapsPx[4]
      return
    }
    val lane = grid.spanSizeLookup.getSpanIndex(position, grid.spanCount)
    val inner = parent.width - parent.paddingLeft - parent.paddingRight
    val offsets = GapGeometry.laneOffsets(lane, grid.spanCount, inner, gapsPx[3], gapsPx[4], gapsPx[2])
    outRect.left = offsets[0]
    outRect.right = offsets[1]
  }
}

// Reserves the gap through item offsets and paints it, inset along the cross axis from the padded edges.
class SeparatorDecoration(
  private val sizePx: Int,
  color: Int,
  private val insetStartPx: Int,
  private val insetEndPx: Int,
  private val rows: () -> ListRows,
) : RecyclerView.ItemDecoration() {
  private val paint = Paint().apply { this.color = color }
  private val transparent = Color.alpha(color) == 0

  override fun getItemOffsets(outRect: Rect, view: View, parent: RecyclerView, state: RecyclerView.State) {
    outRect.setEmpty()
    when (edgeFor(view, parent, state)) {
      Edge.TOP -> outRect.top = sizePx
      Edge.BOTTOM -> outRect.bottom = sizePx
      Edge.LEFT -> outRect.left = sizePx
      Edge.RIGHT -> outRect.right = sizePx
      null -> Unit
    }
  }

  // The row of a position is its span group in a grid and the position itself in a list.
  private fun edgeFor(view: View, parent: RecyclerView, state: RecyclerView.State): Edge? {
    val lm = parent.layoutManager as? LinearLayoutManager ?: return null
    val position = parent.getChildAdapterPosition(view)
    if (position == RecyclerView.NO_POSITION || state.itemCount == 0) return null
    val rowOf: (Int) -> Int = if (lm is GridLayoutManager) { p -> lm.spanSizeLookup.getSpanGroupIndex(p, lm.spanCount) } else { p -> p }
    return SeparatorGeometry.edgeAt(position, rows(), rowOf, lm.orientation == RecyclerView.HORIZONTAL, lm.reverseLayout)
  }

  private val childBounds = IntArray(4)
  private val decoratedBounds = IntArray(4)
  private val lineBounds = IntArray(4)

  // The decorated cell spans its column's gaps and the lane insets, so a grid row's line runs unbroken.
  override fun onDraw(canvas: Canvas, parent: RecyclerView, state: RecyclerView.State) {
    if (transparent) return
    val lm = parent.layoutManager ?: return
    for (i in 0 until parent.childCount) {
      val child = parent.getChildAt(i)
      val edge = edgeFor(child, parent, state) ?: continue
      childBounds[0] = child.left
      childBounds[1] = child.top
      childBounds[2] = child.right
      childBounds[3] = child.bottom
      decoratedBounds[0] = lm.getDecoratedLeft(child)
      decoratedBounds[1] = lm.getDecoratedTop(child)
      decoratedBounds[2] = lm.getDecoratedRight(child)
      decoratedBounds[3] = lm.getDecoratedBottom(child)
      val across = edge == Edge.BOTTOM || edge == Edge.TOP
      val clipStart = (if (across) parent.paddingLeft else parent.paddingTop) + insetStartPx
      val clipEnd = (if (across) parent.width - parent.paddingRight else parent.height - parent.paddingBottom) - insetEndPx
      val line = SeparatorGeometry.line(edge, childBounds, decoratedBounds, sizePx, clipStart, clipEnd, lineBounds)
      val tx = child.translationX
      val ty = child.translationY
      canvas.drawRect(line[0] + tx, line[1] + ty, line[2] + tx, line[3] + ty, paint)
    }
  }
}
