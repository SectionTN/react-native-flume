package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SpikeStatsTest {
  @Test
  fun percentileIsNearestRank() {
    val sorted = LongArray(100) { (it + 1).toLong() }
    assertEquals(50L, SpikeStats.percentile(sorted, 0.5))
    assertEquals(95L, SpikeStats.percentile(sorted, 0.95))
    assertEquals(100L, SpikeStats.percentile(sorted, 1.0))
  }

  @Test
  fun summaryReportsStagesAndFirstBinds() {
    SpikeStats.reset()
    repeat(200) { i ->
      SpikeStats.record(1_000_000L + i * 1_000L, longArrayOf(400_000L, 20_000L, 300_000L, 2L, 1L, 0L))
    }
    SpikeStats.record(5_000_000L, longArrayOf(2_000_000L, 100_000L, 1_000_000L, 2L, 0L, 1L))
    val s = SpikeStats.summary()
    assertTrue(s, s.startsWith("FlumeSpike recycled n=200 total p50=1.10"))
    assertTrue(s, s.contains("worklet 0.40/0.40"))
    assertTrue(s, s.contains("measure 2.0/bind hits 50%"))
    assertTrue(s, s.endsWith("first n=1 p50=5.00"))
  }
}
