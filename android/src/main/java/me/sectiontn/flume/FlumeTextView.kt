package me.sectiontn.flume

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.text.StaticLayout
import android.view.View

class FlumeTextView(context: Context) : View(context) {
  private var layout: StaticLayout? = null
  private var color = Color.BLACK

  fun set(layout: StaticLayout, color: Int) {
    this.layout = layout
    this.color = color
    invalidate()
  }

  override fun onDraw(canvas: Canvas) {
    val current = layout ?: return
    current.paint.color = color
    current.draw(canvas)
  }
}
