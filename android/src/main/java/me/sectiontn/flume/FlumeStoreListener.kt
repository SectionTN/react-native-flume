package me.sectiontn.flume

import com.facebook.proguard.annotations.DoNotStrip

// Called on the main thread when the store's order changes or every holder must rebind.
@DoNotStrip
fun interface FlumeStoreListener {
  @DoNotStrip fun onDataChanged(count: Int)
}
