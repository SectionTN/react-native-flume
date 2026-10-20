package me.sectiontn.flume

import android.content.Context
import android.widget.FrameLayout
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

class FlumeView(context: Context) : FrameLayout(context) {
  val recycler = RecyclerView(context).apply { layoutManager = LinearLayoutManager(context) }
  var storeId = 0

  private val measureAndLayout = Runnable {
    measure(
      MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
      MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY),
    )
    layout(left, top, right, bottom)
  }

  init {
    addView(recycler, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT))
  }

  // Fabric never runs a second Android layout pass, so a RecyclerView relayout has to be driven here.
  override fun requestLayout() {
    super.requestLayout()
    post(measureAndLayout)
  }
}
