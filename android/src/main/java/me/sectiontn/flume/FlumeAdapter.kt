package me.sectiontn.flume

import android.content.Context
import android.os.SystemClock
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

class FlumeAdapter(
  private val context: Context,
  private val measurer: FlumeTextMeasurer,
  private val recycler: RecyclerView,
) : RecyclerView.Adapter<FlumeAdapter.Holder>(), FlumeStoreListener, ListNotifier {
  class Holder(val cell: FlumeCellView) : RecyclerView.ViewHolder(cell)

  private val density = context.resources.displayMetrics.density
  val geometry = ListGeometry(density)
  private val attached = AttachTracker()
  var onPress: ((position: Int, pressId: String?, long: Boolean) -> Unit)? = null
  var isFullSpan: (Int) -> Boolean = { false }
  var onFirstData: (() -> Unit)? = null
  var onDataChanged: ((reset: Boolean) -> Unit)? = null
  var rowsPressable = false
  var anchorConfig = AnchorConfig()
  var onAutoscroll: ((index: Int, viewPosition: Float, animated: Boolean) -> Unit)? = null
  private val storeId get() = attached.storeId
  private var count = 0
  private var nextCellId = 1

  // A new size or font scale drops every measured text and rebinds the visible rows at the new size.
  fun onViewportChanged(widthPx: Int, heightPx: Int, fontScale: Float): Boolean {
    val changed = geometry.update(widthPx, heightPx, fontScale)
    if (changed) rebindAll()
    return changed
  }

  // Orientation, columns and padding come from the host; a change of the cell size rebinds every row.
  fun configure(horizontal: Boolean, numColumns: Int, inverted: Boolean, paddingPx: IntArray) {
    if (geometry.configure(horizontal, numColumns, inverted, paddingPx)) rebindAll()
  }

  private fun rebindAll() {
    measurer.invalidate()
    if (storeId != 0) FlumeJni.invalidateMeasures(storeId)
    notifyDataSetChanged()
  }

  init {
    setHasStableIds(true)
  }

  // The holders and their trees survive a detach; a re-attach reloads the rows from the engine's current order.
  fun attach(storeId: Int) {
    if (this.storeId != 0 && this.storeId != storeId) detach()
    val reload = attached.onAttach(storeId)
    val attachedCount = FlumeJni.attach(storeId, this, measurer, density)
    if (reload || attachedCount != count) {
      setCount(attachedCount)
      notifyDataSetChanged()
      onDataChanged?.invoke(true)
    }
  }

  fun detach() {
    if (storeId == 0) return
    FlumeJni.detach(storeId)
    attached.onDetach()
  }

  override fun onListChanged(reset: Boolean, removed: IntArray, inserted: IntArray, moved: IntArray, changed: IntArray, count: Int) {
    val before = if (reset) null else anchorState()
    setCount(count)
    ListOpsApplier.apply(reset, removed, inserted, moved, changed, this)
    anchor(before, inserted, count)
    onDataChanged?.invoke(reset)
  }

  // Inserts touching the content start or end map to the visual edges through the orientation.
  private fun anchor(before: AnchorState?, inserted: IntArray, count: Int) {
    var atStart = false
    var atEnd = false
    for (i in inserted.indices step 2) {
      if (inserted[i] == 0) atStart = true
      if (inserted[i] + inserted[i + 1] == count) atEnd = true
    }
    val inverted = geometry.inverted
    val decision = AnchorPolicy.decide(anchorConfig, before, if (inverted) atEnd else atStart, if (inverted) atStart else atEnd)
    decision.restore?.let { (position, offset) -> (recycler.layoutManager as? LinearLayoutManager)?.scrollToPositionWithOffset(position, offset) }
    decision.scrollTo?.let { edge ->
      val (index, viewPosition) = AnchorTargets.indexFor(edge, count, inverted)
      recycler.post { onAutoscroll?.invoke(index, viewPosition, decision.animated) }
    }
  }

  // The visually topmost row still in view, with its offset the way the layout manager restores it.
  private fun anchorState(): AnchorState? {
    val lm = recycler.layoutManager as? LinearLayoutManager ?: return null
    val horizontal = geometry.horizontal
    val padStart = if (horizontal) recycler.paddingLeft else recycler.paddingTop
    var top: View? = null
    var topStart = Int.MAX_VALUE
    for (i in 0 until lm.childCount) {
      val child = lm.getChildAt(i) ?: continue
      val start = if (horizontal) lm.getDecoratedLeft(child) else lm.getDecoratedTop(child)
      val end = if (horizontal) lm.getDecoratedRight(child) else lm.getDecoratedBottom(child)
      if (end <= padStart || start >= topStart) continue
      topStart = start
      top = child
    }
    val child = top ?: return null
    val position = lm.getPosition(child)
    if (position == RecyclerView.NO_POSITION) return null
    val offset = if (!lm.reverseLayout) {
      topStart - padStart
    } else if (horizontal) {
      recycler.width - recycler.paddingRight - lm.getDecoratedRight(child)
    } else {
      recycler.height - recycler.paddingBottom - lm.getDecoratedBottom(child)
    }
    val scrollOffset = if (horizontal) recycler.computeHorizontalScrollOffset() else recycler.computeVerticalScrollOffset()
    val range = if (horizontal) recycler.computeHorizontalScrollRange() else recycler.computeVerticalScrollRange()
    val extent = if (horizontal) recycler.computeHorizontalScrollExtent() else recycler.computeVerticalScrollExtent()
    return AnchorState(position, offset, scrollOffset, range - scrollOffset - extent, if (horizontal) geometry.innerWidthPx else geometry.innerHeightPx)
  }

  fun allocateHolderId(): Int = nextCellId++

  // Sticky rows bind at the full width in a grid, so a change of the set rebinds the rows that switched.
  fun onStickyChanged(previous: Set<Int>, next: Set<Int>) {
    if (geometry.numColumns <= 1) return
    for (position in (previous - next) + (next - previous)) if (position < count) notifyItemChanged(position)
  }

  private fun setCount(next: Int) {
    val first = count == 0 && next > 0
    count = next
    if (first) onFirstData?.invoke()
  }

  override fun reset() = notifyDataSetChanged()

  override fun removed(index: Int, count: Int) = notifyItemRangeRemoved(index, count)

  override fun inserted(index: Int, count: Int) = notifyItemRangeInserted(index, count)

  override fun moved(from: Int, to: Int) = notifyItemMoved(from, to)

  override fun changed(index: Int) = notifyItemChanged(index)

  override fun getItemCount(): Int = count

  override fun getItemViewType(position: Int): Int = FlumeJni.viewType(storeId, position)

  override fun getItemId(position: Int): Long = FlumeJni.itemId(storeId, position)

  override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
    val cell = FlumeCellView(context, measurer)
    cell.cellId = nextCellId++
    cell.rowsPressable = rowsPressable
    orient(cell)
    val holder = Holder(cell)
    // The position is read at click time, so a recycled holder reports the item it shows now.
    cell.onPress = { pressId, long ->
      val position = holder.bindingAdapterPosition
      if (position != RecyclerView.NO_POSITION) onPress?.invoke(position, pressId, long)
    }
    return holder
  }

  // A horizontal cell takes the list height and its own width; a vertical one the column width and its own height.
  private fun orient(cell: FlumeCellView) {
    cell.horizontal = geometry.horizontal
    cell.layoutParams = if (geometry.horizontal) {
      RecyclerView.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT)
    } else {
      RecyclerView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
    }
  }

  override fun onBindViewHolder(holder: Holder, position: Int) {
    if (!geometry.ready) return
    if (holder.cell.horizontal != geometry.horizontal) orient(holder.cell)
    holder.cell.rowsPressable = rowsPressable
    val start = SystemClock.elapsedRealtimeNanos()
    val batch = FlumeJni.bind(storeId, holder.cell.cellId, position, geometry.cellWidthDp(isFullSpan(position)), geometry.cellHeightDp()) ?: return
    holder.cell.apply(batch)
    SpikeStats.record(SystemClock.elapsedRealtimeNanos() - start, batch.stats)
  }
}
