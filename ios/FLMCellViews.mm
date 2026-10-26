#import "FLMCellViews.h"

#import "FLMBoxView.h"
#import "FLMColor.h"
#import "FLMImageView.h"
#import "FLMText.h"
#import "FLMTextView.h"

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

flume::ViewHandle FLMCellViews::create(flume::ViewKind kind)
{
  UIView *view;
  switch (kind) {
    case flume::ViewKind::Text:
      view = [FLMTextView new];
      break;
    case flume::ViewKind::Image:
      view = [FLMImageView new];
      break;
    default:
      view = [FLMBoxView new];
  }
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
  UIView *plain = FLMUIView(handle);
  if (![plain conformsToProtocol:@protocol(FLMShaped)]) {
    return;
  }
  UIView<FLMShaped> *view = (UIView<FLMShaped> *)plain;
  CALayer *layer = view.layer;
  view.backgroundColor = style.backgroundColor == 0 ? nil : FLMColor(style.backgroundColor);
  view.alpha = style.opacity;
  layer.borderWidth = style.borderWidth;
  layer.borderColor = style.borderWidth > 0 ? FLMColor(style.borderColor).CGColor : nil;
  FLMSetRadii(view, style.radii);
  // React Native's iOS shadow: the color's alpha scales shadowOpacity.
  const CGFloat shadowAlpha = ((style.shadowColor >> 24) & 0xFF) / 255.0;
  layer.shadowColor = [FLMColor(style.shadowColor) colorWithAlphaComponent:1].CGColor;
  layer.shadowOpacity = shadowAlpha * style.shadowOpacity;
  layer.shadowRadius = style.shadowRadius;
  layer.shadowOffset = CGSizeMake(style.shadowOffsetX, style.shadowOffsetY);
  view.opaqueShadow = ((style.backgroundColor >> 24) & 0xFF) == 0xFF;
  [view setNeedsLayout];
}

void FLMCellViews::text(flume::ViewHandle handle, const flume::TextSpec &text, float insetLeft, float insetTop)
{
  UIView *view = FLMUIView(handle);
  if (![view isKindOfClass:FLMTextView.class]) {
    return;
  }
  [(FLMTextView *)view setText:FLMAttributedString(text.text, FLMTextStyleOf(text))
                         lines:text.numberOfLines
                      maxWidth:text.maxWidth
                         inset:CGPointMake(insetLeft, insetTop)];
}

void FLMCellViews::image(flume::ViewHandle handle, const flume::ImageSpec &image)
{
  UIView *view = FLMUIView(handle);
  if ([view isKindOfClass:FLMImageView.class]) {
    [(FLMImageView *)view setSpec:image];
  }
}

void FLMCellViews::pressable(flume::ViewHandle, const std::string &, float) {}

void FLMCellViews::accessibility(flume::ViewHandle, const std::optional<std::string> &,
                                 const std::optional<std::string> &, int, int)
{
}
