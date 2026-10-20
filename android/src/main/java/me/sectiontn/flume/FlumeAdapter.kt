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
  private var storeId = 0
  private var count = 0
  private var nextCellId = 1

  init {
    setHasStableIds(true)
  }

  fun attach(storeId: Int) {
    if (this.storeId == storeId) return
    if (this.storeId != 0) detach()
    this.storeId = storeId
    count = FlumeJni.attach(storeId, this, measurer, density)
    notifyDataSetChanged()
  }

  fun detach() {
    if (storeId == 0) return
    FlumeJni.detach(storeId)
    storeId = 0
    count = 0
    notifyDataSetChanged()
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
    return Holder(cell)
  }

  override fun onBindViewHolder(holder: Holder, position: Int) {
    val start = SystemClock.elapsedRealtimeNanos()
    val batch = FlumeJni.bind(storeId, holder.cell.cellId, position, recycler.width / density) ?: return
    holder.cell.apply(batch)
    SpikeStats.record(SystemClock.elapsedRealtimeNanos() - start, batch.stats)
  }
}
