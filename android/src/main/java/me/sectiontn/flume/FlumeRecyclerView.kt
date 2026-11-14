package me.sectiontn.flume

import android.content.Context
import android.os.Build
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import androidx.recyclerview.widget.RecyclerView
import com.facebook.react.bridge.ReactContext
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

// ScrollView looks at the touch target: the topmost view under the point, down to an EditText, in local coordinates.
object TextInputHit {
  fun at(view: View, x: Float, y: Float): Boolean {
    if (view.visibility != View.VISIBLE || x < 0 || y < 0 || x >= view.width || y >= view.height) return false
    if (view is EditText) return true
    val group = view as? ViewGroup ?: return false
    for (i in group.childCount - 1 downTo 0) {
      val child = group.getChildAt(i)
      val childX = x + group.scrollX - child.left - child.translationX
      val childY = y + group.scrollY - child.top - child.translationY
      if (child.visibility == View.VISIBLE && childX >= 0 && childY >= 0 && childX < child.width && childY < child.height) {
        return at(child, childX, childY)
      }
    }
    return false
  }
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

class FlumeRecyclerView(context: Context) : RecyclerView(context) {
  var scrollEnabled = true
  var flingFactor = 1f
  var keyboardShouldPersistTaps = KeyboardTaps.NEVER
  private val keyboardGesture = KeyboardGesture()
  private val nativeGesture = NativeGestureTracker()

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
