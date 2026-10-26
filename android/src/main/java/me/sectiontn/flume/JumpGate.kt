package me.sectiontn.flume

// Holds a jump until every slot it waits for has rendered, or until its wait runs out; one jump waits at a time.
class JumpGate<J>(private val waitMs: Long = ScrollTargets.JUMP_WAIT_MS) {
  var pending: J? = null
    private set
  private val waiting = HashSet<Int>()
  private var deadlineMs = 0L

  // Returns the jump this one replaced, whose slots the caller releases.
  fun request(jump: J, slots: Set<Int>, nowMs: Long): J? {
    val replaced = take()
    pending = jump
    waiting.addAll(slots)
    deadlineMs = nowMs + waitMs
    return replaced
  }

  fun rendered(slot: Int): J? {
    if (pending == null || !waiting.remove(slot) || waiting.isNotEmpty()) return null
    return take()
  }

  fun cancel(): J? = take()

  fun due(nowMs: Long): J? = if (pending != null && nowMs >= deadlineMs) take() else null

  private fun take(): J? {
    val jump = pending
    pending = null
    waiting.clear()
    return jump
  }
}
