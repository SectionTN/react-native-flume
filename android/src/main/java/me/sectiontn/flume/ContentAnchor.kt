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

// disabled keeps the first visible index where it was; otherwise an insert at a near edge scrolls to that edge. RecyclerView
// starts over from the first row when no laid-out row survives a change; FlatList and FlashList keep the offset, so that
// change keeps the first visible index too.
object AnchorPolicy {
  fun decide(
    config: AnchorConfig,
    before: AnchorState?,
    insertedAtVisualTop: Boolean,
    insertedAtVisualBottom: Boolean,
    anchorLost: Boolean = false,
  ): AnchorDecision {
    if (before == null) return AnchorDecision()
    val restore = AnchorDecision(restore = before.firstVisiblePosition to before.firstVisibleOffsetPx)
    if (config.disabled) return restore
    val bottom = config.autoscrollToBottomThreshold
    if (bottom != null && insertedAtVisualBottom && before.distanceToVisualBottomPx <= bottom * before.viewportPx) {
      return AnchorDecision(scrollTo = VisualEdge.BOTTOM, animated = config.animateAutoScrollToBottom)
    }
    val top = config.autoscrollToTopThreshold
    if (top != null && insertedAtVisualTop && before.distanceToVisualTopPx <= top * before.viewportPx) {
      return AnchorDecision(scrollTo = VisualEdge.TOP, animated = config.animateAutoScrollToBottom)
    }
    return if (anchorLost) restore else AnchorDecision()
  }

  // Data indices of the laid-out rows, -1 for the list components (they outlive any data change); removed holds
  // (index, count) pairs in the old indices.
  fun losesEveryRow(laidOutDataIndices: IntArray, removed: IntArray): Boolean =
    laidOutDataIndices.isNotEmpty() && laidOutDataIndices.all { index ->
      index >= 0 && (removed.indices step 2).any { i -> index >= removed[i] && index < removed[i] + removed[i + 1] }
    }
}

// The index and viewPosition that put a visual edge in view: the content start is the visual bottom when inverted.
object AnchorTargets {
  fun indexFor(edge: VisualEdge, count: Int, inverted: Boolean): Pair<Int, Float> =
    if ((edge == VisualEdge.TOP) != inverted) 0 to 0f else count - 1 to 1f
}
