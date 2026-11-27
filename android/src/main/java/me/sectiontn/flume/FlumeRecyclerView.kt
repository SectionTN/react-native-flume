package me.sectiontn.flume

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.os.Build
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.facebook.react.bridge.ReactContext
import com.facebook.react.uimanager.TouchTargetHelper
import com.facebook.react.uimanager.events.NativeGestureUtil

// decelerationRate scales the fling velocity: RecyclerView exposes no friction, so fast halves it.
object FlingFactor {
  fun of(decelerationRate: Double): Float {
    val t = ((decelerationRate - 0.99) / (0.998 - 0.99)).toFloat()
    return (0.5f + 0.5f * t).coerceIn(0.25f, 1f)
  }
}

enum class KeyboardTaps { ALWAYS, NEVER, HANDLED }

data class TapDecision(val dismiss: Boolean, val swallow: Boolean)

// never: the first tap only closes the keyboard. handled: a tap off any pressable closes it. always: never. A text input always gets its tap.
object KeyboardTapPolicy {
  fun decide(mode: KeyboardTaps, keyboardUp: Boolean, pressableHit: Boolean, textInputHit: Boolean): TapDecision = when {
    !keyboardUp || mode == KeyboardTaps.ALWAYS || textInputHit -> TapDecision(dismiss = false, swallow = false)
    mode == KeyboardTaps.NEVER -> TapDecision(dismiss = true, swallow = true)
    else -> TapDecision(dismiss = !pressableHit, swallow = false)
  }

  fun mode(value: String?): KeyboardTaps = when (value) {
    "always" -> KeyboardTaps.ALWAYS
    "handled" -> KeyboardTaps.HANDLED
    else -> KeyboardTaps.NEVER
  }
}

// ScrollView's responder logic: a gesture the policy dismisses on closes the keyboard at its release, unless it scrolled.
class KeyboardGesture {
  var swallowing = false
    private set
  private var dismissOnRelease = false

  fun down(decision: TapDecision) {
    swallowing = decision.swallow
    dismissOnRelease = decision.dismiss
  }

  fun scrolled() {
    dismissOnRelease = false
  }

  // True when the keyboard closes now.
  fun release(): Boolean {
    val dismiss = dismissOnRelease
    swallowing = false
    dismissOnRelease = false
    return dismiss
  }
}

enum class GestureSignal { NONE, STARTED, ENDED }

// React Native hears of a gesture the list takes (a drag, or a tap the keyboard policy swallows) once, and of its end,
// so a Pressable in a React cell cancels instead of firing, as in ReactScrollView.
class NativeGestureTracker {
  private var started = false

  fun onEvent(actionMasked: Int, dragging: Boolean, swallowing: Boolean): GestureSignal {
    if (actionMasked == MotionEvent.ACTION_UP || actionMasked == MotionEvent.ACTION_CANCEL) {
      val ended = started
      started = false
      return if (ended) GestureSignal.ENDED else GestureSignal.NONE
    }
    if (actionMasked == MotionEvent.ACTION_DOWN) started = false
    if (started || !(dragging || swallowing)) return GestureSignal.NONE
    started = true
    return GestureSignal.STARTED
  }
}

// ScrollView looks at the JS touch target, found as JSTouchDispatcher finds it (z-index, transforms, pointerEvents).
object TextInputHit {
  fun at(group: ViewGroup, x: Float, y: Float): Boolean =
    group.findViewById<View>(TouchTargetHelper.findTargetTagForTouch(x, y, group)) is EditText
}

// ScrollView's rule: a focused input with the keyboard shown; before API 30 the IME's visibility is unknown and counts as shown.
object KeyboardState {
  fun dismissible(hasFocusedInput: Boolean, imeVisible: Boolean?): Boolean = hasFocusedInput && imeVisible != false
}

object OverScroll {
  fun mode(value: String?): Int = when (value) {
    "always" -> View.OVER_SCROLL_ALWAYS
    "never" -> View.OVER_SCROLL_NEVER
    else -> View.OVER_SCROLL_IF_CONTENT_SCROLLS
  }
}

