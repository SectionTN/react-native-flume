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

data class ViewToken(val index: Int, val key: String)

data class ViewableChange(val token: ViewToken, val isViewable: Boolean)

class ViewabilityReport(val viewable: List<ViewToken>, val changed: List<ViewableChange>)

// FlatList's rule: an overlapping row counts when it is entirely visible or its visible share reaches the threshold.
class ViewabilityTracker(var config: ViewabilityConfig) {
  var interacted = false
  private var lastComputed = IntArray(0)
  private var dirty = false
  private var current = emptyList<ViewToken>()
  private var lastReported = emptyList<ViewToken>()

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
    val changed = dirty || !viewable.contentEquals(lastComputed)
    dirty = false
    lastComputed = viewable
    return changed
  }

  // Against the last reported set by key, so rows that only moved report nothing: exits first, then entries.
  fun report(viewable: List<ViewToken>): ViewabilityReport? {
    val previous = lastReported.mapTo(HashSet()) { it.key }
    val next = viewable.mapTo(HashSet()) { it.key }
    val changed = lastReported.filter { it.key !in next }.map { ViewableChange(it, false) } +
      viewable.filter { it.key !in previous }.map { ViewableChange(it, true) }
    lastReported = viewable
    return if (changed.isEmpty()) null else ViewabilityReport(viewable, changed)
  }

  // A data change moves keys between indices, so the next computed set counts as changed, as FlatList resets its cache.
  fun invalidate() {
    dirty = true
  }

  // The computed set with its keys when it changed, null when it did not; keys are read once per change.
  fun keyed(viewable: IntArray, keyAt: (Int) -> String): List<ViewToken>? {
    if (!changed(viewable)) return null
    current = viewable.map { ViewToken(it, keyAt(it)) }
    return current
  }

  // After minimumViewTime, only the scheduled rows still in the computed set count, at their indices now, as FlatList filters.
  fun reportDelayed(scheduled: List<ViewToken>): ViewabilityReport? {
    val keys = scheduled.mapTo(HashSet()) { it.key }
    return report(current.filter { it.key in keys })
  }
}

class FlumeViewabilityEvent(
  surfaceId: Int,
  viewTag: Int,
  private val viewable: List<ViewToken>,
  private val changed: List<ViewableChange>,
) : Event<FlumeViewabilityEvent>(surfaceId, viewTag) {
  override fun getEventName(): String = NAME

  override fun getEventData(): WritableMap = Arguments.createMap().apply {
    putArray("viewable", Arguments.createArray().apply { for (token in viewable) pushMap(token(token, true)) })
    putArray("changed", Arguments.createArray().apply { for (change in changed) pushMap(token(change.token, change.isViewable)) })
  }

  private fun token(token: ViewToken, isViewable: Boolean): WritableMap = Arguments.createMap().apply {
    putInt("index", token.index)
    putString("key", token.key)
    putBoolean("isViewable", isViewable)
  }

  companion object {
    const val NAME = "topViewableItemsChanged"
  }
}
