package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

// Three rows of 100 in a viewport of 600.
class ContentPlacementTest {
  @Test
  fun shortContentMovesToTheCenterOnceAndStays() {
    assertEquals(150, ContentPlacement.centerShift(firstStart = 0, lastEnd = 300, viewportStart = 0, viewportEnd = 600))
    assertEquals(0, ContentPlacement.centerShift(firstStart = 150, lastEnd = 450, viewportStart = 0, viewportEnd = 600))
    assertEquals(0, ContentPlacement.centerShift(firstStart = 0, lastEnd = 900, viewportStart = 0, viewportEnd = 600))
  }

  @Test
  fun aGrowingFooterTakesWhatTheRowsAboveLeave() {
    assertEquals(300, ContentPlacement.footerMinPx(contentStart = 0, footerStart = 300, viewportPx = 600))
    assertEquals(0, ContentPlacement.footerMinPx(contentStart = 0, footerStart = 700, viewportPx = 600))
  }

  @Test
  fun justifyEndStacksASingleColumnFromTheEnd() {
    assertEquals(true, LayoutShape.of(horizontal = false, numColumns = 1, inverted = false, startRenderingFromBottom = false, justifyEnd = true).stackFromEnd)
    assertEquals(false, LayoutShape.of(horizontal = false, numColumns = 1, inverted = false, startRenderingFromBottom = false, justifyEnd = false).stackFromEnd)
  }
}
