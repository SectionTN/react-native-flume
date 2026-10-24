package me.sectiontn.flume

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.text.StaticLayout
import android.view.View

class FlumeTextView(context: Context) : View(context) {
  private var layout: StaticLayout? = null
  private var color = Color.BLACK
  private var insetLeft = 0f
  private var insetTop = 0f

  // The insets are the text node's padding plus border; Yoga's frame is the border box.
  fun set(layout: StaticLayout, color: Int, insetLeftPx: Float, insetTopPx: Float) {
    this.layout = layout
    this.color = color
    insetLeft = insetLeftPx
    insetTop = insetTopPx
    invalidate()
  }

  override fun onDraw(canvas: Canvas) {
    val current = layout ?: return
    current.paint.color = color
    canvas.save()
    canvas.translate(insetLeft, insetTop)
    current.draw(canvas)
    canvas.restore()
  }
}
