package me.sectiontn.flume

import org.junit.Assert.assertEquals
import org.junit.Test

// Three rows of 100 in a viewport of 600.
class ContentPlacementTest {
  @Test
  fun shortContentMovesToTheCenterOnceAndStays() {
    assertEquals(150, ContentPlacement.shift(ContentJustify.Center, firstStart = 0, lastEnd = 300, viewportStart = 0, viewportEnd = 600))
    assertEquals(0, ContentPlacement.shift(ContentJustify.Center, firstStart = 150, lastEnd = 450, viewportStart = 0, viewportEnd = 600))
    assertEquals(0, ContentPlacement.shift(ContentJustify.Center, firstStart = 0, lastEnd = 900, viewportStart = 0, viewportEnd = 600))
  }

  @Test
  fun shortContentMovesToTheEndOnceAndStays() {
    assertEquals(300, ContentPlacement.shift(ContentJustify.End, firstStart = 0, lastEnd = 300, viewportStart = 0, viewportEnd = 600))
    assertEquals(0, ContentPlacement.shift(ContentJustify.End, firstStart = 300, lastEnd = 600, viewportStart = 0, viewportEnd = 600))
    assertEquals(0, ContentPlacement.shift(ContentJustify.End, firstStart = 0, lastEnd = 900, viewportStart = 0, viewportEnd = 600))
  }

  @Test
  fun justifyContentNamesMapToPlacements() {
    assertEquals(ContentJustify.End, ContentJustify.of("end"))
    assertEquals(ContentJustify.Center, ContentJustify.of("center"))
    assertEquals(ContentJustify.Start, ContentJustify.of(null))
  }

  @Test
  fun aGrowingFooterTakesWhatTheRowsAboveLeave() {
    assertEquals(300, ContentPlacement.footerMinPx(contentStart = 0, footerStart = 300, viewportPx = 600))
    assertEquals(0, ContentPlacement.footerMinPx(contentStart = 0, footerStart = 700, viewportPx = 600))
  }
}
