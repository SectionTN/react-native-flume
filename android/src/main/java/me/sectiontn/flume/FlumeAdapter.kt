package me.sectiontn.flume

import android.content.Context
import android.os.SystemClock
import android.view.ViewGroup
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
  private val storeId get() = attached.storeId
  private var count = 0
  private var nextCellId = 1

  // A new size or font scale drops every measured text and rebinds the visible rows at the new size.
  fun onViewportChanged(widthPx: Int, heightPx: Int, fontScale: Float) {
    if (geometry.update(widthPx, heightPx, fontScale)) rebindAll()
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
    setCount(count)
    ListOpsApplier.apply(reset, removed, inserted, moved, changed, this)
    onDataChanged?.invoke(reset)
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
    val start = SystemClock.elapsedRealtimeNanos()
    val batch = FlumeJni.bind(storeId, holder.cell.cellId, position, geometry.cellWidthDp(isFullSpan(position)), geometry.cellHeightDp()) ?: return
    holder.cell.apply(batch)
    SpikeStats.record(SystemClock.elapsedRealtimeNanos() - start, batch.stats)
  }
}
