package me.sectiontn.flume

import android.content.Context
import android.view.ViewGroup

// A container whose children are placed by the cell view from Yoga frames.
open class FlumeGroup(context: Context) : ViewGroup(context) {
  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) = Unit
}
