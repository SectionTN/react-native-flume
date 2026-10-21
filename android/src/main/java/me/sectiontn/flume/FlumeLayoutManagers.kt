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
  val fullSpanLookup = FlumeSpanSizeLookup { this.spanCount }

  init {
    spanSizeLookup = fullSpanLookup
  }

  override fun calculateExtraLayoutSpace(state: RecyclerView.State, extraLayoutSpace: IntArray) =
    fillExtraSpace(drawDistancePx, extraLayoutSpace)

  override fun onLayoutCompleted(state: RecyclerView.State?) {
    super.onLayoutCompleted(state)
    onLaidOut?.invoke()
  }
}

// Sticky header rows take every span; everything else takes one.
class FlumeSpanSizeLookup(private val spanCount: () -> Int) : GridLayoutManager.SpanSizeLookup() {
  var isFullSpan: (Int) -> Boolean = { false }

  override fun getSpanSize(position: Int): Int = if (isFullSpan(position)) spanCount() else 1
}

// inverted only reverses: stacking from the end on a reversed layout would start the list at its oldest end.
object LayoutFlags {
  fun stackFromEnd(inverted: Boolean, startRenderingFromBottom: Boolean): Boolean = !inverted && startRenderingFromBottom
}
