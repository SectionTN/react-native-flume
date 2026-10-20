package me.sectiontn.flume

import android.content.Context
import android.graphics.RectF
import android.graphics.drawable.GradientDrawable
import android.util.SparseArray
import android.view.View
import android.view.ViewGroup
import com.facebook.drawee.view.SimpleDraweeView
import kotlin.math.min
import kotlin.math.roundToInt

// One holder's view tree, kept across binds and updated from the engine's op batches.
class FlumeCellView(context: Context, private val measurer: FlumeTextMeasurer) : FlumeGroup(context) {
  private val density = context.resources.displayMetrics.density
  private val views = SparseArray<View>()
  private val frames = SparseArray<RectF>()
  private var rootHeightPx = 0
  var cellId = 0

  fun apply(batch: FlumeBatch) {
    val ints = batch.ints
    val floats = batch.floats
    var i = 0
    var f = 0
    while (i < ints.size) {
      when (ints[i]) {
        1 -> {
          val id = ints[i + 1]
          val kind = ints[i + 2]
          val parentId = ints[i + 3]
          val index = ints[i + 4]
          i += 5
          val view = when (kind) {
            1 -> FlumeTextView(context)
            2 -> FlumeImages.create(context)
            else -> FlumeGroup(context)
          }
          views.put(id, view)
          frames.put(id, RectF())
          val parent = if (parentId == 0) this else views.get(parentId) as? ViewGroup ?: this
          parent.addView(view, min(index, parent.childCount))
        }
        2 -> {
          val id = ints[i + 1]
          i += 2
          val view = views.get(id)
          if (view != null) {
            (view.parent as? ViewGroup)?.removeView(view)
            views.remove(id)
            frames.remove(id)
          }
        }
        3 -> {
          val id = ints[i + 1]
          i += 2
          val x = floats[f] * density
          val y = floats[f + 1] * density
          val w = floats[f + 2] * density
          val h = floats[f + 3] * density
          f += 4
          if (id == 0) rootHeightPx = h.roundToInt() else frames.get(id)?.set(x, y, x + w, y + h)
        }
        4 -> {
          val id = ints[i + 1]
          val color = ints[i + 2]
          i += 3
          val opacity = floats[f]
          val radius = floats[f + 1] * density
          f += 2
          val view = if (id == 0) this else views.get(id)
          if (view != null) {
            view.background = if (color != 0 || radius > 0f) GradientDrawable().apply {
              setColor(color)
              cornerRadius = radius
            } else null
            view.alpha = opacity
          }
        }
        5 -> {
          val id = ints[i + 1]
          val text = batch.strings[ints[i + 2]]
          val color = ints[i + 3]
          val weight = ints[i + 4]
          val lines = ints[i + 5]
          i += 6
          val fontSize = floats[f]
          val maxWidth = floats[f + 1]
          f += 2
          (views.get(id) as? FlumeTextView)?.set(measurer.layoutFor(text, fontSize, weight, maxWidth, lines), color)
        }
        6 -> {
          val id = ints[i + 1]
          val uri = batch.strings[ints[i + 2]]
          i += 3
          val w = floats[f] * density
          val h = floats[f + 1] * density
          val radius = floats[f + 2] * density
          f += 3
          (views.get(id) as? SimpleDraweeView)?.let { FlumeImages.bind(it, uri, w.roundToInt(), h.roundToInt(), radius) }
        }
        else -> return
      }
    }
    requestLayout()
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), rootHeightPx)
  }

  // Every descendant is placed here from its Yoga frame; FlumeGroup containers do no layout of their own.
  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
    for (n in 0 until views.size()) {
      val view = views.valueAt(n)
      val frame = frames.get(views.keyAt(n)) ?: continue
      val w = (frame.right - frame.left).roundToInt()
      val h = (frame.bottom - frame.top).roundToInt()
      view.measure(MeasureSpec.makeMeasureSpec(w, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(h, MeasureSpec.EXACTLY))
      val left = frame.left.roundToInt()
      val top = frame.top.roundToInt()
      view.layout(left, top, left + w, top + h)
    }
  }
}
