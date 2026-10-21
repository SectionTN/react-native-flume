package me.sectiontn.flume

import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.WritableMap
import com.facebook.react.uimanager.events.Event
import kotlin.math.roundToInt

// React Native's progress view offsets: the circle starts above the offset and settles 64 dp below it.
object RefreshOffsets {
  private const val CIRCLE_TARGET_DP = 64.0

  fun of(offsetDp: Double, density: Float, diameterPx: Int): IntArray = intArrayOf(
    (offsetDp * density).roundToInt() - diameterPx,
    ((offsetDp + CIRCLE_TARGET_DP) * density).roundToInt() - diameterPx,
  )
}

class FlumeRefreshEvent(surfaceId: Int, viewTag: Int) : Event<FlumeRefreshEvent>(surfaceId, viewTag) {
  override fun getEventName(): String = NAME

  override fun getEventData(): WritableMap = Arguments.createMap()

  companion object {
    const val NAME = "topRefresh"
  }
}
