package me.sectiontn.flume

import android.content.Context
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.net.Uri
import com.facebook.drawee.backends.pipeline.Fresco
import com.facebook.drawee.drawable.ScalingUtils
import com.facebook.drawee.generic.GenericDraweeHierarchy
import com.facebook.drawee.generic.GenericDraweeHierarchyBuilder
import com.facebook.drawee.generic.RoundingParams
import com.facebook.drawee.view.SimpleDraweeView
import com.facebook.imagepipeline.common.ResizeOptions
import com.facebook.imagepipeline.request.ImageRequestBuilder
import com.facebook.react.bridge.Arguments
import com.facebook.react.modules.fresco.ReactNetworkImageRequest

// The bitmap fills the padding, set to the content insets, so a border draws beside it as in React Native's iOS Image.
class FlumeImageView(context: Context, hierarchy: GenericDraweeHierarchy) : SimpleDraweeView(context, hierarchy) {
  fun setInsets(left: Int, top: Int, right: Int, bottom: Int) {
    if (left != paddingLeft || top != paddingTop || right != paddingRight || bottom != paddingBottom) {
      setPadding(left, top, right, bottom)
    }
  }
}

// Same Fresco pipeline and caches as <Image>; no fade on load, for parity with iOS.
object FlumeImages {
  fun create(context: Context): FlumeImageView {
    val hierarchy = GenericDraweeHierarchyBuilder(context.resources)
      .setFadeDuration(0)
      .setActualImageScaleType(ScalingUtils.ScaleType.CENTER_CROP)
      .build()
    return FlumeImageView(context, hierarchy)
  }

  fun bind(view: FlumeImageView, spec: ImageSpec) {
    val hierarchy = view.hierarchy
    hierarchy.actualImageScaleType = when (spec.resizeMode) {
      1 -> ScalingUtils.ScaleType.FIT_CENTER
      2 -> ScalingUtils.ScaleType.FIT_XY
      3 -> ScalingUtils.ScaleType.CENTER
      else -> ScalingUtils.ScaleType.CENTER_CROP
    }
    hierarchy.setActualImageColorFilter(spec.tintColor?.let { PorterDuffColorFilter(it, PorterDuff.Mode.SRC_IN) })
    val r = spec.radiiPx
    hierarchy.roundingParams = if (r.any { it > 0f }) RoundingParams.fromCornersRadii(r[0], r[1], r[2], r[3]) else null
    val builder = ImageRequestBuilder.newBuilderWithSource(Uri.parse(spec.uri))
    if (spec.widthPx > 0 && spec.heightPx > 0) builder.resizeOptions = ResizeOptions(spec.widthPx, spec.heightPx)
    // Headers ride on React Native's own request type, which its Fresco fetcher reads.
    val request = if (spec.headers.isEmpty()) builder.build()
    else ReactNetworkImageRequest.fromBuilderWithHeaders(builder, Arguments.makeNativeMap(spec.headers as Map<String, Any?>))
    view.controller = Fresco.newDraweeControllerBuilder()
      .setImageRequest(request)
      .setOldController(view.controller)
      .build()
  }
}
