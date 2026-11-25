package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

class ScrollCommandsTest {
  private val rest = ScrollCommands.Rest(moving = false, dragging = false, jumpWaiting = false, realignPending = false)

  @Test
  fun endsTheCommandInFlightOnce() {
    val commands = ScrollCommands()
    commands.begin(3)
    assertEquals(3, commands.pending)
    assertEquals(3, commands.end())
    assertEquals(0, commands.pending)
    assertEquals(0, commands.end())
  }

  @Test
  fun reportsNothingWithNoCommand() {
    val commands = ScrollCommands()
    assertEquals(0, commands.end())
    assertEquals(0, commands.settle(rest))
  }

  @Test
  fun aNewerCommandIsTheOneReported() {
    val commands = ScrollCommands()
    commands.begin(3)
    commands.begin(4)
    assertEquals(4, commands.end())
  }

  @Test
  fun resetForgetsTheCommand() {
    val commands = ScrollCommands()
    commands.begin(3)
    commands.land()
    commands.reset()
    assertEquals(0, commands.end())
    assertEquals(0, commands.settle(rest))
  }

  @Test
  fun anEndAfterResetAndANewCommandReportsTheNewOne() {
    val commands = ScrollCommands()
    commands.begin(3)
    commands.reset()
    commands.begin(1)
    assertEquals(1, commands.end())
  }

  @Test
  fun settlesOnlyAtRest() {
    val commands = ScrollCommands()
    commands.begin(3)
    commands.land()
    assertEquals(0, commands.settle(rest.copy(moving = true)))
    assertEquals(0, commands.settle(rest.copy(dragging = true)))
    assertEquals(0, commands.settle(rest.copy(jumpWaiting = true)))
    assertEquals(0, commands.settle(rest.copy(realignPending = true)))
    assertEquals(3, commands.settle(rest))
    assertEquals(0, commands.settle(rest))
  }

  @Test
  fun doesNotSettleBeforeItLands() {
    val commands = ScrollCommands()
    commands.begin(3)
    assertEquals(0, commands.settle(rest))
    commands.land()
    assertEquals(3, commands.settle(rest))
  }

  @Test
  fun aNewerCommandClearsTheLandedMark() {
    val commands = ScrollCommands()
    commands.begin(3)
    commands.land()
    commands.begin(4)
    assertEquals(0, commands.settle(rest))
  }
}
