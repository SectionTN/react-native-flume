package me.sectiontn.flume

import android.content.Context
import android.net.Uri
import com.facebook.drawee.backends.pipeline.Fresco
import com.facebook.drawee.drawable.ScalingUtils
import com.facebook.drawee.generic.GenericDraweeHierarchyBuilder
import com.facebook.drawee.generic.RoundingParams
import com.facebook.drawee.view.SimpleDraweeView
import com.facebook.imagepipeline.common.ResizeOptions
import com.facebook.imagepipeline.request.ImageRequestBuilder

// Same Fresco pipeline and caches as <Image>; no fade on load, for parity with iOS.
object FlumeImages {
  fun create(context: Context): SimpleDraweeView {
    val hierarchy = GenericDraweeHierarchyBuilder(context.resources)
      .setFadeDuration(0)
      .setActualImageScaleType(ScalingUtils.ScaleType.CENTER_CROP)
      .build()
    return SimpleDraweeView(context, hierarchy)
  }

  fun bind(view: SimpleDraweeView, uri: String, widthPx: Int, heightPx: Int, radiusPx: Float) {
    view.hierarchy.roundingParams = if (radiusPx > 0f) RoundingParams.fromCornersRadius(radiusPx) else null
    val request = ImageRequestBuilder.newBuilderWithSource(Uri.parse(uri))
    if (widthPx > 0 && heightPx > 0) request.resizeOptions = ResizeOptions(widthPx, heightPx)
    view.controller = Fresco.newDraweeControllerBuilder()
      .setImageRequest(request.build())
      .setOldController(view.controller)
      .build()
  }
}
