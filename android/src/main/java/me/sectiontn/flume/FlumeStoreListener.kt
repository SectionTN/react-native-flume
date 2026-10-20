package me.sectiontn.flume

import com.facebook.proguard.annotations.DoNotStrip

// Called on the main thread with the list operations of one data change; ranges and moves are flat int pairs.
@DoNotStrip
fun interface FlumeStoreListener {
  @DoNotStrip
  fun onListChanged(reset: Boolean, removed: IntArray, inserted: IntArray, moved: IntArray, changed: IntArray, count: Int)
}
