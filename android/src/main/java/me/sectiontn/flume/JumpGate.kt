package me.sectiontn.flume

// Holds a jump until every slot it waits for has rendered, or until its wait runs out; one jump waits at a time.
class JumpGate<J>(private val waitMs: Long = ScrollTargets.JUMP_WAIT_MS) {
  var pending: J? = null
    private set
  private val waiting = HashSet<Int>()
  private var waitsForData = false
  private var deadlineMs = 0L

  // Returns the jump this one replaced, whose slots the caller releases. A jump that waits for data runs on no render.
  fun request(jump: J, slots: Set<Int>, nowMs: Long, waitsForData: Boolean = false): J? {
    val replaced = take()
    pending = jump
    waiting.addAll(slots)
    this.waitsForData = waitsForData
    deadlineMs = nowMs + waitMs
    return replaced
  }

  // Swaps the waiting jump for one aimed at new rows, keeping its deadline. Returns the old one, or null when none waits.
  fun retarget(jump: J, slots: Set<Int>, waitsForData: Boolean = false): J? {
    val replaced = pending ?: return null
    pending = jump
    waiting.clear()
    waiting.addAll(slots)
    this.waitsForData = waitsForData
    return replaced
  }

  fun rendered(slot: Int): J? {
    if (pending == null || !waiting.remove(slot) || waiting.isNotEmpty() || waitsForData) return null
    return take()
  }

  fun cancel(): J? = take()

  fun due(nowMs: Long): J? = if (pending != null && nowMs >= deadlineMs) take() else null

  private fun take(): J? {
    val jump = pending
    pending = null
    waiting.clear()
    waitsForData = false
    return jump
  }
}
