package me.sectiontn.flume

// Decides whether an attach reloads the rows: the engine drops list operations while nothing listens,
// so any attach after a detach starts from the current order, as does a new store.
class AttachTracker {
  var storeId = 0
    private set
  private var detached = false

  fun onAttach(storeId: Int): Boolean {
    val reload = storeId != this.storeId || detached
    this.storeId = storeId
    detached = false
    return reload
  }

  fun onDetach() {
    detached = true
  }
}
