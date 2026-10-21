package me.sectiontn.flume

// One text node as the Text op describes it, dp units; the measurer and the view read the same spec.
data class TextSpec(
  val text: String,
  val fontSizeDp: Float,
  val fontWeight: Int,
  val fontFamily: String?,
  val italic: Boolean,
  val lineHeightDp: Float,
  val letterSpacingDp: Float,
  val textAlign: Int,
  val decoration: Int,
  val maxWidthDp: Float,
  val numberOfLines: Int,
  val allowFontScaling: Boolean = true,
)

// One image node as the Image op describes it, px units.
data class ImageSpec(
  val uri: String,
  val resizeMode: Int,
  val tintColor: Int?,
  val headers: Map<String, String>,
  val widthPx: Int,
  val heightPx: Int,
  val radiiPx: FloatArray,
)
