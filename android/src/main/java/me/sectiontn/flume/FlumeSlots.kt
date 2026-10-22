package me.sectiontn.flume

import android.content.Context
import android.view.View
import android.view.ViewGroup
import com.facebook.react.uimanager.PointerEvents
import com.facebook.react.uimanager.ReactPointerEventsView

// The bay for slot views no holder shows: zero size, invisible and out of touch targeting, so they stay mounted.
class SlotParking(context: Context) : ViewGroup(context), ReactPointerEventsView {
  override val pointerEvents: PointerEvents = PointerEvents.NONE

  init {
    visibility = INVISIBLE
    isEnabled = false
  }

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) = setMeasuredDimension(0, 0)

  override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) = Unit
}

// The host's Fabric children: slot views, parked until a holder shows them.
class FlumeSlots(private val host: FlumeView) {
  val parking = SlotParking(host.context)
  private val children = FabricChildren<View> { (it as? FlumeSlotView)?.slot ?: 0 }

  val count: Int get() = children.count

  fun childAt(index: Int): View? = children.get(index)

  fun add(child: View, index: Int) {
    children.add(index, child)
    (child.parent as? ViewGroup)?.removeView(child)
    parking.addView(child)
  }

  fun removeAt(index: Int) {
    val child = children.removeAt(index)
    (child.parent as? ViewGroup)?.removeView(child)
  }
}
