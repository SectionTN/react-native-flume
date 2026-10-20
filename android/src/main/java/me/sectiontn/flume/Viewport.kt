package me.sectiontn.flume

// Remembers the last valid list width and font scale: binds skip a zero width, a change rebinds every row.
class ViewportTracker {
  var widthPx = 0
    private set
  private var fontScale = 0f

  fun update(widthPx: Int, fontScale: Float): Boolean {
    if (widthPx <= 0) return false
    if (widthPx == this.widthPx && fontScale == this.fontScale) return false
    this.widthPx = widthPx
    this.fontScale = fontScale
    return true
  }
}
