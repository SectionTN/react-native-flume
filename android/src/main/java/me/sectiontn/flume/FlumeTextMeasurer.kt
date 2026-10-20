package me.sectiontn.flume

import android.content.Context
import android.graphics.Paint
import android.graphics.Typeface
import android.os.Build
import android.text.StaticLayout
import android.text.TextPaint
import android.text.TextUtils
import android.util.LruCache
import android.util.TypedValue
import com.facebook.proguard.annotations.DoNotStrip
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

// Builds the StaticLayout that is both measured and drawn, so the two cannot disagree.
@DoNotStrip
class FlumeTextMeasurer(context: Context) {
  private val metrics = context.resources.displayMetrics
  private val density = metrics.density
  private val layouts = LruCache<String, StaticLayout>(512)
  private val paints = HashMap<Long, TextPaint>()

  // Width float bits in the high 32 bits, height in the low 32, both dp.
  @DoNotStrip
  fun measure(text: String, fontSizeDp: Float, fontWeight: Int, maxWidthDp: Float, numberOfLines: Int): Long {
    val layout = layoutFor(text, fontSizeDp, fontWeight, maxWidthDp, numberOfLines)
    var widest = 0f
    for (line in 0 until layout.lineCount) widest = max(widest, layout.getLineWidth(line))
    val width = ceil(widest.toDouble()).toFloat() / density
    val height = layout.height / density
    return (width.toRawBits().toLong() shl 32) or (height.toRawBits().toLong() and 0xffffffffL)
  }

  // Sizes in sp move with the font scale, so a viewport change drops every layout and paint.
  fun invalidate() {
    layouts.evictAll()
    paints.clear()
  }

  fun layoutFor(text: String, fontSizeDp: Float, fontWeight: Int, maxWidthDp: Float, numberOfLines: Int): StaticLayout {
    val key = "$text\u0000$fontSizeDp\u0000$fontWeight\u0000$maxWidthDp\u0000$numberOfLines"
    layouts.get(key)?.let { return it }
    val widthPx = max(0, min(floor(maxWidthDp * density).toInt(), 1 shl 24))
    val builder = StaticLayout.Builder.obtain(text, 0, text.length, paintFor(fontSizeDp, fontWeight), widthPx)
      .setIncludePad(true)
    if (numberOfLines > 0) builder.setMaxLines(numberOfLines).setEllipsize(TextUtils.TruncateAt.END)
    val layout = builder.build()
    layouts.put(key, layout)
    return layout
  }

  private fun paintFor(fontSizeDp: Float, fontWeight: Int): TextPaint {
    val key = (fontSizeDp.toRawBits().toLong() shl 32) or fontWeight.toLong()
    return paints.getOrPut(key) {
      TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        // sp, so text follows the system font scale the way <Text> does.
        textSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, fontSizeDp, metrics)
        typeface = when {
          Build.VERSION.SDK_INT >= 28 -> Typeface.create(Typeface.DEFAULT, fontWeight, false)
          fontWeight >= 600 -> Typeface.DEFAULT_BOLD
          else -> Typeface.DEFAULT
        }
      }
    }
  }
}
