package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class JumpGateTest {
  @Test
  fun runsOnceTheLastSlotRenders() {
    val gate = JumpGate<String>()
    assertNull(gate.request("a", setOf(1, 2), nowMs = 0))
    assertNull(gate.rendered(1))
    assertEquals("a", gate.rendered(2))
    assertNull(gate.pending)
  }

  @Test
  fun aNewerJumpReplacesTheWaitingOne() {
    val gate = JumpGate<String>()
    gate.request("a", setOf(1), nowMs = 0)
    assertEquals("a", gate.request("b", setOf(2), nowMs = 10))
    assertNull(gate.rendered(1))
    assertEquals("b", gate.rendered(2))
  }

  @Test
  fun aDragCancelsTheWaitingJump() {
    val gate = JumpGate<String>()
    gate.request("a", setOf(1), nowMs = 0)
    assertEquals("a", gate.cancel())
    assertNull(gate.rendered(1))
  }

  @Test
  fun itRunsAnywayAfterTheWait() {
    val gate = JumpGate<String>(waitMs = 1000)
    gate.request("a", setOf(1), nowMs = 0)
    assertNull(gate.due(nowMs = 999))
    assertEquals("a", gate.due(nowMs = 1000))
    assertNull(gate.pending)
  }

  @Test
  fun aRenderedSlotOfNoJumpIsIgnored() {
    val gate = JumpGate<String>()
    assertNull(gate.rendered(5))
    gate.request("a", setOf(1), nowMs = 0)
    assertNull(gate.rendered(5))
    assertEquals("a", gate.pending)
  }
}
