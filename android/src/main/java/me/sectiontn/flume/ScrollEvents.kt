package me.sectiontn.flume

import androidx.recyclerview.widget.RecyclerView
import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.WritableMap
import com.facebook.react.uimanager.events.Event
import kotlin.math.max
import kotlin.math.roundToInt

enum class ScrollKind(val eventName: String) {
  SCROLL("topScroll"),
  BEGIN_DRAG("topScrollBeginDrag"),
  END_DRAG("topScrollEndDrag"),
  MOMENTUM_BEGIN("topMomentumScrollBegin"),
  MOMENTUM_END("topMomentumScrollEnd"),
}

// The ScrollView events a RecyclerView state change stands for; a caught fling ends its momentum before the drag.
object ScrollStateMachine {
  fun transition(from: Int, to: Int): List<ScrollKind> {
    if (from == to) return emptyList()
    val out = mutableListOf<ScrollKind>()
    if (from == RecyclerView.SCROLL_STATE_DRAGGING) out += ScrollKind.END_DRAG
    if (from == RecyclerView.SCROLL_STATE_SETTLING) out += ScrollKind.MOMENTUM_END
    if (to == RecyclerView.SCROLL_STATE_DRAGGING) out += ScrollKind.BEGIN_DRAG
    if (to == RecyclerView.SCROLL_STATE_SETTLING) out += ScrollKind.MOMENTUM_BEGIN
    return out
  }
}

// At most one scroll event per interval; a suppressed one is flushed when the scroll settles.
class ScrollThrottle(private val intervalMs: Long) {
  private var lastMs = Long.MIN_VALUE
  private var suppressed = false

  fun shouldEmit(nowMs: Long): Boolean {
    if (lastMs == Long.MIN_VALUE || nowMs - lastMs >= intervalMs) {
      lastMs = nowMs
      suppressed = false
      return true
    }
    suppressed = true
    return false
  }

  fun flush(): Boolean {
    val had = suppressed
    suppressed = false
    return had
  }
}

// RecyclerView measures from the visual top; an inverted list reports its offset from the content start instead.
object ScrollMetrics {
  fun fromContentStart(offsetPx: Int, rangePx: Int, extentPx: Int, inverted: Boolean): Int =
    if (inverted) max(0, rangePx - extentPx - offsetPx) else offsetPx

  // ScrollView's content size holds its padding, so the last offset plus the view's length reaches it.
  fun contentSizePx(horizontal: Boolean, rangePx: Int, paddingPx: IntArray, widthPx: Int, heightPx: Int): Pair<Int, Int> =
    if (horizontal) {
      Pair(rangePx + paddingPx[0] + paddingPx[2], heightPx)
    } else {
      Pair(widthPx, rangePx + paddingPx[1] + paddingPx[3])
    }
}

// ScrollView's velocities in px per ms: a scroll from the offset change, a drag's end from the finger, momentum from that release.
class ScrollVelocity {
  private var lastOffsetPx = 0
  private var lastMs = Long.MIN_VALUE
  private var scrollPxPerMs = 0f
  private var releaseX = 0f
  private var releaseY = 0f

  fun scrolled(offsetPx: Int, nowMs: Long) {
    if (lastMs != Long.MIN_VALUE && nowMs != lastMs) scrollPxPerMs = (offsetPx - lastOffsetPx).toFloat() / (nowMs - lastMs)
    lastOffsetPx = offsetPx
    lastMs = nowMs
  }

  fun touchStarted() {
    releaseX = 0f
    releaseY = 0f
  }

  // An inverted list flips the finger on its axis, so the release keeps pointing against the reported offset.
  fun released(xPxPerMs: Float, yPxPerMs: Float, horizontal: Boolean, inverted: Boolean) {
    releaseX = if (inverted && horizontal) -xPxPerMs else xPxPerMs
    releaseY = if (inverted && !horizontal) -yPxPerMs else yPxPerMs
  }

  fun of(kind: ScrollKind, horizontal: Boolean, afterDrag: Boolean): Pair<Float, Float> = when (kind) {
    ScrollKind.SCROLL -> if (horizontal) Pair(scrollPxPerMs, 0f) else Pair(0f, scrollPxPerMs)
    ScrollKind.END_DRAG -> Pair(releaseX, releaseY)
    // ScrollView rounds the release to whole px before it starts the momentum.
    ScrollKind.MOMENTUM_BEGIN ->
      if (afterDrag) Pair(releaseX.roundToInt().toFloat(), releaseY.roundToInt().toFloat()) else Pair(0f, 0f)
    ScrollKind.BEGIN_DRAG, ScrollKind.MOMENTUM_END -> Pair(0f, 0f)
  }
}

// The ScrollView payload in dp, so a Reanimated scroll handler reads it unchanged.
class FlumeScrollEvent(
  surfaceId: Int,
  viewTag: Int,
  private val kind: ScrollKind,
  private val horizontal: Boolean,
  private val offsetDp: Float,
  private val contentWidthDp: Float,
  private val contentHeightDp: Float,
  private val widthDp: Float,
  private val heightDp: Float,
  private val velocityXDp: Float = 0f,
  private val velocityYDp: Float = 0f,
) : Event<FlumeScrollEvent>(surfaceId, viewTag) {
  override fun getEventName(): String = kind.eventName

  override fun canCoalesce(): Boolean = kind == ScrollKind.SCROLL

  override fun getEventData(): WritableMap = Arguments.createMap().apply {
    putMap("contentInset", sides(0f, 0f, 0f, 0f))
    putMap("contentOffset", point(if (horizontal) offsetDp else 0f, if (horizontal) 0f else offsetDp))
    putMap("contentSize", size(contentWidthDp, contentHeightDp))
    putMap("layoutMeasurement", size(widthDp, heightDp))
    putMap("velocity", point(velocityXDp, velocityYDp))
    putInt("target", viewTag)
    putDouble("timestamp", timestampMs.toDouble())
  }

  private fun point(x: Float, y: Float): WritableMap = Arguments.createMap().apply {
    putDouble("x", x.toDouble())
    putDouble("y", y.toDouble())
  }

  private fun size(width: Float, height: Float): WritableMap = Arguments.createMap().apply {
    putDouble("width", width.toDouble())
    putDouble("height", height.toDouble())
  }

  private fun sides(top: Float, left: Float, bottom: Float, right: Float): WritableMap = Arguments.createMap().apply {
    putDouble("top", top.toDouble())
    putDouble("left", left.toDouble())
    putDouble("bottom", bottom.toDouble())
    putDouble("right", right.toDouble())
  }
}
