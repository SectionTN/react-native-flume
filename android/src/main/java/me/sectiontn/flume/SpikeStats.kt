package me.sectiontn.flume

import android.util.Log
import java.util.Locale
import kotlin.math.ceil

// Spike instrumentation: per-bind stage times over the last 2,000 recycled binds, logged every 200.
object SpikeStats {
  private const val TAG = "FlumeSpike"
  private const val WINDOW = 2000
  private const val LOG_EVERY = 200

  private class Ring(private val capacity: Int) {
    private val values = LongArray(capacity)
    private var next = 0
    var size = 0
      private set

    fun add(value: Long) {
      values[next] = value
      next = (next + 1) % capacity
      if (size < capacity) size++
    }

    fun sorted(): LongArray = values.copyOf(size).also { it.sort() }

    fun sum(): Long {
      var total = 0L
      for (i in 0 until size) total += values[i]
      return total
    }

    fun clear() {
      next = 0
      size = 0
    }
  }

  private val total = Ring(WINDOW)
  private val worklet = Ring(WINDOW)
  private val reconcile = Ring(WINDOW)
  private val layout = Ring(WINDOW)
  private val apply = Ring(WINDOW)
  private val measureCalls = Ring(WINDOW)
  private val measureHits = Ring(WINDOW)
  private val first = Ring(WINDOW)
  private var recycled = 0

  fun record(totalNs: Long, stats: LongArray) {
    if (stats[5] == 1L) {
      first.add(totalNs)
      return
    }
    total.add(totalNs)
    worklet.add(stats[0])
    reconcile.add(stats[1])
    layout.add(stats[2])
    apply.add(totalNs - stats[0] - stats[1] - stats[2])
    measureCalls.add(stats[3])
    measureHits.add(stats[4])
    recycled++
    if (recycled % LOG_EVERY == 0) Log.i(TAG, summary())
  }

  fun summary(): String {
    val sortedTotal = total.sorted()
    val calls = measureCalls.sum()
    val hitPercent = if (calls == 0L) 0 else (measureHits.sum() * 100 / calls).toInt()
    val perBind = if (total.size == 0) 0.0 else calls.toDouble() / total.size
    val sortedFirst = first.sorted()
    return "FlumeSpike recycled n=${total.size} total p50=${ms(percentile(sortedTotal, 0.5))} " +
      "p95=${ms(percentile(sortedTotal, 0.95))} max=${ms(percentile(sortedTotal, 1.0))}" +
      " | worklet ${pair(worklet)} | reconcile ${pair(reconcile)} | layout ${pair(layout)} | apply ${pair(apply)}" +
      " | measure ${String.format(Locale.US, "%.1f", perBind)}/bind hits $hitPercent%" +
      " | first n=${first.size} p50=${ms(percentile(sortedFirst, 0.5))}"
  }

  // Nearest-rank percentile of an ascending array; 0 when empty.
  internal fun percentile(sorted: LongArray, p: Double): Long {
    if (sorted.isEmpty()) return 0
    val rank = ceil(p * sorted.size).toInt().coerceIn(1, sorted.size)
    return sorted[rank - 1]
  }

  internal fun reset() {
    listOf(total, worklet, reconcile, layout, apply, measureCalls, measureHits, first).forEach { it.clear() }
    recycled = 0
  }

  private fun pair(ring: Ring): String {
    val sorted = ring.sorted()
    return "${ms(percentile(sorted, 0.5))}/${ms(percentile(sorted, 0.95))}"
  }

  private fun ms(ns: Long): String = String.format(Locale.US, "%.2f", ns / 1_000_000.0)
}
