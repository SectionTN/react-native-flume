package me.sectiontn.flume

import org.junit.Assert.assertArrayEquals
import org.junit.Test

class RefreshOffsetsTest {
  @Test fun followsReactNativeAtTheDefaultOffset() = assertArrayEquals(intArrayOf(-80, 48), RefreshOffsets.of(0.0, 2f, 80))

  @Test fun shiftsBothEndsByTheOffset() = assertArrayEquals(intArrayOf(-60, 68), RefreshOffsets.of(10.0, 2f, 80))
}
