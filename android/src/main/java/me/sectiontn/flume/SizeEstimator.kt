package me.sectiontn.flume

// The length a React holder takes before its slot measures: estimatedItemSize, else its group's mean above the floor.
class SizeEstimator(private val floorPx: Int) {
  private class Mean {
    var sum = 0L
    var count = 0
  }

  var estimatedPx = -1
  private val means = HashMap<SlotGroup, Mean>()

  fun record(group: SlotGroup, lengthPx: Int) {
    val mean = means.getOrPut(group) { Mean() }
    mean.sum += lengthPx
    mean.count++
  }

  fun estimatePx(group: SlotGroup): Int {
    if (estimatedPx >= 0) return estimatedPx
    val mean = means[group]?.takeIf { it.count > 0 } ?: return floorPx
    return maxOf(floorPx, (mean.sum / mean.count).toInt())
  }
}
