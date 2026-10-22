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

  override fun onDraw(canvas: Canvas, parent: RecyclerView, state: RecyclerView.State) {
    if (transparent) return
    val lm = parent.layoutManager ?: return
    for (i in 0 until parent.childCount) {
      val child = parent.getChildAt(i)
      val edge = edgeFor(child, parent, state) ?: continue
      val tx = child.translationX
      val ty = child.translationY
      when (edge) {
        Edge.BOTTOM, Edge.TOP -> {
          val left = max(child.left, parent.paddingLeft + insetStartPx) + tx
          val right = min(child.right, parent.width - parent.paddingRight - insetEndPx) + tx
          if (edge == Edge.BOTTOM) canvas.drawRect(left, child.bottom + ty, right, lm.getDecoratedBottom(child) + ty, paint)
          else canvas.drawRect(left, lm.getDecoratedTop(child) + ty, right, child.top + ty, paint)
        }
        Edge.RIGHT, Edge.LEFT -> {
          val top = max(child.top, parent.paddingTop + insetStartPx) + ty
          val bottom = min(child.bottom, parent.height - parent.paddingBottom - insetEndPx) + ty
          if (edge == Edge.RIGHT) canvas.drawRect(child.right + tx, top, lm.getDecoratedRight(child) + tx, bottom, paint)
          else canvas.drawRect(lm.getDecoratedLeft(child) + tx, top, child.left + tx, bottom, paint)
        }
      }
    }
  }
}
