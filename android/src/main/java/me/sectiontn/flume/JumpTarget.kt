package me.sectiontn.flume

// A scroll command's target in data terms, so a jump that waits through a data change lands on the same index.
sealed interface JumpTarget {
  data class Index(val index: Int) : JumpTarget

  // An adapter position, such as the content start or an edge anchoring scrolls to.
  data class Position(val position: Int) : JumpTarget

  data object End : JumpTarget

  // The adapter position to land on, or null with no rows.
  fun position(rows: ListRows): Int? = when (this) {
    is Index -> ScrollTargets.clampIndex(index, rows.dataCount)?.let(rows::position)
    is Position -> ScrollTargets.clampIndex(position, rows.itemCount)
    End -> ScrollTargets.clampIndex(rows.itemCount - 1, rows.itemCount)
  }

  // An index past the data waits for the data to grow, as when new data and the scroll come from one handler.
  fun waitsForData(rows: ListRows): Boolean = this is Index && index >= rows.dataCount
}
