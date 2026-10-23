package me.sectiontn.flume

object SlotKinds {
  const val ROW = 0
  const val HEADER = 1
  const val FOOTER = 2
  const val EMPTY = 3
  const val STICKY = 4
}

// The holders a slot may move between: one view type, in the list or pinned.
data class SlotGroup(val viewType: Int, val pinned: Boolean = false)

data class SlotEntry(
  val slot: Int,
  val kind: Int,
  val key: String,
  val index: Int,
  val bound: Boolean,
  val minMainDp: Float = 0f,
)

// Which slot each holder shows: a slot belongs to one key for life, and a released one waits for its key to come back.
// A pre-rendered slot (a negative id from JS) waits the same way, in no group until a bind for its key adopts it.
class SlotPool(private val maxFree: Int = MAX_FREE) {
  private class Slot(val id: Int, var group: SlotGroup?, val kind: Int, val key: String) {
    var index = -1
    var owner = 0
    var minMainDp = 0f
  }

  private val slots = HashMap<Int, Slot>()
  private val free = LinkedHashSet<Int>()
  private val unadopted = LinkedHashSet<Int>()
  private var nextId = 1
  private var batchOpen = false

  var dirty = false
    private set

  // The lowest pre-rendered id registered; JS drops a pending slot once this reaches it.
  var jsSeen = 0
    private set

  // Every pre-rendered slot of one mount batch waits for the binds its data brings; older ones past the cap leave as the
  // next batch registers.
  fun register(slot: Int, kind: Int, key: String) {
    if (slots.containsKey(slot)) return
    if (!batchOpen) {
      batchOpen = true
      trim(unadopted)
    }
    slots[slot] = Slot(slot, null, kind, key)
    jsSeen = minOf(jsSeen, slot)
    unadopted.add(slot)
    dirty = true
  }

  fun endBatch() {
    batchOpen = false
  }

  fun acquire(owner: Int, group: SlotGroup, kind: Int, key: String, index: Int): Int {
    val slot = slots.values.firstOrNull { it.owner == owner && it.matches(group, kind, key) }
      ?: free.firstNotNullOfOrNull { id -> slots[id]?.takeIf { it.matches(group, kind, key) } }
      ?: unadopted.firstNotNullOfOrNull { id -> slots[id]?.takeIf { it.matches(null, kind, key) } }
      ?: Slot(nextId++, group, kind, key).also { slots[it.id] = it }
    if (slot.owner != owner) {
      free.remove(slot.id)
      unadopted.remove(slot.id)
      slot.group = group
      slot.owner = owner
      dirty = true
    }
    setIndex(slot.id, index)
    return slot.id
  }

  // The owner's waiting slot leaves the pool and the pre-rendered one for the same key takes its place.
  fun replace(owner: Int, waiting: Int, prerendered: Int): Boolean {
    val old = slots[waiting]?.takeIf { it.owner == owner } ?: return false
    val next = slots[prerendered]?.takeIf { it.matches(null, old.kind, old.key) } ?: return false
    slots.remove(waiting)
    unadopted.remove(prerendered)
    next.group = old.group
    next.owner = owner
    next.index = old.index
    next.minMainDp = old.minMainDp
    dirty = true
    return true
  }

  fun release(owner: Int, slot: Int) {
    val entry = slots[slot] ?: return
    if (owner == 0 || entry.owner != owner) return
    entry.owner = 0
    cache(free, slot)
  }

  fun setIndex(slot: Int, index: Int) {
    val entry = slots[slot] ?: return
    if (entry.index == index) return
    entry.index = index
    dirty = true
  }

  fun setMinMain(slot: Int, dp: Float) {
    val entry = slots[slot] ?: return
    if (entry.minMainDp == dp) return
    entry.minMainDp = dp
    dirty = true
  }

  fun ownerOf(slot: Int): Int? = slots[slot]?.owner?.takeIf { it != 0 }

  fun entry(slot: Int): SlotEntry? = slots[slot]?.toEntry()

  fun snapshot(): List<SlotEntry> {
    dirty = false
    return slots.keys.sorted().map { slots.getValue(it).toEntry() }
  }

  // Released and pre-rendered slots wait in separate queues, so a release never evicts a slot a bind is about to adopt.
  private fun cache(queue: LinkedHashSet<Int>, slot: Int) {
    queue.add(slot)
    dirty = true
    trim(queue)
  }

  private fun trim(queue: LinkedHashSet<Int>) {
    while (queue.size > maxFree) {
      val oldest = queue.first()
      queue.remove(oldest)
      slots.remove(oldest)
      dirty = true
    }
  }

  private fun Slot.matches(group: SlotGroup?, kind: Int, key: String) =
    this.group == group && this.kind == kind && this.key == key

  private fun Slot.toEntry() = SlotEntry(id, kind, key, index, bound = owner != 0, minMainDp = minMainDp)

  companion object {
    const val MAX_FREE = 20
  }
}
