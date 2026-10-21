package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

class ExtraLayoutSpaceTest {
  @Test fun theDefaultIsOneViewport() = assertEquals(1800, ExtraLayoutSpace.of(-1, 1800))
  @Test fun aDistanceIsUsedAsIs() = assertEquals(600, ExtraLayoutSpace.of(600, 1800))
  @Test fun zeroLaysOutNothingExtra() = assertEquals(0, ExtraLayoutSpace.of(0, 1800))
}
