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
  private val viewport = ViewportTracker()
  var onPress: ((position: Int, pressId: String?, long: Boolean) -> Unit)? = null
  private var storeId = 0
  private var count = 0
  private var nextCellId = 1

  // A new width or font scale drops every measured text and rebinds the visible rows at the new size.
  fun onViewportChanged(widthPx: Int, fontScale: Float) {
    if (!viewport.update(widthPx, fontScale)) return
    measurer.invalidate()
    if (storeId != 0) FlumeJni.invalidateMeasures(storeId)
    notifyDataSetChanged()
  }

  init {
    setHasStableIds(true)
  }

  // Re-attaching the same store keeps the holders and their trees; the engine only re-registers the listener.
  fun attach(storeId: Int) {
    val changed = this.storeId != storeId
    if (changed && this.storeId != 0) detach()
    this.storeId = storeId
    val attached = FlumeJni.attach(storeId, this, measurer, density)
    if (changed || attached != count) {
      count = attached
      notifyDataSetChanged()
    }
  }

  fun detach() {
    if (storeId != 0) FlumeJni.detach(storeId)
  }

  override fun onListChanged(reset: Boolean, removed: IntArray, inserted: IntArray, moved: IntArray, changed: IntArray, count: Int) {
    this.count = count
    ListOpsApplier.apply(reset, removed, inserted, moved, changed, this)
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
    cell.layoutParams = RecyclerView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
    val holder = Holder(cell)
    // The position is read at click time, so a recycled holder reports the item it shows now.
    cell.onPress = { pressId, long ->
      val position = holder.bindingAdapterPosition
      if (position != RecyclerView.NO_POSITION) onPress?.invoke(position, pressId, long)
    }
    return holder
  }

  override fun onBindViewHolder(holder: Holder, position: Int) {
    val widthPx = viewport.widthPx
    if (widthPx == 0) return
    val start = SystemClock.elapsedRealtimeNanos()
    val batch = FlumeJni.bind(storeId, holder.cell.cellId, position, widthPx / density) ?: return
    holder.cell.apply(batch)
    SpikeStats.record(SystemClock.elapsedRealtimeNanos() - start, batch.stats)
  }
}
