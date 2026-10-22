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
class SlotPool(private val maxFree: Int = MAX_FREE) {
  private class Slot(val id: Int, val group: SlotGroup, val kind: Int, val key: String) {
    var index = -1
    var owner = 0
  }

  private val slots = HashMap<Int, Slot>()
  private val free = LinkedHashSet<Int>()
  private var nextId = 1

  var dirty = false
    private set

  fun acquire(owner: Int, group: SlotGroup, kind: Int, key: String, index: Int): Int {
    val slot = slots.values.firstOrNull { it.owner == owner && it.matches(group, kind, key) }
      ?: free.firstNotNullOfOrNull { id -> slots[id]?.takeIf { it.matches(group, kind, key) } }
      ?: Slot(nextId++, group, kind, key).also { slots[it.id] = it }
    if (slot.owner != owner) {
      free.remove(slot.id)
      slot.owner = owner
      dirty = true
    }
    setIndex(slot.id, index)
    return slot.id
  }

  fun release(owner: Int, slot: Int) {
    val entry = slots[slot] ?: return
    if (owner == 0 || entry.owner != owner) return
    entry.owner = 0
    free.add(slot)
    dirty = true
    while (free.size > maxFree) {
      val oldest = free.first()
      free.remove(oldest)
      slots.remove(oldest)
    }
  }

  fun setIndex(slot: Int, index: Int) {
    val entry = slots[slot] ?: return
    if (entry.index == index) return
    entry.index = index
    dirty = true
  }

  fun ownerOf(slot: Int): Int? = slots[slot]?.owner?.takeIf { it != 0 }

  fun entry(slot: Int): SlotEntry? = slots[slot]?.toEntry()

  fun snapshot(): List<SlotEntry> {
    dirty = false
    return slots.keys.sorted().map { slots.getValue(it).toEntry() }
  }

  private fun Slot.matches(group: SlotGroup, kind: Int, key: String) =
    this.group == group && this.kind == kind && this.key == key

  private fun Slot.toEntry() = SlotEntry(id, kind, key, index, bound = owner != 0)

  companion object {
    const val MAX_FREE = 20
  }
}
