package me.sectiontn.flume

import android.content.Context
import android.graphics.Canvas
import android.graphics.Outline
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.ViewGroup
import android.view.ViewOutlineProvider
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.Button
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

  // The root's box, background and border: drawn in its Yoga frame, which can be smaller than the cell.
  private var rootPaint: GradientDrawable? = null

  init {
    outlineProvider = object : ViewOutlineProvider() {
      override fun getOutline(view: View, outline: Outline) {
        val paint = rootPaint
        if (paint != null) {
          paint.getOutline(outline)
        } else {
          outline.setRect(0, 0, view.width, view.height)
          outline.alpha = 0f
        }
      }
    }
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

  // A list's own cell, which can stretch to its grid row. The pinned copy and the measuring holder keep 0.
  var storeId = 0
  private var naturalHeightPx = 0
  private var stretchedPx = 0

  // A bind lays the root out at its own height, so the next measure stretches it again.
  fun apply(batch: FlumeBatch) {
    tree.apply(batch)
    naturalHeightPx = tree.rootHeightPx
    stretchedPx = 0
    requestLayout()
  }

  private fun stretchTo(targetPx: Int) {
    if (storeId == 0 || targetPx == stretchedPx) return
    val batch = FlumeJni.relayout(storeId, cellId, if (targetPx > 0) targetPx / density else Float.NaN) ?: return
    tree.apply(batch)
    stretchedPx = targetPx
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
    val paint = if (drawn) GradientDrawable().apply {
      setColor(backgroundColor)
      cornerRadii = floatArrayOf(radiiPx[0], radiiPx[0], radiiPx[1], radiiPx[1], radiiPx[2], radiiPx[2], radiiPx[3], radiiPx[3])
      if (borderWidthPx > 0f) setStroke(borderWidthPx.roundToInt(), borderColor)
    } else null
    if (view === this) {
      rootPaint = paint
      placeRootPaint()
      invalidate()
    } else {
      view.background = paint
    }
    view.elevation = elevationPx
    view.alpha = opacity
    (view as? FlumePressable)?.baseOpacity = opacity
  }

  override fun text(view: View, spec: TextSpec, color: Int, insetLeftPx: Float, insetTopPx: Float) {
    (view as? FlumeTextView)?.let {
      it.set(measurer.layoutFor(spec), color, insetLeftPx, insetTopPx)
      // A custom-drawn text exposes nothing by itself; the description is what TalkBack names a pressable from.
      it.contentDescription = spec.text
    }
  }

  override fun image(view: View, spec: ImageSpec) {
    (view as? FlumeImageView)?.let { FlumeImages.bind(it, spec) }
  }

  override fun imageInsets(view: View, leftPx: Float, topPx: Float, rightPx: Float, bottomPx: Float) {
    (view as? FlumeImageView)?.setInsets(leftPx.roundToInt(), topPx.roundToInt(), rightPx.roundToInt(), bottomPx.roundToInt())
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

  // GridLayoutManager measures a row's shorter cells again at the row's height, exactly, and they stretch to it.
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    if (horizontal) {
      setMeasuredDimension(tree.rootWidthPx, MeasureSpec.getSize(heightMeasureSpec))
      return
    }
    val exact = MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.EXACTLY
    stretchTo(CellStretch.target(exact, MeasureSpec.getSize(heightMeasureSpec), naturalHeightPx))
    setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), if (stretchedPx > 0) stretchedPx else naturalHeightPx)
  }

  // Every descendant is placed here from its Yoga frame; FlumeGroup containers do no layout of their own.
  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
    // The tallest cell of a row skips the exact pass, so its final height decides whether it stays stretched.
    if (!horizontal) stretchTo(CellStretch.target(true, b - t, naturalHeightPx))
    tree.frames { view, left, top, width, height ->
      view.measure(MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY))
      view.layout(left, top, left + width, top + height)
    }
    placeRootPaint()
  }

  // A grid tile with its own height or width paints that box inside a longer row or a wider column, as React Native.
  private fun placeRootPaint() {
    val bounds = rootPaint?.bounds
    if (bounds != null && (bounds.right != tree.rootWidthPx || bounds.bottom != tree.rootHeightPx)) {
      rootPaint?.setBounds(0, 0, tree.rootWidthPx, tree.rootHeightPx)
    }
    invalidateOutline()
  }

  override fun dispatchDraw(canvas: Canvas) {
    rootPaint?.draw(canvas)
    super.dispatchDraw(canvas)
  }

  private object ButtonDelegate : View.AccessibilityDelegate() {
    override fun onInitializeAccessibilityNodeInfo(host: View, info: AccessibilityNodeInfo) {
      super.onInitializeAccessibilityNodeInfo(host, info)
      info.className = Button::class.java.name
    }
  }
}

// The length a cell's root is laid out at: a longer exact spec stretches it, anything else gives its own, as 0.
object CellStretch {
  fun target(exact: Boolean, specPx: Int, naturalPx: Int): Int = if (exact && specPx > naturalPx) specPx else 0
}
