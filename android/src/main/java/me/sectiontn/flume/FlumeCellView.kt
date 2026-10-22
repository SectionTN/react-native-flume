package me.sectiontn.flume

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Button
import com.facebook.drawee.view.SimpleDraweeView
import kotlin.math.min
import kotlin.math.roundToInt

// One holder's view tree, kept across binds and updated from the engine's op batches through CellTree.
class FlumeCellView(context: Context, private val measurer: FlumeTextMeasurer) : FlumeGroup(context), CellViews<View> {
  private val density = context.resources.displayMetrics.density
  private val tree = CellTree(this, density)
  var cellId = 0
  var horizontal = false

  // Receives the pressable id (null for the cell itself) and whether it was a long press.
  var onPress: ((id: String?, long: Boolean) -> Unit)? = null

  // Rows are clickable only when the list has a press handler, so TalkBack and the keyboard see them right.
  var rowsPressable = false
    set(value) {
      field = value
      isClickable = value
      isLongClickable = value
    }

  init {
    setOnClickListener { onPress?.invoke(null, false) }
    setOnLongClickListener {
      onPress?.invoke(null, true)
      true
    }
    // Setting the listeners made the cell clickable; rowsPressable decides that.
    isClickable = false
    isLongClickable = false
  }

  override val root: View
    get() = this

  val rootWidthPx: Int get() = tree.rootWidthPx
  val rootHeightPx: Int get() = tree.rootHeightPx

  fun apply(batch: FlumeBatch) {
    tree.apply(batch)
    requestLayout()
  }

  override fun create(kind: Int): View = when (kind) {
    1 -> FlumeTextView(context)
    2 -> FlumeImages.create(context)
    3 -> FlumePressable(context).also { pressable ->
      pressable.setOnClickListener { onPress?.invoke(pressable.pressId, false) }
      pressable.setOnLongClickListener {
        onPress?.invoke(pressable.pressId, true)
        true
      }
    }
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
    val drawn = backgroundColor != 0 || borderWidthPx > 0f || radiiPx.any { it > 0f }
    view.background = if (drawn) GradientDrawable().apply {
      setColor(backgroundColor)
      cornerRadii = floatArrayOf(radiiPx[0], radiiPx[0], radiiPx[1], radiiPx[1], radiiPx[2], radiiPx[2], radiiPx[3], radiiPx[3])
      if (borderWidthPx > 0f) setStroke(borderWidthPx.roundToInt(), borderColor)
    } else null
    view.elevation = elevationPx
    view.alpha = opacity
    (view as? FlumePressable)?.baseOpacity = opacity
  }

  override fun text(view: View, spec: TextSpec, color: Int) {
    (view as? FlumeTextView)?.let {
      it.set(measurer.layoutFor(spec), color)
      // A custom-drawn text exposes nothing by itself; the description is what TalkBack names a pressable from.
      it.contentDescription = spec.text
    }
  }

  override fun image(view: View, spec: ImageSpec) {
    (view as? SimpleDraweeView)?.let { FlumeImages.bind(it, spec) }
  }

  override fun pressable(view: View, id: String, pressedOpacity: Float) {
    (view as? FlumePressable)?.let {
      it.pressId = id
      it.pressedOpacity = pressedOpacity
    }
  }

  override fun accessibility(view: View, label: String?, testId: String?, role: Int, accessible: Int) {
    view.contentDescription = label
    view.tag = testId
    view.importantForAccessibility = when (accessible) {
      1 -> View.IMPORTANT_FOR_ACCESSIBILITY_YES
      0 -> View.IMPORTANT_FOR_ACCESSIBILITY_NO
      else -> View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
    }
    view.accessibilityDelegate = if (role == 1) ButtonDelegate else null
  }

  fun hasPressableAt(x: Int, y: Int): Boolean = findPressable(this, x, y)

  private fun findPressable(group: ViewGroup, x: Int, y: Int): Boolean {
    for (i in 0 until group.childCount) {
      val child = group.getChildAt(i)
      if (x < child.left || x >= child.right || y < child.top || y >= child.bottom) continue
      if (child is FlumePressable) return true
      if (child is ViewGroup && findPressable(child, x - child.left, y - child.top)) return true
    }
    return false
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    if (horizontal) setMeasuredDimension(tree.rootWidthPx, MeasureSpec.getSize(heightMeasureSpec))
    else setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), tree.rootHeightPx)
  }

  // Every descendant is placed here from its Yoga frame; FlumeGroup containers do no layout of their own.
  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
    tree.frames { view, left, top, width, height ->
      view.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY))
      view.layout(left, top, left + width, top + height)
    }
  }

  private object ButtonDelegate : View.AccessibilityDelegate() {
    override fun onInitializeAccessibilityNodeInfo(host: View, info: AccessibilityNodeInfo) {
      super.onInitializeAccessibilityNodeInfo(host, info)
      info.className = Button::class.java.name
    }
  }
}
