package me.sectiontn.flume

import android.graphics.Paint
import android.text.style.LineHeightSpan
import kotlin.math.ceil
import kotlin.math.floor

// Makes every line exactly heightPx tall, the way React Native's own line height span does.
class FlumeLineHeightSpan(private val heightPx: Int) : LineHeightSpan {
  override fun chooseHeight(text: CharSequence, start: Int, end: Int, spanstartv: Int, lineHeight: Int, fm: Paint.FontMetricsInt) {
    if (fm.descent > heightPx) {
      fm.bottom = heightPx
      fm.descent = heightPx
      fm.ascent = 0
      fm.top = 0
    } else if (-fm.ascent + fm.descent > heightPx) {
      fm.bottom = fm.descent
      fm.ascent = -heightPx + fm.descent
      fm.top = fm.ascent
    } else if (-fm.ascent + fm.bottom > heightPx) {
      fm.top = fm.ascent
      fm.bottom = fm.ascent + heightPx
    } else if (-fm.top + fm.bottom > heightPx) {
      fm.top = fm.bottom - heightPx
    } else {
      val additional = heightPx - (-fm.top + fm.bottom)
      fm.top -= ceil(additional / 2f).toInt()
      fm.bottom += floor(additional / 2f).toInt()
      fm.ascent = fm.top
      fm.descent = fm.bottom
    }
  }
}
