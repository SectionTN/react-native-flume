#import "FLMCellViews.h"

#import "FLMColor.h"

#include <algorithm>

// A cell view that keeps its corner mask and shadow path in step with its bounds.
@interface FLMBoxView : UIView
@property (nonatomic) std::array<float, 4> radii;  // topLeft, topRight, bottomRight, bottomLeft
@property (nonatomic) BOOL opaqueShadow;
@end

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

@implementation FLMBoxView

- (void)layoutSubviews
{
  [super layoutSubviews];
  CGFloat radius = 0;
  const BOOL uniform = FLMUniformRadii(_radii, &radius);
  if (!uniform) {
    CAShapeLayer *mask = [self.layer.mask isKindOfClass:CAShapeLayer.class] ? (CAShapeLayer *)self.layer.mask
                                                                             : [CAShapeLayer layer];
    mask.path = FLMRoundedPath(self.bounds, _radii).CGPath;
    self.layer.mask = mask;
  }
  if (self.layer.shadowOpacity > 0 && _opaqueShadow) {
    self.layer.shadowPath = FLMRoundedPath(self.bounds, _radii).CGPath;
  } else {
    self.layer.shadowPath = nil;
  }
}

@end

static UIView *FLMUIView(flume::ViewHandle handle)
{
  return (__bridge UIView *)handle;
}

FLMCellViews::FLMCellViews(UIView *container) : pending_([NSMutableSet set])
{
  FLMBoxView *root = [[FLMBoxView alloc] initWithFrame:container.bounds];
  root.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [container addSubview:root];
  root_ = root;
}

flume::ViewHandle FLMCellViews::root()
{
  return (__bridge flume::ViewHandle)root_;
}

flume::ViewHandle FLMCellViews::create(flume::ViewKind)
{
  FLMBoxView *view = [FLMBoxView new];
  [pending_ addObject:view];
  return (__bridge flume::ViewHandle)view;
}

void FLMCellViews::add(flume::ViewHandle parent, flume::ViewHandle child, int index)
{
  UIView *view = FLMUIView(child);
  [FLMUIView(parent) insertSubview:view atIndex:index];
  [pending_ removeObject:view];
}

void FLMCellViews::remove(flume::ViewHandle, flume::ViewHandle child)
{
  [FLMUIView(child) removeFromSuperview];
}

void FLMCellViews::move(flume::ViewHandle parent, flume::ViewHandle child, int index)
{
  [FLMUIView(parent) insertSubview:FLMUIView(child) atIndex:index];
}

void FLMCellViews::removeAll()
{
  for (UIView *view in [root_.subviews copy]) {
    [view removeFromSuperview];
  }
  [pending_ removeAllObjects];
  viewStyle(root(), flume::ViewStyleSpec{});
}

void FLMCellViews::frame(flume::ViewHandle view, float x, float y, float width, float height)
{
  FLMUIView(view).frame = CGRectMake(x, y, width, height);
}

void FLMCellViews::viewStyle(flume::ViewHandle handle, const flume::ViewStyleSpec &style)
{
  FLMBoxView *view = (__bridge FLMBoxView *)handle;
  CALayer *layer = view.layer;
  view.backgroundColor = style.backgroundColor == 0 ? nil : FLMColor(style.backgroundColor);
  view.alpha = style.opacity;
  layer.borderWidth = style.borderWidth;
  layer.borderColor = style.borderWidth > 0 ? FLMColor(style.borderColor).CGColor : nil;
  view.radii = style.radii;
  CGFloat radius = 0;
  if (FLMUniformRadii(style.radii, &radius)) {
    layer.mask = nil;
    layer.cornerRadius = radius;
    CACornerMask corners = 0;
    if (style.radii[0] > 0) corners |= kCALayerMinXMinYCorner;
    if (style.radii[1] > 0) corners |= kCALayerMaxXMinYCorner;
    if (style.radii[2] > 0) corners |= kCALayerMaxXMaxYCorner;
    if (style.radii[3] > 0) corners |= kCALayerMinXMaxYCorner;
    layer.maskedCorners = radius > 0 ? corners : (kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner |
                                                  kCALayerMaxXMaxYCorner | kCALayerMinXMaxYCorner);
  } else {
    layer.cornerRadius = 0;
  }
  // React Native's iOS shadow: the color's alpha scales shadowOpacity.
  const CGFloat shadowAlpha = ((style.shadowColor >> 24) & 0xFF) / 255.0;
  layer.shadowColor = [FLMColor(style.shadowColor) colorWithAlphaComponent:1].CGColor;
  layer.shadowOpacity = shadowAlpha * style.shadowOpacity;
  layer.shadowRadius = style.shadowRadius;
  layer.shadowOffset = CGSizeMake(style.shadowOffsetX, style.shadowOffsetY);
  view.opaqueShadow = ((style.backgroundColor >> 24) & 0xFF) == 0xFF;
  [view setNeedsLayout];
}

void FLMCellViews::text(flume::ViewHandle, const flume::TextSpec &, float, float) {}

void FLMCellViews::image(flume::ViewHandle, const flume::ImageSpec &) {}

void FLMCellViews::pressable(flume::ViewHandle, const std::string &, float) {}

void FLMCellViews::accessibility(flume::ViewHandle, const std::optional<std::string> &,
                                 const std::optional<std::string> &, int, int)
{
}
