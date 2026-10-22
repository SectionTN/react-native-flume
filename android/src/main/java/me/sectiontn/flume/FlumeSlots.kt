package me.sectiontn.flume

import android.content.Context
import android.view.View
import android.view.ViewGroup
import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.WritableMap
import com.facebook.react.uimanager.PointerEvents
import com.facebook.react.uimanager.ReactPointerEventsView
import com.facebook.react.uimanager.UIManagerHelper
import com.facebook.react.uimanager.events.Event
import kotlin.math.roundToInt

// The bay for slot views no holder shows: zero size, invisible and out of touch targeting, so they stay mounted.
class SlotParking(context: Context) : ViewGroup(context), ReactPointerEventsView {
  override val pointerEvents: PointerEvents = PointerEvents.NONE

  init {
    visibility = INVISIBLE
    isEnabled = false
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) = setMeasuredDimension(0, 0)

  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) = Unit
}

// The host's Fabric children and the holders that show them; JS renders the slots the pool lists.
class FlumeSlots(private val host: FlumeView) {
  private companion object {
    const val ESTIMATE_FLOOR_DP = 50f
  }

  val parking = SlotParking(host.context)
  val pool = SlotPool()
  val estimator = SizeEstimator((ESTIMATE_FLOOR_DP * host.resources.displayMetrics.density).roundToInt())
  private val children = FabricChildren<View> { (it as? FlumeSlotView)?.slot ?: 0 }
  private val holders = HashMap<Int, FlumeSlotHolder>()
  private val recorded = HashMap<Int, Int>()
  var cellWidthDp = -1f
  private var sentCellWidthDp = Float.NaN
  private var flushPosted = false

  val count: Int get() = children.count

  fun childAt(index: Int): View? = children.get(index)

  fun add(child: View, index: Int) {
    children.add(index, child)
    (child.parent as? ViewGroup)?.removeView(child)
    val view = child as? FlumeSlotView
    if (view == null) {
      parking.addView(child)
      return
    }
    view.onMeasured = { measured ->
      holderShowing(measured)?.let { holder ->
        recordLength(holder)
        holder.refresh()
      }
    }
    if (view.slot < 0) register(view)
    val holder = holders[view.slot]
    if (holder != null) holder.show(view) else parking.addView(view)
  }

  fun removeAt(index: Int) {
    val child = children.removeAt(index)
    (child as? FlumeSlotView)?.let { view ->
      view.onMeasured = null
      recorded.remove(view.slot)
      holderShowing(view)?.show(null)
    }
    (child.parent as? ViewGroup)?.removeView(child)
  }

  fun bind(holder: FlumeSlotHolder, group: SlotGroup, kind: Int, key: String, index: Int) {
    if (holder.slot != 0 && pool.entry(holder.slot)?.key != key) release(holder)
    val slot = pool.acquire(holder.owner, group, kind, key, index)
    holder.slot = slot
    holder.group = group
    holder.estimatePx = estimator.estimatePx(group)
    holders[slot] = holder
    holder.show(children.bySlot(slot) as? FlumeSlotView)
    recordLength(holder)
    flush()
  }

  fun recycle(holder: FlumeSlotHolder) {
    release(holder)
    holder.show(null)
    flush()
  }

  // A slot its owner keeps with no holder bound to it, such as a sticky row's copy waiting to pin.
  fun retain(owner: Int, group: SlotGroup, kind: Int, key: String, index: Int): Int {
    val slot = pool.acquire(owner, group, kind, key, index)
    flush()
    return slot
  }

  fun drop(owner: Int, slot: Int) {
    holders[slot]?.let { show(it, 0) }
    pool.release(owner, slot)
    flush()
  }

  // Points a holder at a slot its owner retained, or at none, without releasing the slot it showed before.
  fun show(holder: FlumeSlotHolder, slot: Int) {
    if (holder.slot == slot) return
    if (holder.slot != 0 && holders[holder.slot] === holder) holders.remove(holder.slot)
    holder.slot = slot
    if (slot != 0) holders[slot] = holder
    holder.show(if (slot == 0) null else children.bySlot(slot) as? FlumeSlotView)
  }

  // The empty row fills the viewport the header and footer leave; JS gives its slot that minimum length.
  fun setMinMain(holder: FlumeSlotHolder, dp: Float) {
    if (holder.slot == 0) return
    pool.setMinMain(holder.slot, dp)
    flush()
  }

  fun reindex(holder: FlumeSlotHolder, index: Int) {
    if (holder.slot == 0 || index < 0) return
    pool.setIndex(holder.slot, index)
    flush()
  }

  // One snapshot per main-loop turn; Fabric coalesces queued events per type, so each carries the whole table.
  fun flush() {
    if (flushPosted) return
    flushPosted = true
    host.post {
      flushPosted = false
      if (!pool.dirty && cellWidthDp == sentCellWidthDp) return@post
      sentCellWidthDp = cellWidthDp
      host.dispatch(FlumeSlotsEvent(UIManagerHelper.getSurfaceId(host), host.id, pool.snapshot(), pool.jsSeen, cellWidthDp))
    }
  }

  private fun release(holder: FlumeSlotHolder) {
    val slot = holder.slot
    if (slot == 0) return
    pool.release(holder.owner, slot)
    if (holders[slot] === holder) holders.remove(slot)
    holder.slot = 0
  }

  // A pre-rendered view takes the place of a slot its key still waits on, so the holder shows it without a blank frame.
  private fun register(view: FlumeSlotView) {
    pool.register(view.slot, view.kind, view.itemKey)
    host.refreshData()
    flush()
    val holder = holders.values.firstOrNull { holder ->
      val waiting = pool.entry(holder.slot)
      holder.slot > 0 && !holder.ready && waiting != null && waiting.kind == view.kind && waiting.key == view.itemKey
    } ?: return
    if (!pool.replace(holder.owner, holder.slot, view.slot)) return
    holders.remove(holder.slot)
    holder.slot = view.slot
    holders[view.slot] = holder
  }

  private fun holderShowing(view: FlumeSlotView): FlumeSlotHolder? = holders[view.slot]?.takeIf { it.view === view }

  // Each new length of a shown slot feeds its group's estimate once.
  private fun recordLength(holder: FlumeSlotHolder) {
    val view = holder.view?.takeIf { it.measured } ?: return
    val group = holder.group ?: return
    val length = view.lengthPx(holder.horizontal)
    if (recorded.put(view.slot, length) != length) estimator.record(group, length)
  }
}

class FlumeSlotsEvent(
  surfaceId: Int,
  viewTag: Int,
  private val entries: List<SlotEntry>,
  private val jsSeen: Int,
  private val cellWidthDp: Float,
) : Event<FlumeSlotsEvent>(surfaceId, viewTag) {
  override fun getEventName(): String = NAME

  override fun getEventData(): WritableMap = Arguments.createMap().apply {
    putArray(
      "slots",
      Arguments.createArray().apply {
        for (entry in entries) {
          pushMap(
            Arguments.createMap().apply {
              putInt("slot", entry.slot)
              putInt("kind", entry.kind)
              putString("key", entry.key)
              putInt("index", entry.index)
              putBoolean("bound", entry.bound)
              putDouble("minMain", entry.minMainDp.toDouble())
            },
          )
        }
      },
    )
    putInt("jsSeen", jsSeen)
    putDouble("cellWidth", cellWidthDp.toDouble())
  }

  companion object {
    const val NAME = "topSlotsChanged"
  }
}
