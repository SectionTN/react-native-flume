package me.sectiontn.flume

import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.WritableMap
import com.facebook.react.uimanager.events.Event
import kotlin.math.max
import kotlin.math.min

data class ViewabilityConfig(
  val itemVisiblePercentThreshold: Double? = null,
  val viewAreaCoveragePercentThreshold: Double? = null,
  val minimumViewTime: Long = 0,
  val waitForInteraction: Boolean = false,
)

data class ViewableChange(val position: Int, val isViewable: Boolean)

class ViewabilityReport(val viewable: IntArray, val changed: List<ViewableChange>)

// FlatList's rule: an overlapping row counts when it is entirely visible or its visible share reaches the threshold.
class ViewabilityTracker(var config: ViewabilityConfig) {
  var interacted = false
  private var lastComputed = IntArray(0)
  private var lastReported = IntArray(0)

  fun compute(viewportLength: Int, children: List<ChildFrame>): IntArray {
    if (viewportLength <= 0 || (config.waitForInteraction && !interacted)) return IntArray(0)
    val areaThreshold = config.viewAreaCoveragePercentThreshold
    val areaMode = areaThreshold != null || config.itemVisiblePercentThreshold == null
    val threshold = areaThreshold ?: config.itemVisiblePercentThreshold ?: 0.0
    return children
      .filter { it.start < viewportLength && it.end > 0 && isViewable(it, viewportLength, areaMode, threshold) }
      .map { it.position }
      .sorted()
      .toIntArray()
  }

  private fun isViewable(frame: ChildFrame, viewportLength: Int, areaMode: Boolean, threshold: Double): Boolean {
    if (frame.start >= 0 && frame.end <= viewportLength && frame.end > frame.start) return true
    val pixels = max(0, min(frame.end, viewportLength) - max(frame.start, 0)).toDouble()
    val percent = 100.0 * (if (areaMode) pixels / viewportLength else pixels / (frame.end - frame.start))
    return percent >= threshold
  }

  // Against the last computed set, which this records; the report is delayed by minimumViewTime from here.
  fun changed(viewable: IntArray): Boolean {
    val changed = !viewable.contentEquals(lastComputed)
    lastComputed = viewable
    return changed
  }

  // Against the last reported set: exits first, then entries; null when nothing moved.
  fun report(viewable: IntArray): ViewabilityReport? {
    if (viewable.contentEquals(lastReported)) return null
    val previous = lastReported.toSet()
    val next = viewable.toSet()
    val changed = (previous - next).sorted().map { ViewableChange(it, false) } +
      (next - previous).sorted().map { ViewableChange(it, true) }
    lastReported = viewable
    return ViewabilityReport(viewable, changed)
  }

  fun reset() {
    lastComputed = IntArray(0)
    lastReported = IntArray(0)
  }
}

class FlumeViewabilityEvent(
  surfaceId: Int,
  viewTag: Int,
  private val viewable: List<Pair<Int, String>>,
  private val changed: List<Triple<Int, String, Boolean>>,
) : Event<FlumeViewabilityEvent>(surfaceId, viewTag) {
  override fun getEventName(): String = NAME

  override fun getEventData(): WritableMap = Arguments.createMap().apply {
    putArray("viewable", Arguments.createArray().apply { for ((index, key) in viewable) pushMap(token(index, key, true)) })
    putArray("changed", Arguments.createArray().apply { for ((index, key, isViewable) in changed) pushMap(token(index, key, isViewable)) })
  }

  private fun token(index: Int, key: String, isViewable: Boolean): WritableMap = Arguments.createMap().apply {
    putInt("index", index)
    putString("key", key)
    putBoolean("isViewable", isViewable)
  }

  companion object {
    const val NAME = "topViewableItemsChanged"
  }
}
