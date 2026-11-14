package me.sectiontn.flume

import com.facebook.proguard.annotations.DoNotStrip

// Main thread, one data change: ranges and moves are flat int pairs, sticky is the new data's sticky header indices.
@DoNotStrip
fun interface FlumeStoreListener {
  @DoNotStrip
  fun onListChanged(reset: Boolean, removed: IntArray, inserted: IntArray, moved: IntArray, changed: IntArray, count: Int, sticky: IntArray)
}
