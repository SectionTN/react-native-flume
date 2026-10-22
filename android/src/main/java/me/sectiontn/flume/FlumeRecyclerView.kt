package me.sectiontn.flume

import android.content.Context
import android.os.Build
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import androidx.recyclerview.widget.RecyclerView
import com.facebook.react.bridge.ReactContext

// decelerationRate scales the fling velocity: RecyclerView exposes no friction, so fast halves it.
object FlingFactor {
  fun of(decelerationRate: Double): Float {
    val t = ((decelerationRate - 0.99) / (0.998 - 0.99)).toFloat()
    return (0.5f + 0.5f * t).coerceIn(0.25f, 1f)
  }
}

enum class KeyboardTaps { ALWAYS, NEVER, HANDLED }

data class TapDecision(val dismiss: Boolean, val swallow: Boolean)

// never: the first tap only closes the keyboard; handled: a tap off any pressable closes it; always: never.
object KeyboardTapPolicy {
  fun decide(mode: KeyboardTaps, keyboardUp: Boolean, pressableHit: Boolean): TapDecision = when {
    !keyboardUp || mode == KeyboardTaps.ALWAYS -> TapDecision(dismiss = false, swallow = false)
    mode == KeyboardTaps.NEVER -> TapDecision(dismiss = true, swallow = true)
    else -> TapDecision(dismiss = !pressableHit, swallow = false)
  }

  fun mode(value: String?): KeyboardTaps = when (value) {
    "always" -> KeyboardTaps.ALWAYS
    "handled" -> KeyboardTaps.HANDLED
    else -> KeyboardTaps.NEVER
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
  private var swallowing = false

  // A disabled list leaves taps to its rows and takes no drag; a swallowed tap never reaches a row.
  override fun onInterceptTouchEvent(e: MotionEvent): Boolean {
    if (e.actionMasked == MotionEvent.ACTION_DOWN) {
      swallowing = false
      val keyboardUp = KeyboardState.dismissible(focusedInput() != null, imeVisible())
      val decision = KeyboardTapPolicy.decide(keyboardShouldPersistTaps, keyboardUp, pressableHitAt(e.x, e.y))
      if (decision.dismiss) dismissKeyboard()
      if (decision.swallow) {
        swallowing = true
        return true
      }
    }
    if (!scrollEnabled) return false
    return super.onInterceptTouchEvent(e)
  }

  override fun onTouchEvent(e: MotionEvent): Boolean {
    if (swallowing) {
      if (e.actionMasked == MotionEvent.ACTION_UP || e.actionMasked == MotionEvent.ACTION_CANCEL) swallowing = false
      return true
    }
    if (!scrollEnabled) return false
    return super.onTouchEvent(e)
  }

  override fun fling(velocityX: Int, velocityY: Int): Boolean =
    super.fling((velocityX * flingFactor).toInt(), (velocityY * flingFactor).toInt())

  fun dismissKeyboard() {
    val input = focusedInput() ?: return
    input.clearFocus()
    (context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager)?.hideSoftInputFromWindow(input.windowToken, 0)
  }

  private fun focusedInput(): View? = (context as? ReactContext)?.currentActivity?.currentFocus as? EditText

  private fun imeVisible(): Boolean? =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) rootWindowInsets?.isVisible(WindowInsets.Type.ime()) else null

  private fun pressableHitAt(x: Float, y: Float): Boolean {
    val cell = findChildViewUnder(x, y) as? FlumeCellView ?: return false
    return cell.isClickable || cell.hasPressableAt((x - cell.left - cell.translationX).toInt(), (y - cell.top - cell.translationY).toInt())
  }
}
