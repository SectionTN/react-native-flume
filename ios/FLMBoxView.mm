#import "FLMBoxView.h"

#include <algorithm>

static BOOL FLMUniformRadii(const std::array<float, 4> &radii, CGFloat *radius)
{
  float value = 0;
  for (float r : radii) {
    if (r == 0) {
      continue;
    }
    if (value != 0 && r != value) {
      return NO;
    }
    value = r;
  }
  *radius = value;
  return YES;
}

static UIBezierPath *FLMRoundedPath(CGRect rect, const std::array<float, 4> &radii)
{
  const CGFloat limit = std::min(rect.size.width, rect.size.height) / 2;
  const CGFloat tl = std::min<CGFloat>(radii[0], limit);
  const CGFloat tr = std::min<CGFloat>(radii[1], limit);
  const CGFloat br = std::min<CGFloat>(radii[2], limit);
  const CGFloat bl = std::min<CGFloat>(radii[3], limit);
  UIBezierPath *path = [UIBezierPath bezierPath];
  [path moveToPoint:CGPointMake(CGRectGetMinX(rect) + tl, CGRectGetMinY(rect))];
  [path addLineToPoint:CGPointMake(CGRectGetMaxX(rect) - tr, CGRectGetMinY(rect))];
  [path addArcWithCenter:CGPointMake(CGRectGetMaxX(rect) - tr, CGRectGetMinY(rect) + tr)
                  radius:tr startAngle:-M_PI_2 endAngle:0 clockwise:YES];
  [path addLineToPoint:CGPointMake(CGRectGetMaxX(rect), CGRectGetMaxY(rect) - br)];
  [path addArcWithCenter:CGPointMake(CGRectGetMaxX(rect) - br, CGRectGetMaxY(rect) - br)
                  radius:br startAngle:0 endAngle:M_PI_2 clockwise:YES];
  [path addLineToPoint:CGPointMake(CGRectGetMinX(rect) + bl, CGRectGetMaxY(rect))];
  [path addArcWithCenter:CGPointMake(CGRectGetMinX(rect) + bl, CGRectGetMaxY(rect) - bl)
                  radius:bl startAngle:M_PI_2 endAngle:M_PI clockwise:YES];
  [path addLineToPoint:CGPointMake(CGRectGetMinX(rect), CGRectGetMinY(rect) + tl)];
  [path addArcWithCenter:CGPointMake(CGRectGetMinX(rect) + tl, CGRectGetMinY(rect) + tl)
                  radius:tl startAngle:M_PI endAngle:3 * M_PI_2 clockwise:YES];
  [path closePath];
  return path;
}

void FLMSetRadii(UIView<FLMShaped> *view, const std::array<float, 4> &radii)
{
  CALayer *layer = view.layer;
  view.radii = radii;
  CGFloat radius = 0;
  if (FLMUniformRadii(radii, &radius)) {
    layer.mask = nil;
    layer.cornerRadius = radius;
    CACornerMask corners = 0;
    if (radii[0] > 0) corners |= kCALayerMinXMinYCorner;
    if (radii[1] > 0) corners |= kCALayerMaxXMinYCorner;
    if (radii[2] > 0) corners |= kCALayerMaxXMaxYCorner;
    if (radii[3] > 0) corners |= kCALayerMinXMaxYCorner;
    layer.maskedCorners = radius > 0 ? corners : (kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner |
                                                  kCALayerMaxXMaxYCorner | kCALayerMinXMaxYCorner);
  } else {
    layer.cornerRadius = 0;
  }
  [view setNeedsLayout];
}

void FLMLayoutShape(UIView<FLMShaped> *view)
{
  const std::array<float, 4> radii = view.radii;
  CGFloat radius = 0;
  if (!FLMUniformRadii(radii, &radius)) {
    CAShapeLayer *mask = [view.layer.mask isKindOfClass:CAShapeLayer.class] ? (CAShapeLayer *)view.layer.mask
                                                                             : [CAShapeLayer layer];
    mask.path = FLMRoundedPath(view.bounds, radii).CGPath;
    view.layer.mask = mask;
  }
  if (view.layer.shadowOpacity > 0 && view.opaqueShadow) {
    view.layer.shadowPath = FLMRoundedPath(view.bounds, radii).CGPath;
  } else {
    view.layer.shadowPath = nil;
  }
}

@implementation FLMBoxView

- (void)layoutSubviews
{
  [super layoutSubviews];
  FLMLayoutShape(self);
}

@end
