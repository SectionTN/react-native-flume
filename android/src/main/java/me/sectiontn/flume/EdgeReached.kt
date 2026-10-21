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

data class EdgeHits(val start: Boolean, val end: Boolean)

// FlatList's rule: an edge fires once per content length while within its threshold, and re-arms outside it.
class EdgeTracker {
  private var sentStart = -1
  private var sentEnd = -1

  fun update(distances: EdgeDistances, visibleLengthPx: Int, contentLengthPx: Int, startThreshold: Double, endThreshold: Double): EdgeHits {
    val withinStart = distances.fromStartPx <= startThreshold * visibleLengthPx
    val withinEnd = distances.fromEndPx <= endThreshold * visibleLengthPx
    val start = withinStart && contentLengthPx != sentStart
    val end = withinEnd && contentLengthPx != sentEnd
    sentStart = if (!withinStart) -1 else if (start) contentLengthPx else sentStart
    sentEnd = if (!withinEnd) -1 else if (end) contentLengthPx else sentEnd
    return EdgeHits(start, end)
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
