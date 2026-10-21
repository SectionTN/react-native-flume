package me.sectiontn.flume

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LayoutFlagsTest {
  @Test fun anInvertedListOnlyReverses() = assertFalse(LayoutFlags.stackFromEnd(inverted = true, startRenderingFromBottom = true))

  @Test fun startRenderingFromBottomStacksAnUprightList() = assertTrue(LayoutFlags.stackFromEnd(inverted = false, startRenderingFromBottom = true))

  @Test fun anUprightListStartsAtTheTopByDefault() = assertFalse(LayoutFlags.stackFromEnd(inverted = false, startRenderingFromBottom = false))
}