// ScrollView's default style gives the view its themed scroll bars, which a view built in code lacks.
class FlumeRecyclerView(context: Context) :
  RecyclerView(context, null, android.R.attr.scrollViewStyle) {
  var scrollEnabled = true
  var flingFactor = 1f
  var keyboardShouldPersistTaps = KeyboardTaps.NEVER
  private val keyboardGesture = KeyboardGesture()
  private val nativeGesture = NativeGestureTracker()
  private val backgroundPaint = Paint()

  init {
    // The style enables the vertical bar only. Outside the padding, as React Native's ScrollView draws it.
    isHorizontalScrollBarEnabled = true
    scrollBarStyle = View.SCROLLBARS_OUTSIDE_OVERLAY
  }

  // contentContainerStyle's backgroundColor, 0 for none: the rows and their padding, scrolled with them.
  var contentBackground = 0
    set(value) {
      if (field == value) return
      field = value
      backgroundPaint.color = value
      setWillNotDraw(false)
      invalidate()
    }

  // Under the decorations, so a separator line draws over it.
  override fun onDraw(c: Canvas) {
    if (Color.alpha(contentBackground) != 0) drawContentBackground(c)
    super.onDraw(c)
  }

  private fun drawContentBackground(c: Canvas) {
    val lm = layoutManager as? LinearLayoutManager ?: return
    val count = adapter?.itemCount ?: 0
    if (childCount == 0 || count == 0) return
    val horizontal = lm.orientation == HORIZONTAL
    val firstPosition = if (lm.reverseLayout) count - 1 else 0
    val lastPosition = if (lm.reverseLayout) 0 else count - 1
    var firstStart = Int.MAX_VALUE
    var lastEnd = Int.MIN_VALUE
    var hasFirst = false
    var hasLast = false
    for (i in 0 until childCount) {
      val child = getChildAt(i)
      val shift = if (horizontal) child.translationX else child.translationY
      val start = (if (horizontal) lm.getDecoratedLeft(child) else lm.getDecoratedTop(child)) + shift.toInt()
      val end = (if (horizontal) lm.getDecoratedRight(child) else lm.getDecoratedBottom(child)) + shift.toInt()
      firstStart = minOf(firstStart, start)
      lastEnd = maxOf(lastEnd, end)
      val position = getChildAdapterPosition(child)
      if (position == firstPosition) hasFirst = true
      if (position == lastPosition) hasLast = true
    }
    val span = ContentBackground.span(
      firstStart, lastEnd, hasFirst, hasLast,
      if (horizontal) paddingLeft else paddingTop, if (horizontal) paddingRight else paddingBottom,
      if (horizontal) width else height,
    )
    if (horizontal) c.drawRect(span[0].toFloat(), 0f, span[1].toFloat(), height.toFloat(), backgroundPaint)
    else c.drawRect(0f, span[0].toFloat(), width.toFloat(), span[1].toFloat(), backgroundPaint)
  }

  // The keyboard policy sees every gesture here, including the ones a row consumes.
  override fun dispatchTouchEvent(e: MotionEvent): Boolean {
    if (e.actionMasked == MotionEvent.ACTION_DOWN) {
      keyboardGesture.down(keyboardDecision(pressableHitAt(e.x, e.y), textInputAt(e.x, e.y)))
    }
    val handled = super.dispatchTouchEvent(e)
    when (nativeGesture.onEvent(e.actionMasked, scrollState == SCROLL_STATE_DRAGGING, keyboardGesture.swallowing)) {
      GestureSignal.STARTED -> NativeGestureUtil.notifyNativeGestureStarted(this, e)
      GestureSignal.ENDED -> NativeGestureUtil.notifyNativeGestureEnded(this, e)
      GestureSignal.NONE -> Unit
    }
    if (e.actionMasked == MotionEvent.ACTION_UP && keyboardGesture.release()) dismissKeyboard()
    return handled
  }

  // A swallowed gesture never reaches a row but still scrolls the list; a disabled list leaves taps to its rows and takes no drag.
  override fun onInterceptTouchEvent(e: MotionEvent): Boolean {
    if (!scrollEnabled) return keyboardGesture.swallowing
    return super.onInterceptTouchEvent(e) || keyboardGesture.swallowing
  }

  override fun onTouchEvent(e: MotionEvent): Boolean {
    if (!scrollEnabled) return keyboardGesture.swallowing
    return super.onTouchEvent(e) || keyboardGesture.swallowing
  }

  override fun onScrollStateChanged(state: Int) {
    super.onScrollStateChanged(state)
    if (state == SCROLL_STATE_DRAGGING) keyboardGesture.scrolled()
  }

  override fun fling(velocityX: Int, velocityY: Int): Boolean =
    super.fling((velocityX * flingFactor).toInt(), (velocityY * flingFactor).toInt())

  // Fabric lays out a detached list (an inactive tab on rotation); RecyclerView records those item animations but runs them
  // only while attached, and the rows it was animating out stay drawn in place, so they end here.
  override fun onAttachedToWindow() {
    super.onAttachedToWindow()
    itemAnimator?.endAnimations()
  }

  fun keyboardDecision(pressableHit: Boolean, textInputHit: Boolean): TapDecision =
    KeyboardTapPolicy.decide(keyboardShouldPersistTaps, KeyboardState.dismissible(focusedInput() != null, imeVisible()), pressableHit, textInputHit)

  fun dismissKeyboard() {
    val input = focusedInput() ?: return
    input.clearFocus()
    (context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager)?.hideSoftInputFromWindow(input.windowToken, 0)
  }

  private fun focusedInput(): View? = (context as? ReactContext)?.currentActivity?.currentFocus as? EditText

  private fun imeVisible(): Boolean? =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) rootWindowInsets?.isVisible(WindowInsets.Type.ime()) else null

  // Worklet cells hold no inputs, so only a React row can.
  private fun textInputAt(x: Float, y: Float): Boolean {
    val child = findChildViewUnder(x, y) as? FlumeSlotHolder ?: return false
    return TextInputHit.at(child, x - child.left - child.translationX, y - child.top - child.translationY)
  }

  // A React row decides for itself: its slot view takes a tap no child claims and closes the keyboard (FlumeSlot).
  private fun pressableHitAt(x: Float, y: Float): Boolean {
    val child = findChildViewUnder(x, y) ?: return false
    if (child is FlumeSlotHolder) return true
    val cell = child as? FlumeCellView ?: return false
    return cell.isClickable || cell.hasPressableAt((x - cell.left - cell.translationX).toInt(), (y - cell.top - cell.translationY).toInt())
  }
}

// Along the scroll axis: the padding past the first and last laid-out rows, the view's edge where those rows are off screen.
object ContentBackground {
  fun span(firstStart: Int, lastEnd: Int, hasFirst: Boolean, hasLast: Boolean, paddingStart: Int, paddingEnd: Int, viewLength: Int): IntArray =
    intArrayOf(if (hasFirst) firstStart - paddingStart else 0, if (hasLast) lastEnd + paddingEnd else viewLength)
}
