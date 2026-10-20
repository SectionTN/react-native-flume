package me.sectiontn.flume

import android.content.Context

// A pressable container: the pressed state dims it to pressedOpacity, clicks report its id to the cell.
class FlumePressable(context: Context) : FlumeGroup(context) {
  var pressId = ""
  var pressedOpacity = 0.7f
  var baseOpacity = 1f

  init {
    isClickable = true
    isLongClickable = true
  }

  override fun drawableStateChanged() {
    super.drawableStateChanged()
    alpha = if (isPressed) baseOpacity * pressedOpacity else baseOpacity
  }
}
