package me.sectiontn.flume

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.ViewGroup
import com.facebook.drawee.view.SimpleDraweeView
import kotlin.math.min

// One holder's view tree, kept across binds and updated from the engine's op batches through CellTree.
class FlumeCellView(context: Context, private val measurer: FlumeTextMeasurer) : FlumeGroup(context), CellViews<View> {
  private val density = context.resources.displayMetrics.density
  private val tree = CellTree(this, density)
  private var rootHeightPx = 0
  var cellId = 0

  override val root: View
    get() = this

  fun apply(batch: FlumeBatch) {
    rootHeightPx = tree.apply(batch)
    requestLayout()
  }

  override fun create(kind: Int): View = when (kind) {
    1 -> FlumeTextView(context)
    2 -> FlumeImages.create(context)
    else -> FlumeGroup(context)
  }

  override fun add(parent: View, child: View, index: Int) {
    val group = parent as? ViewGroup ?: return
    group.addView(child, min(index, group.childCount))
  }

  override fun remove(parent: View, child: View) {
    (parent as? ViewGroup)?.removeView(child)
  }

  override fun move(parent: View, child: View, index: Int) {
    val group = parent as? ViewGroup ?: return
    group.removeView(child)
    group.addView(child, min(index, group.childCount))
  }

  override fun removeAll() = removeAllViews()

  override fun viewStyle(view: View, backgroundColor: Int, borderColor: Int, opacity: Float, radiiPx: FloatArray, borderWidthPx: Float, elevationPx: Float) {
    view.background = if (backgroundColor != 0 || radiiPx[0] > 0f) GradientDrawable().apply {
      setColor(backgroundColor)
      cornerRadius = radiiPx[0]
    } else null
    view.alpha = opacity
  }

  override fun text(view: View, spec: TextSpec, color: Int) {
    (view as? FlumeTextView)?.set(measurer.layoutFor(spec.text, spec.fontSizeDp, spec.fontWeight, spec.maxWidthDp, spec.numberOfLines), color)
  }

  override fun image(view: View, spec: ImageSpec) {
    (view as? SimpleDraweeView)?.let { FlumeImages.bind(it, spec.uri, spec.widthPx, spec.heightPx, spec.radiiPx[0]) }
  }

  override fun pressable(view: View, id: String, pressedOpacity: Float) = Unit

  override fun accessibility(view: View, label: String?, testId: String?, role: Int, accessible: Int) = Unit

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), rootHeightPx)
  }

  // Every descendant is placed here from its Yoga frame; FlumeGroup containers do no layout of their own.
  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
    tree.frames { view, left, top, width, height ->
      view.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY))
      view.layout(left, top, left + width, top + height)
    }
  }
}
