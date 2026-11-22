package me.sectiontn.flume

import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.RecyclerView

// RecyclerView's animator, held still for the layout that rebinds every row at a new shape or size.
class FlumeItemAnimator : DefaultItemAnimator() {
  // Set before such a rebind and cleared once its layout completes, so its rows land in place instead of fading in.
  var still = false

  override fun animateAppearance(holder: RecyclerView.ViewHolder, pre: ItemHolderInfo?, post: ItemHolderInfo): Boolean =
    if (still) settle(holder) else super.animateAppearance(holder, pre, post)

  override fun animateDisappearance(holder: RecyclerView.ViewHolder, pre: ItemHolderInfo, post: ItemHolderInfo?): Boolean =
    if (still) settle(holder) else super.animateDisappearance(holder, pre, post)

  override fun animatePersistence(holder: RecyclerView.ViewHolder, pre: ItemHolderInfo, post: ItemHolderInfo): Boolean =
    if (still) settle(holder) else super.animatePersistence(holder, pre, post)

  override fun animateChange(
    oldHolder: RecyclerView.ViewHolder,
    newHolder: RecyclerView.ViewHolder,
    pre: ItemHolderInfo,
    post: ItemHolderInfo,
  ): Boolean {
    if (!still) return super.animateChange(oldHolder, newHolder, pre, post)
    if (newHolder !== oldHolder) dispatchAnimationFinished(newHolder)
    return settle(oldHolder)
  }

  private fun settle(holder: RecyclerView.ViewHolder): Boolean {
    dispatchAnimationFinished(holder)
    return false
  }
}
