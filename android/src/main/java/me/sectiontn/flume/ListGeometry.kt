package me.sectiontn.flume

// A grid's lane insets and column gap.
data class Lanes(val insetStartPx: Int = 0, val insetEndPx: Int = 0, val gapPx: Int = 0)

// Orientation, columns, padding, viewport and font scale of one list; cells bind at the size computed here.
class ListGeometry(private val density: Float) {
  var horizontal = false
    private set
  var numColumns = 1
    private set
  var inverted = false
    private set
  var lanes = Lanes()
    private set
  var paddingPx = IntArray(4)
    private set
  var widthPx = 0
    private set
  var heightPx = 0
    private set
  private var fontScale = 0f

  val ready: Boolean get() = bindAxisPx(widthPx, heightPx) > 0
  val innerWidthPx: Int get() = widthPx - paddingPx[0] - paddingPx[2]
  val innerHeightPx: Int get() = heightPx - paddingPx[1] - paddingPx[3]
  val widthDp: Float get() = if (widthPx > 0) widthPx / density else -1f

  // True when the bind axis or the font scale changed; a zero bind axis is ignored, the other axis is only stored.
  fun update(widthPx: Int, heightPx: Int, fontScale: Float): Boolean {
    val next = bindAxisPx(widthPx, heightPx)
    if (next <= 0) return false
    val changed = next != bindAxisPx(this.widthPx, this.heightPx) || fontScale != this.fontScale
    this.widthPx = widthPx
    this.heightPx = heightPx
    this.fontScale = fontScale
    return changed
  }

  // True when cells must rebind: the orientation, the columns, the lanes or the padding along the bind axis changed.
  fun configure(horizontal: Boolean, numColumns: Int, inverted: Boolean, paddingPx: IntArray, lanes: Lanes = Lanes()): Boolean {
    val changed = horizontal != this.horizontal || numColumns != this.numColumns || lanes != this.lanes ||
      bindPadding(horizontal, paddingPx) != bindPadding(this.horizontal, this.paddingPx)
    this.horizontal = horizontal
    this.numColumns = numColumns
    this.inverted = inverted
    this.paddingPx = paddingPx.copyOf()
    this.lanes = lanes
    return changed
  }

  // A data row's width: inside the lane insets, a grid column also leaves the gaps between columns.
  fun cellWidthDp(fullSpan: Boolean): Float {
    if (horizontal) return Float.NaN
    val row = (innerWidthPx - lanes.insetStartPx - lanes.insetEndPx).toFloat()
    return (if (fullSpan) row else (row - lanes.gapPx * (numColumns - 1)) / numColumns) / density
  }

  fun cellHeightDp(): Float = if (horizontal) innerHeightPx / density else Float.NaN

  private fun bindAxisPx(widthPx: Int, heightPx: Int) = if (horizontal) heightPx else widthPx

  private fun bindPadding(horizontal: Boolean, paddingPx: IntArray) =
    if (horizontal) paddingPx[1] + paddingPx[3] else paddingPx[0] + paddingPx[2]
}
