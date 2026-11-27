package me.sectiontn.flume

// The scroll command in flight: its id is reported once, when it settles at rest or is interrupted.
class ScrollCommands {
  data class Rest(val moving: Boolean, val dragging: Boolean, val jumpWaiting: Boolean, val realignPending: Boolean)

  var pending = 0
    private set
  private var landed = false

  fun begin(id: Int) {
    pending = id
    landed = false
  }

  // The command's own movement has finished, so a list at rest ends it.
  fun land() {
    landed = pending != 0
  }

  // Ends the command in flight and returns its id, or 0 when there is none.
  fun end(): Int {
    val id = pending
    reset()
    return id
  }

  // Ends the command once it has landed and the list rests, else returns 0.
  fun settle(rest: Rest): Int {
    if (!landed || rest.moving || rest.dragging || rest.jumpWaiting || rest.realignPending) return 0
    return end()
  }

  fun reset() {
    pending = 0
    landed = false
  }
}

// The viewPosition that brings a row into view: 0 before the fully visible rows, 1 after them, null when it is one.
object IntoView {
  fun target(position: Int, firstFull: Int, lastFull: Int): Float? = when {
    firstFull < 0 || lastFull < firstFull -> 0f
    position < firstFull -> 0f
    position > lastFull -> 1f
    else -> null
  }
}
