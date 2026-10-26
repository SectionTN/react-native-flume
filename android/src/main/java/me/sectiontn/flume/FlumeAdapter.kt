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
  private val slots: FlumeSlots,
) : RecyclerView.Adapter<FlumeAdapter.Holder>(), FlumeStoreListener, ListNotifier {
  class Holder(val view: View) : RecyclerView.ViewHolder(view) {
    val cell: FlumeCellView? get() = view as? FlumeCellView
    val slot: FlumeSlotHolder? get() = view as? FlumeSlotHolder
  }

  private val density = context.resources.displayMetrics.density
  val geometry = ListGeometry(density)
  private val attached = AttachTracker()
  var onPress: ((position: Int, pressId: String?, long: Boolean) -> Unit)? = null
  var fullSpans = FullSpans(IntArray(0))
  var onFirstData: (() -> Unit)? = null
  var onDataChanged: ((reset: Boolean) -> Unit)? = null
  var rowsPressable = false
  var anchorConfig = AnchorConfig()
  var onAutoscroll: ((index: Int, viewPosition: Float, animated: Boolean) -> Unit)? = null
  private val storeId get() = attached.storeId
  var rows = ListRows()
    private set
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
    // React grid cells take their width from JS, which learns it from the slot snapshot.
    val grid = geometry.ready && !geometry.horizontal && geometry.numColumns > 1
    slots.cellWidthDp = if (grid) geometry.cellWidthDp(false) else -1f
    slots.flush()
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
    if (reload || attachedCount != rows.dataCount) {
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
    // The empty row comes and goes with the data, which the store's operations do not know about.
    val reload = reset || rows.hasEmptyRow != rows.copy(dataCount = count).hasEmptyRow
    val before = if (reload) null else anchorState()
    val anchorLost = !reload && AnchorPolicy.losesEveryRow(laidOutDataIndices(), removed)
    setCount(count)
    ListOpsApplier.apply(reload, removed, inserted, moved, changed, this)
    reindexSlots()
    anchor(before, inserted, count, anchorLost)
    onDataChanged?.invoke(reload)
  }

  // Rows RecyclerView could anchor on, before the operations apply; a row on its way out has no position.
  private fun laidOutDataIndices(): IntArray = (0 until recycler.childCount)
    .map { recycler.getChildAdapterPosition(recycler.getChildAt(it)) }
    .filter { it != RecyclerView.NO_POSITION }
    .map { rows.dataIndex(it) }
    .toIntArray()

  // Inserts and removes shift rows without a rebind; a React cell's index follows its holder's new position.
  private fun reindexSlots() {
    for (i in 0 until recycler.childCount) {
      val holder = recycler.getChildViewHolder(recycler.getChildAt(i)) as? Holder ?: continue
      holder.slot?.let { slots.reindex(it, rows.dataIndex(holder.bindingAdapterPosition)) }
    }
  }

  // Inserts touching the content start or end map to the visual edges through the orientation.
  private fun anchor(before: AnchorState?, inserted: IntArray, count: Int, anchorLost: Boolean) {
    var atStart = false
    var atEnd = false
    for (i in inserted.indices step 2) {
      if (inserted[i] == 0) atStart = true
      if (inserted[i] + inserted[i + 1] == count) atEnd = true
    }
    val inverted = geometry.inverted
    val decision = AnchorPolicy.decide(anchorConfig, before, if (inverted) atEnd else atStart, if (inverted) atStart else atEnd, anchorLost)
    decision.restore?.let { (position, offset) ->
      // A shorter list keeps its last row in view; RecyclerView ignores a position past the count.
      (recycler.layoutManager as? LinearLayoutManager)?.scrollToPositionWithOffset(minOf(position, rows.itemCount - 1), offset)
    }
    decision.scrollTo?.let { edge ->
      val (index, viewPosition) = AnchorTargets.indexFor(edge, rows.itemCount, inverted)
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
    for (position in (previous - next) + (next - previous)) if (position < rows.itemCount) notifyItemChanged(position)
  }

  private fun setCount(next: Int) {
    val first = rows.dataCount == 0 && next > 0
    rows = rows.copy(dataCount = next)
    if (first) onFirstData?.invoke()
  }

  override fun reset() = notifyDataSetChanged()

  override fun removed(index: Int, count: Int) = notifyItemRangeRemoved(index + rows.headerCount, count)

  override fun inserted(index: Int, count: Int) = notifyItemRangeInserted(index + rows.headerCount, count)

  override fun moved(from: Int, to: Int) = notifyItemMoved(from + rows.headerCount, to + rows.headerCount)

  override fun changed(index: Int) = notifyItemChanged(index + rows.headerCount)

  override fun getItemCount(): Int = rows.itemCount

  override fun getItemViewType(position: Int): Int = when (rows.kind(position)) {
    RowKind.HEADER -> ViewTypes.HEADER
    RowKind.EMPTY -> ViewTypes.EMPTY
    RowKind.FOOTER -> ViewTypes.FOOTER
    RowKind.DATA -> FlumeJni.viewType(storeId, rows.dataIndex(position))
  }

  // Item ids start at 1, so the list components take ids no item can have.
  override fun getItemId(position: Int): Long = when (rows.kind(position)) {
    RowKind.HEADER -> -2L
    RowKind.EMPTY -> -3L
    RowKind.FOOTER -> -4L
    RowKind.DATA -> FlumeJni.itemId(storeId, rows.dataIndex(position))
  }

  // The list components are adapter rows around the data; the empty row stands in for the data while there is none.
  fun setListComponents(header: Boolean, footer: Boolean, empty: Boolean) {
    val before = rows
    val after = before.copy(header = header, footer = footer, emptyComponent = empty)
    if (after == before) return
    rows = after
    if (before.header != after.header) if (after.header) notifyItemInserted(0) else notifyItemRemoved(0)
    if (before.hasEmptyRow != after.hasEmptyRow) {
      if (after.hasEmptyRow) notifyItemInserted(after.headerCount) else notifyItemRemoved(after.headerCount)
    }
    if (before.footer != after.footer) if (after.footer) notifyItemInserted(after.itemCount - 1) else notifyItemRemoved(after.itemCount)
  }

  override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
    if (ViewTypes.isReact(viewType)) {
      val slot = FlumeSlotHolder(context, slots.parking)
      slot.owner = nextCellId++
      orient(slot)
      return Holder(slot)
    }
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
  private fun orient(view: View) {
    (view as? FlumeCellView)?.horizontal = geometry.horizontal
    (view as? FlumeSlotHolder)?.horizontal = geometry.horizontal
    view.layoutParams = if (geometry.horizontal) {
      RecyclerView.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT)
    } else {
      RecyclerView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
    }
  }

  fun slotRequest(position: Int, viewType: Int): SlotRequest {
    val group = SlotGroup(viewType)
    return when (rows.kind(position)) {
      RowKind.HEADER -> SlotRequest(group, SlotKinds.HEADER, "header", -1)
      RowKind.EMPTY -> SlotRequest(group, SlotKinds.EMPTY, "empty", -1)
      RowKind.FOOTER -> SlotRequest(group, SlotKinds.FOOTER, "footer", -1)
      RowKind.DATA -> rows.dataIndex(position).let { SlotRequest(group, SlotKinds.ROW, FlumeJni.keyAt(storeId, it), it) }
    }
  }

  override fun onBindViewHolder(holder: Holder, position: Int) {
    if (!geometry.ready) return
    holder.slot?.let { slot ->
      if (slot.horizontal != geometry.horizontal) orient(slot)
      val request = slotRequest(position, holder.itemViewType)
      slots.bind(slot, request.group, request.kind, request.key, request.index)
      return
    }
    val cell = holder.cell ?: return
    val index = rows.dataIndex(position).takeIf { it >= 0 } ?: return
    if (cell.horizontal != geometry.horizontal) orient(cell)
    cell.rowsPressable = rowsPressable
    val start = if (BindStats.enabled) SystemClock.elapsedRealtimeNanos() else 0L
    val batch = FlumeJni.bind(storeId, cell.cellId, index, geometry.cellWidthDp(fullSpans.contains(position)), geometry.cellHeightDp()) ?: return
    cell.apply(batch)
    batch.stats?.let { BindStats.record(SystemClock.elapsedRealtimeNanos() - start, it) }
  }

  override fun onViewRecycled(holder: Holder) {
    holder.slot?.let { slots.recycle(it) }
  }

  // A cached holder comes back at a shifted position without a rebind.
  override fun onViewAttachedToWindow(holder: Holder) {
    holder.slot?.let { slots.reindex(it, rows.dataIndex(holder.bindingAdapterPosition)) }
  }

  // A focused input keeps a React holder out of the pool otherwise; its slot stays mounted in the bay.
  override fun onFailedToRecycleView(holder: Holder): Boolean = holder.slot != null || super.onFailedToRecycleView(holder)
}
