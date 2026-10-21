package me.sectiontn.flume

// FlashList's shape: thresholds are fractions of the viewport, measured at the visual edges.
data class AnchorConfig(
  val disabled: Boolean = false,
  val autoscrollToTopThreshold: Double? = null,
  val autoscrollToBottomThreshold: Double? = null,
  val animateAutoScrollToBottom: Boolean = true,
  val startRenderingFromBottom: Boolean = false,
)

// Captured before a data change; the offset is in the layout manager's own convention for scrollToPositionWithOffset.
data class AnchorState(
  val firstVisiblePosition: Int,
  val firstVisibleOffsetPx: Int,
  val distanceToVisualTopPx: Int,
  val distanceToVisualBottomPx: Int,
  val viewportPx: Int,
)

enum class VisualEdge { TOP, BOTTOM }

data class AnchorDecision(val restore: Pair<Int, Int>? = null, val scrollTo: VisualEdge? = null, val animated: Boolean = false)

// disabled keeps the first visible index where it was; otherwise an insert at a near edge scrolls to that edge.
object AnchorPolicy {
  fun decide(config: AnchorConfig, before: AnchorState?, insertedAtVisualTop: Boolean, insertedAtVisualBottom: Boolean): AnchorDecision {
    if (before == null) return AnchorDecision()
    if (config.disabled) return AnchorDecision(restore = before.firstVisiblePosition to before.firstVisibleOffsetPx)
    val bottom = config.autoscrollToBottomThreshold
    if (bottom != null && insertedAtVisualBottom && before.distanceToVisualBottomPx <= bottom * before.viewportPx) {
      return AnchorDecision(scrollTo = VisualEdge.BOTTOM, animated = config.animateAutoScrollToBottom)
    }
    val top = config.autoscrollToTopThreshold
    if (top != null && insertedAtVisualTop && before.distanceToVisualTopPx <= top * before.viewportPx) {
      return AnchorDecision(scrollTo = VisualEdge.TOP, animated = config.animateAutoScrollToBottom)
    }
    return AnchorDecision()
  }
}

// The index and viewPosition that put a visual edge in view: the content start is the visual bottom when inverted.
object AnchorTargets {
  fun indexFor(edge: VisualEdge, count: Int, inverted: Boolean): Pair<Int, Float> =
    if ((edge == VisualEdge.TOP) != inverted) 0 to 0f else count - 1 to 1f
}
