package me.sectiontn.flume

// View types: a data type is typeId * 2, plus 1 for React; the list components are negative and odd, so React.
object ViewTypes {
  const val HEADER = -1
  const val FOOTER = -3
  const val EMPTY = -5

  fun isReact(viewType: Int): Boolean = viewType % 2 != 0
}

enum class RowKind { HEADER, DATA, EMPTY, FOOTER }

// The adapter's rows: the header, then the data or the empty row, then the footer. Indices outside count data only.
data class ListRows(
  val header: Boolean = false,
  val dataCount: Int = 0,
  val emptyComponent: Boolean = false,
  val footer: Boolean = false,
) {
  val headerCount: Int get() = if (header) 1 else 0
  val hasEmptyRow: Boolean get() = emptyComponent && dataCount == 0
  val itemCount: Int get() = headerCount + dataCount + (if (hasEmptyRow) 1 else 0) + (if (footer) 1 else 0)

  fun kind(position: Int): RowKind = when {
    header && position == 0 -> RowKind.HEADER
    position < headerCount + dataCount -> RowKind.DATA
    hasEmptyRow && position == headerCount -> RowKind.EMPTY
    else -> RowKind.FOOTER
  }

  fun dataIndex(position: Int): Int = if (position in headerCount until headerCount + dataCount) position - headerCount else -1

  fun position(dataIndex: Int): Int = dataIndex + headerCount

  fun fullSpanPositions(stickyDataIndices: IntArray): IntArray {
    val positions = ArrayList<Int>(stickyDataIndices.size + 3)
    if (header) positions.add(0)
    for (index in stickyDataIndices) if (index in 0 until dataCount) positions.add(position(index))
    if (hasEmptyRow) positions.add(headerCount)
    if (footer) positions.add(itemCount - 1)
    return positions.distinct().sorted().toIntArray()
  }
}
