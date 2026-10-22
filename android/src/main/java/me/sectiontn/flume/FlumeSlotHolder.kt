package me.sectiontn.flume

import android.content.Context
import android.view.ViewGroup
import com.facebook.react.uimanager.PointerEvents
import com.facebook.react.uimanager.ReactPointerEventsView

// Shows one slot's view once Fabric has measured it; until then it is invisible and, since touch targeting
// ignores visibility, out of touch targeting too.
class FlumeSlotHolder(context: Context, private val parking: ViewGroup) : ViewGroup(context), ReactPointerEventsView {
  var horizontal = false
  var estimatePx = 0
  var owner = 0
  var slot = 0
  var view: FlumeSlotView? = null
    private set
  private var touch = PointerEvents.NONE

  override val pointerEvents: PointerEvents get() = touch

  val ready: Boolean get() = view?.measured == true

  init {
    visibility = INVISIBLE
  }

  // The incoming holder can bind before the outgoing one recycles, so a view goes back to the bay only from its holder.
  fun show(next: FlumeSlotView?) {
    val previous = view
    if (previous !== next) {
      if (previous != null && previous.parent === this) {
        removeView(previous)
        parking.addView(previous)
      }
      view = next
      if (next != null) {
        (next.parent as? ViewGroup)?.removeView(next)
        addView(next)
      }
    }
    refresh()
  }

  fun refresh() {
    val shown = ready
    visibility = if (shown) VISIBLE else INVISIBLE
    touch = if (shown) PointerEvents.BOX_NONE else PointerEvents.NONE
    requestLayout()
  }

  // A ReactViewGroup throws on an UNSPECIFIED spec, so the view is measured at its own Fabric size.
  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val shown = view?.takeIf { it.measured }
    shown?.measure(exactly(shown.measuredWidth), exactly(shown.measuredHeight))
    val main = shown?.lengthPx(horizontal) ?: estimatePx
    if (horizontal) {
      setMeasuredDimension(main, MeasureSpec.getSize(heightMeasureSpec))
    } else {
      setMeasuredDimension(MeasureSpec.getSize(widthMeasureSpec), main)
    }
  }

  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
    view?.takeIf { it.measured }?.let { it.layout(0, 0, it.measuredWidth, it.measuredHeight) }
  }

  private fun exactly(px: Int) = MeasureSpec.makeMeasureSpec(px, MeasureSpec.EXACTLY)
}
