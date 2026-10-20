package me.sectiontn.flume

import android.content.Context
import android.graphics.Paint
import android.graphics.Typeface
import android.os.Build
import android.text.Layout
import android.text.SpannableString
import android.text.Spanned
import android.text.StaticLayout
import android.text.TextPaint
import android.text.TextUtils
import android.util.LruCache
import android.util.TypedValue
import com.facebook.proguard.annotations.DoNotStrip
import com.facebook.react.common.assets.ReactFontManager
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

// Builds the StaticLayout that is both measured and drawn, so the two cannot disagree.
@DoNotStrip
class FlumeTextMeasurer(private val context: Context) {
  private val metrics = context.resources.displayMetrics
  private val density = metrics.density
  private val layouts = LruCache<TextSpec, StaticLayout>(512)
  private val paints = HashMap<PaintKey, TextPaint>()

  private data class PaintKey(val fontSizeDp: Float, val fontWeight: Int, val fontFamily: String?, val italic: Boolean, val letterSpacingDp: Float, val decoration: Int)

  // Width float bits in the high 32 bits, height in the low 32, both dp.
  @DoNotStrip
  fun measure(
    text: String,
    fontSizeDp: Float,
    fontWeight: Int,
    maxWidthDp: Float,
    numberOfLines: Int,
    fontFamily: String,
    italic: Boolean,
    lineHeightDp: Float,
    letterSpacingDp: Float,
    textAlign: Int,
    decoration: Int,
  ): Long {
    val spec = TextSpec(text, fontSizeDp, fontWeight, fontFamily.ifEmpty { null }, italic, lineHeightDp, letterSpacingDp, textAlign, decoration, maxWidthDp, numberOfLines)
    val layout = layoutFor(spec)
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

  fun layoutFor(spec: TextSpec): StaticLayout {
    layouts.get(spec)?.let { return it }
    val widthPx = max(0, min(floor(spec.maxWidthDp * density).toInt(), 1 shl 24))
    val source: CharSequence = if (spec.lineHeightDp > 0f) {
      SpannableString(spec.text).apply {
        setSpan(FlumeLineHeightSpan(sp(spec.lineHeightDp).roundToInt()), 0, length, Spanned.SPAN_INCLUSIVE_INCLUSIVE)
      }
    } else {
      spec.text
    }
    val builder = StaticLayout.Builder.obtain(source, 0, source.length, paintFor(spec), widthPx)
      .setIncludePad(true)
      .setAlignment(
        when (spec.textAlign) {
          2 -> Layout.Alignment.ALIGN_OPPOSITE
          3 -> Layout.Alignment.ALIGN_CENTER
          else -> Layout.Alignment.ALIGN_NORMAL
        },
      )
    if (spec.textAlign == 4 && Build.VERSION.SDK_INT >= 26) builder.setJustificationMode(Layout.JUSTIFICATION_MODE_INTER_WORD)
    if (spec.numberOfLines > 0) builder.setMaxLines(spec.numberOfLines).setEllipsize(TextUtils.TruncateAt.END)
    val layout = builder.build()
    layouts.put(spec, layout)
    return layout
  }

  private fun sp(value: Float): Float = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, value, metrics)

  private fun paintFor(spec: TextSpec): TextPaint {
    val key = PaintKey(spec.fontSizeDp, spec.fontWeight, spec.fontFamily, spec.italic, spec.letterSpacingDp, spec.decoration)
    return paints.getOrPut(key) {
      TextPaint(Paint.ANTI_ALIAS_FLAG).apply {
        // sp, so text follows the system font scale the way <Text> does.
        textSize = sp(spec.fontSizeDp)
        typeface = typefaceFor(spec.fontFamily, spec.fontWeight, spec.italic)
        if (spec.letterSpacingDp != 0f) letterSpacing = sp(spec.letterSpacingDp) / textSize
        isUnderlineText = spec.decoration and 1 != 0
        isStrikeThruText = spec.decoration and 2 != 0
      }
    }
  }

  // Named families resolve through React Native's font manager, so custom and expo-font fonts match <Text>.
  private fun typefaceFor(family: String?, weight: Int, italic: Boolean): Typeface = when {
    !family.isNullOrEmpty() -> ReactFontManager.getInstance().getTypeface(family, weight, italic, context.assets)
    Build.VERSION.SDK_INT >= 28 -> Typeface.create(Typeface.DEFAULT, weight, italic)
    else -> Typeface.create(Typeface.DEFAULT, (if (weight >= 600) Typeface.BOLD else 0) or (if (italic) Typeface.ITALIC else 0))
  }
}
