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
// Frames count from the pin line; a header whose row has not reached it shows in place.
object StickyResolver {
  fun resolve(sticky: IntArray, children: List<ChildFrame>, count: Int, inverted: Boolean, pinnedLength: Int): Pinned? {
    val top = children.filter { it.end > 0 }.minByOrNull { it.start } ?: return null
    val valid = sticky.filter { it < count }
    val index = if (inverted) valid.firstOrNull { it >= top.position } else valid.lastOrNull { it <= top.position }
    if (index == null || children.any { it.position == index && it.start > 0 }) return null
    val pusher = if (inverted) valid.lastOrNull { it < index } else valid.firstOrNull { it > index }
    val frame = pusher?.let { p -> children.firstOrNull { it.position == p } }
    val translation = if (frame != null && frame.start < pinnedLength) frame.start - pinnedLength else 0
    return Pinned(index, translation)
  }
}

// A copy pins at the viewport's leading edge, as a ScrollView sticky header does, inside the padding across the scroll axis.
object StickyPlacement {
  // Left, top, right and bottom.
  fun margins(horizontal: Boolean, left: Int, top: Int, right: Int, bottom: Int): IntArray =
    if (horizontal) intArrayOf(0, top, right, bottom) else intArrayOf(left, 0, right, bottom)

  fun fromViewportEdge(frames: List<ChildFrame>, leadingPaddingPx: Int): List<ChildFrame> =
    frames.map { it.copy(start = it.start + leadingPaddingPx, end = it.end + leadingPaddingPx) }
}

// The sticky rows that keep a React copy: the pinned one and the laid-out ones, so the next is rendered before it pins.
object StickyCopies {
  fun wanted(stickyPositions: IntArray, laidOut: List<ChildFrame>, pinned: Int, isReact: (position: Int) -> Boolean): IntArray {
    val shown = laidOut.mapTo(HashSet()) { it.position }
    return stickyPositions.filter { (it == pinned || it in shown) && isReact(it) }.distinct().sorted().toIntArray()
  }
}

// Owns the pinned copy of the current sticky row: a holder of its own, bound through the engine like any row.
// A drag that starts on the pinned header belongs to the list once it leaves the touch slop; a tap stays with the header.
object PinnedDrag {
  fun takesOver(onPinned: Boolean, dx: Float, dy: Float, slopPx: Int): Boolean =
    onPinned && dx * dx + dy * dy > slopPx.toFloat() * slopPx
}

