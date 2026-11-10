package me.sectiontn.flume

import com.facebook.react.bridge.WritableNativeMap
import com.facebook.react.uimanager.PixelUtil
import com.facebook.react.uimanager.StateWrapper

// Writes where React cells show into the shadow tree, so measure() inside a cell matches the screen.
// The scroll sums move every row by the same amount, so a plain scroll rewrites only the list's state.
class ShadowOrigins {
  var viewState: StateWrapper? = null
    set(value) {
      if (field == null) writtenX = Long.MIN_VALUE
      field = value
    }
  var scrollXPx = 0L
    private set
  var scrollYPx = 0L
    private set
  private var writtenX = Long.MIN_VALUE
  private var writtenY = Long.MIN_VALUE

  fun scrolled(dx: Int, dy: Int) {
    scrollXPx += dx
    scrollYPx += dy
  }

  fun writeView() {
    val state = viewState ?: return
    if (scrollXPx == writtenX && scrollYPx == writtenY) return
    writtenX = scrollXPx
    writtenY = scrollYPx
    state.updateState(point(scrollXPx, scrollYPx))
  }

  // Takes the slot's position in the list in px, before the scroll sums are added.
  fun writeSlot(view: FlumeSlotView, leftPx: Int, topPx: Int) {
    val state = view.stateWrapper ?: return
    val x = leftPx + scrollXPx
    val y = topPx + scrollYPx
    if (x == view.writtenX && y == view.writtenY) return
    view.writtenX = x
    view.writtenY = y
    state.updateState(point(x, y))
  }

  private fun point(x: Long, y: Long) = WritableNativeMap().apply {
    putDouble("x", PixelUtil.toDIPFromPixel(x.toFloat()).toDouble())
    putDouble("y", PixelUtil.toDIPFromPixel(y.toFloat()).toDouble())
  }
}
