package me.sectiontn.flume

// Fabric's child order for a host that parks its children elsewhere; Fabric removes a child by its index here.
class FabricChildren<T>(private val slotOf: (T) -> Int) {
  private val children = ArrayList<T>()
  private val bySlot = HashMap<Int, T>()

  val count: Int get() = children.size

  fun add(index: Int, child: T) {
    children.add(index, child)
    bySlot[slotOf(child)] = child
  }

  fun removeAt(index: Int): T {
    val child = children.removeAt(index)
    val slot = slotOf(child)
    if (bySlot[slot] === child) bySlot.remove(slot)
    return child
  }

  fun get(index: Int): T? = children.getOrNull(index)

  fun bySlot(slot: Int): T? = bySlot[slot]
}
