package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

class FlingFactorTest {
  @Test fun normalKeepsTheVelocity() = assertEquals(1f, FlingFactor.of(0.998), 0f)

  @Test fun fastHalvesIt() = assertEquals(0.5f, FlingFactor.of(0.99), 0f)

  @Test fun ratesInBetweenInterpolate() = assertEquals(0.75f, FlingFactor.of(0.994), 0.001f)

  @Test fun theFactorIsClamped() {
    assertEquals(0.25f, FlingFactor.of(0.9), 0f)
    assertEquals(1f, FlingFactor.of(1.0), 0f)
  }
}
