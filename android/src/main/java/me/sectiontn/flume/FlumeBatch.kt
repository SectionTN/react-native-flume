package me.sectiontn.flume

import com.facebook.proguard.annotations.DoNotStrip

// One bind's view operations as typed arrays, dp units; stats (null unless BindStats is on) are workletNs, reconcileNs, layoutNs, measureCalls, measureHits, first.
@DoNotStrip
class FlumeBatch
@DoNotStrip
constructor(
  @JvmField val ints: IntArray,
  @JvmField val floats: FloatArray,
  @JvmField val strings: Array<String>,
  @JvmField val rootWidth: Float,
  @JvmField val rootHeight: Float,
  @JvmField val first: Boolean,
  @JvmField val stats: LongArray?,
)
