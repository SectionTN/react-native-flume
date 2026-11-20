package me.sectiontn.flume

// FlashList's shape, measured at the visual edges: the top threshold in pixels, the bottom one in viewports.
data class AnchorConfig(
  val disabled: Boolean = false,
  val autoscrollToTopThresholdPx: Double? = null,
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
// starts over from the first row when no laid-out data row survives a change; FlatList and FlashList keep the offset, so that
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
    // An autoscroll after a change that lost every row starts from the kept position, not from where RecyclerView lands.
    val kept = if (anchorLost) restore.restore else null
    val bottom = config.autoscrollToBottomThreshold
    if (bottom != null && insertedAtVisualBottom && before.distanceToVisualBottomPx <= bottom * before.viewportPx) {
      return AnchorDecision(restore = kept, scrollTo = VisualEdge.BOTTOM, animated = config.animateAutoScrollToBottom)
    }
    // React Native's ScrollView always animates to the top.
    val top = config.autoscrollToTopThresholdPx
    if (top != null && top >= 0 && insertedAtVisualTop && before.distanceToVisualTopPx <= top) {
      return AnchorDecision(restore = kept, scrollTo = VisualEdge.TOP, animated = true)
    }
    return if (anchorLost) restore else AnchorDecision()
  }

  // List components (-1) never keep the anchor. removed holds (index, count) pairs in the old indices.
  fun losesEveryRow(laidOutDataIndices: IntArray, removed: IntArray): Boolean {
    val rows = laidOutDataIndices.filter { it >= 0 }
    return rows.isNotEmpty() && rows.all { index ->
      (removed.indices step 2).any { i -> index >= removed[i] && index < removed[i] + removed[i + 1] }
    }
  }
}

// The index and viewPosition that put a visual edge in view: the content start is the visual bottom when inverted.
object AnchorTargets {
  fun indexFor(edge: VisualEdge, count: Int, inverted: Boolean): Pair<Int, Float> =
    if ((edge == VisualEdge.TOP) != inverted) 0 to 0f else count - 1 to 1f
}
