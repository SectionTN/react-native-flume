package me.sectiontn.flume

import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

// A laid-out row along the scroll axis, in px from the viewport's leading edge after padding, translation included.
data class ChildFrame(val position: Int, val start: Int, val end: Int)

data class Pinned(val index: Int, val translation: Int)

// Which sticky row is pinned and how far the next header pushes it; indices at or past the count are ignored.
object StickyResolver {
  fun resolve(sticky: IntArray, children: List<ChildFrame>, count: Int, inverted: Boolean, pinnedLength: Int): Pinned? {
    val top = children.filter { it.end > 0 }.minByOrNull { it.start } ?: return null
    val valid = sticky.filter { it < count }
    val index = if (inverted) valid.firstOrNull { it >= top.position } else valid.lastOrNull { it <= top.position }
    if (index == null) return null
    val pusher = if (inverted) valid.lastOrNull { it < index } else valid.firstOrNull { it > index }
    val frame = pusher?.let { p -> children.firstOrNull { it.position == p } }
    val translation = if (frame != null && frame.start < pinnedLength) frame.start - pinnedLength else 0
    return Pinned(index, translation)
  }
}

// Owns the pinned copy of the current sticky row: a holder of its own, bound through the engine like any row.
class StickyHeaderHost(private val view: FlumeView, val pinnedView: FlumeCellView, private val holderId: Int) {
  var indices = IntArray(0)
  var pinnedIndex = -1
    private set
  private var pinnedLength = 0

  init {
    pinnedView.visibility = View.GONE
    pinnedView.onPress = { id, long ->
      if (pinnedIndex >= 0) view.emitPress(pinnedIndex, FlumeJni.keyAt(view.storeId, pinnedIndex), id, long)
    }
  }

  fun refresh(force: Boolean = false) {
    val recycler = view.recycler
    val lm = recycler.layoutManager as? LinearLayoutManager ?: return
    val count = recycler.adapter?.itemCount ?: 0
    if (indices.isEmpty() || count == 0 || view.storeId == 0) {
      hide()
      return
    }
    val horizontal = lm.orientation == RecyclerView.HORIZONTAL
    val frames = view.childFrames()
    var result = StickyResolver.resolve(indices, frames, count, lm.reverseLayout, pinnedLength)
    if (result == null) {
      hide()
      return
    }
    if (force || result.index != pinnedIndex) {
      bind(result.index, horizontal)
      result = StickyResolver.resolve(indices, frames, count, lm.reverseLayout, pinnedLength) ?: return hide()
    }
    place(horizontal)
    pinnedView.visibility = View.VISIBLE
    if (horizontal) pinnedView.translationX = result.translation.toFloat() else pinnedView.translationY = result.translation.toFloat()
  }

  private fun bind(index: Int, horizontal: Boolean) {
    pinnedView.horizontal = horizontal
    val geometry = view.geometry
    val batch = FlumeJni.bind(view.storeId, holderId, index, geometry.cellWidthDp(fullSpan = true), geometry.cellHeightDp()) ?: return
    pinnedView.apply(batch)
    pinnedIndex = index
    pinnedLength = if (horizontal) pinnedView.rootWidthPx else pinnedView.rootHeightPx
  }

  // The copy sits at the leading edge inside the content padding, full width or full height.
  private fun place(horizontal: Boolean) {
    val recycler = view.recycler
    val params = FrameLayout.LayoutParams(
      if (horizontal) ViewGroup.LayoutParams.WRAP_CONTENT else ViewGroup.LayoutParams.MATCH_PARENT,
      if (horizontal) ViewGroup.LayoutParams.MATCH_PARENT else ViewGroup.LayoutParams.WRAP_CONTENT,
      Gravity.TOP or Gravity.START,
    )
    params.setMargins(recycler.paddingLeft, recycler.paddingTop, recycler.paddingRight, recycler.paddingBottom)
    val current = pinnedView.layoutParams as? FrameLayout.LayoutParams
    if (current == null || current.width != params.width || current.height != params.height ||
      current.leftMargin != params.leftMargin || current.topMargin != params.topMargin ||
      current.rightMargin != params.rightMargin || current.bottomMargin != params.bottomMargin
    ) {
      pinnedView.layoutParams = params
    }
  }

  private fun hide() {
    pinnedView.visibility = View.GONE
    pinnedIndex = -1
    pinnedLength = 0
  }
}