class StickyHeaderHost(
  private val view: FlumeView,
  val pinnedView: FlumeCellView,
  private val holderId: Int,
  val reactPinned: FlumeSlotHolder,
) {
  var indices = IntArray(0)
  var pinnedIndex = -1
    private set
  private var pinnedLength = 0
  private var wanted = IntArray(0)
  private val copies = HashMap<Int, Int>()
  private var refreshing = false

  init {
    pinnedView.visibility = View.GONE
    pinnedView.onPress = { id, long ->
      val index = view.rows.dataIndex(pinnedIndex)
      if (index >= 0) view.emitPress(index, FlumeJni.keyAt(view.storeId, index), id, long)
    }
    // A copy can render at rest, after the last layout pass wrote the origins.
    reactPinned.onReady = {
      refresh()
      view.requestOrigins()
    }
  }

  // Whether a touch in the host's coordinates lands on a pinned copy where it is drawn.
  fun contains(x: Float, y: Float): Boolean = drawnAt(pinnedView, x, y) || drawnAt(reactPinned, x, y)

  fun textInputAt(x: Float, y: Float): Boolean =
    drawnAt(reactPinned, x, y) &&
      TextInputHit.at(reactPinned, x - reactPinned.left - reactPinned.translationX, y - reactPinned.top - reactPinned.translationY)

  // A React copy handles its own taps; a worklet copy does where it is pressable.
  fun pressableAt(x: Float, y: Float): Boolean {
    if (drawnAt(reactPinned, x, y)) return true
    val left = x - pinnedView.left - pinnedView.translationX
    val top = y - pinnedView.top - pinnedView.translationY
    return pinnedView.isClickable || pinnedView.hasPressableAt(left.toInt(), top.toInt())
  }

  // The React copy reports ready while this pins it, so a nested call is dropped.
  fun refresh(force: Boolean = false) {
    if (refreshing) return
    refreshing = true
    try {
      update(force)
    } finally {
      refreshing = false
    }
  }

  private fun update(force: Boolean) {
    val recycler = view.recycler
    val lm = recycler.layoutManager as? LinearLayoutManager ?: return
    val count = recycler.adapter?.itemCount ?: 0
    if (indices.isEmpty() || count == 0 || view.storeId == 0) {
      syncCopies(IntArray(0), force)
      hide()
      return
    }
    val horizontal = lm.orientation == RecyclerView.HORIZONTAL
    val frames = StickyPlacement.fromViewportEdge(view.childFrames(), if (horizontal) recycler.paddingLeft else recycler.paddingTop)
    var result = StickyResolver.resolve(indices, frames, count, lm.reverseLayout, pinnedLength)
    syncCopies(StickyCopies.wanted(indices, frames, result?.index ?: -1, view::isReactRow), force)
    if (result == null) {
      hide()
      return
    }
    if (view.isReactRow(result.index)) {
      pinnedView.visibility = View.GONE
      if (!pinReact(result.index, horizontal)) return
      result = StickyResolver.resolve(indices, frames, count, lm.reverseLayout, pinnedLength) ?: return hide()
      translate(reactPinned, horizontal, result.translation)
      return
    }
    view.slots.show(reactPinned, 0)
    if (force || result.index != pinnedIndex) {
      if (!bind(result.index, horizontal)) return hide()
      result = StickyResolver.resolve(indices, frames, count, lm.reverseLayout, pinnedLength) ?: return hide()
    }
    place(pinnedView, horizontal)
    pinnedView.visibility = View.VISIBLE
    translate(pinnedView, horizontal, result.translation)
  }

  // Each wanted sticky row keeps a STICKY slot for its key; the holder below shows the pinned one.
  private fun syncCopies(next: IntArray, force: Boolean) {
    if (!force && next.contentEquals(wanted)) return
    wanted = next
    val owner = reactPinned.owner
    val kept = HashMap<Int, Int>()
    for (position in next) {
      val index = view.rows.dataIndex(position).takeIf { it >= 0 } ?: continue
      val group = SlotGroup(view.viewTypeAt(position), pinned = true)
      kept[position] = view.slots.retain(owner, group, SlotKinds.STICKY, FlumeJni.keyAt(view.storeId, index), index)
    }
    for (slot in copies.values) if (slot !in kept.values) view.slots.drop(owner, slot)
    copies.clear()
    copies.putAll(kept)
  }

  // Until the copy has measured nothing is pinned, and the row in the list stays where it is.
  private fun pinReact(position: Int, horizontal: Boolean): Boolean {
    val slot = copies[position] ?: return false.also { hide() }
    reactPinned.horizontal = horizontal
    place(reactPinned, horizontal)
    view.slots.show(reactPinned, slot)
    val copy = reactPinned.view?.takeIf { reactPinned.ready }
    if (copy == null) {
      pinnedIndex = -1
      pinnedLength = 0
      return false
    }
    pinnedIndex = position
    pinnedLength = copy.lengthPx(horizontal)
    return true
  }

  // index is an adapter position of a worklet row.
  private fun bind(index: Int, horizontal: Boolean): Boolean {
    pinnedView.horizontal = horizontal
    val geometry = view.geometry
    val dataIndex = view.rows.dataIndex(index).takeIf { it >= 0 } ?: return false
    val batch = FlumeJni.bind(view.storeId, holderId, dataIndex, geometry.cellWidthDp(fullSpan = true), geometry.cellHeightDp()) ?: return false
    pinnedView.apply(batch)
    pinnedIndex = index
    pinnedLength = if (horizontal) pinnedView.rootWidthPx else pinnedView.rootHeightPx
    layoutPinned(horizontal)
    return true
  }

  // A bind often runs inside the host's own layout pass, which drops the layout request it makes, so the copy is measured here.
  private fun layoutPinned(horizontal: Boolean) {
    val recycler = view.recycler
    val geometry = view.geometry
    pinnedView.forceLayout()
    val laneWidthPx = geometry.innerWidthPx - geometry.lanes.insetStartPx - geometry.lanes.insetEndPx
    val widthSpec = if (horizontal) View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED) else View.MeasureSpec.makeMeasureSpec(laneWidthPx, View.MeasureSpec.EXACTLY)
    val heightSpec = if (horizontal) View.MeasureSpec.makeMeasureSpec(geometry.innerHeightPx, View.MeasureSpec.EXACTLY) else View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
    pinnedView.measure(widthSpec, heightSpec)
    val (left, top) = margins(horizontal)
    pinnedView.layout(left, top, left + pinnedView.measuredWidth, top + pinnedView.measuredHeight)
  }

  // A sticky row is a data row, so it keeps a grid's lane insets.
  private fun margins(horizontal: Boolean): IntArray {
    val recycler = view.recycler
    val lanes = view.geometry.lanes
    return StickyPlacement.margins(
      horizontal, recycler.paddingLeft + lanes.insetStartPx, recycler.paddingTop, recycler.paddingRight + lanes.insetEndPx, recycler.paddingBottom,
    )
  }

  // A copy sits at the viewport's leading edge, inside the content padding across the scroll axis, full width or full height.
  private fun place(copy: View, horizontal: Boolean) {
    val params = FrameLayout.LayoutParams(
      if (horizontal) ViewGroup.LayoutParams.WRAP_CONTENT else ViewGroup.LayoutParams.MATCH_PARENT,
      if (horizontal) ViewGroup.LayoutParams.MATCH_PARENT else ViewGroup.LayoutParams.WRAP_CONTENT,
      Gravity.TOP or Gravity.START,
    )
    val (left, top, right, bottom) = margins(horizontal)
    params.setMargins(left, top, right, bottom)
    val current = copy.layoutParams as? FrameLayout.LayoutParams
    if (current == null || current.width != params.width || current.height != params.height ||
      current.leftMargin != params.leftMargin || current.topMargin != params.topMargin ||
      current.rightMargin != params.rightMargin || current.bottomMargin != params.bottomMargin
    ) {
      copy.layoutParams = params
    }
  }

  private fun translate(copy: View, horizontal: Boolean, translation: Int) {
    if (horizontal) copy.translationX = translation.toFloat() else copy.translationY = translation.toFloat()
  }

  private fun drawnAt(copy: View, x: Float, y: Float): Boolean {
    if (copy.visibility != View.VISIBLE) return false
    val left = copy.left + copy.translationX
    val top = copy.top + copy.translationY
    return x >= left && x < left + copy.width && y >= top && y < top + copy.height
  }

  private fun hide() {
    pinnedView.visibility = View.GONE
    view.slots.show(reactPinned, 0)
    pinnedIndex = -1
    pinnedLength = 0
  }
}
