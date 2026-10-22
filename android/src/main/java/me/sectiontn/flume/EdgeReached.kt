package me.sectiontn.flume

import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.WritableMap
import com.facebook.react.uimanager.events.Event
import kotlin.math.max

// Distances in content order: an inverted list's content start is its visual bottom.
data class EdgeDistances(val fromStartPx: Int, val fromEndPx: Int)

object ListDistances {
  fun of(offsetPx: Int, rangePx: Int, extentPx: Int, inverted: Boolean): EdgeDistances {
    val fromTop = max(0, offsetPx)
    val fromBottom = max(0, rangePx - offsetPx - extentPx)
    return if (inverted) EdgeDistances(fromBottom, fromTop) else EdgeDistances(fromTop, fromBottom)
  }
}

// RecyclerView's offset, range and extent along the scroll axis, estimated from the laid-out rows.
data class ScrollRange(val offsetPx: Int, val rangePx: Int, val extentPx: Int)

data class EdgeHits(val start: Boolean, val end: Boolean, val distances: EdgeDistances)

// FlatList's rule per item count, since RecyclerView's range moves with the visible rows: once within the threshold, re-armed outside it.
class EdgeTracker {
  private var sentStart = 0
  private var sentEnd = 0

  fun update(range: ScrollRange, itemCount: Int, inverted: Boolean, visibleLengthPx: Int, startThreshold: Double, endThreshold: Double): EdgeHits {
    val distances = ListDistances.of(range.offsetPx, range.rangePx, range.extentPx, inverted)
    val withinStart = distances.fromStartPx <= startThreshold * visibleLengthPx
    val withinEnd = distances.fromEndPx <= endThreshold * visibleLengthPx
    val start = withinStart && itemCount != sentStart
    val end = withinEnd && itemCount != sentEnd
    sentStart = if (!withinStart) 0 else if (start) itemCount else sentStart
    sentEnd = if (!withinEnd) 0 else if (end) itemCount else sentEnd
    return EdgeHits(start, end, distances)
  }
}

class FlumeEdgeEvent(surfaceId: Int, viewTag: Int, private val end: Boolean, private val distanceDp: Float) :
  Event<FlumeEdgeEvent>(surfaceId, viewTag) {
  override fun getEventName(): String = if (end) END else START

  override fun getEventData(): WritableMap = Arguments.createMap().apply {
    putDouble(if (end) "distanceFromEnd" else "distanceFromStart", distanceDp.toDouble())
  }

  companion object {
    const val END = "topEndReached"
    const val START = "topStartReached"
  }
}
