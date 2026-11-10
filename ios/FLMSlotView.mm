#import "FLMSlotView.h"

#include "fabric/FlumeComponentDescriptors.h"
#import <react/renderer/components/FlumeSpec/Props.h>

using namespace facebook::react;

@implementation FLMSlotView {
  FlumeSlotViewShadowNode::ConcreteState::Shared _state;
  CGPoint _writtenOrigin;
  BOOL _written;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<FlumeSlotViewComponentDescriptor>();
}

// A slot moves between cells and the bay, so the registry must never pool one that still sits in a cell.
+ (BOOL)shouldBeRecycled
{
  return NO;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const FlumeSlotViewProps>();
    _props = defaultProps;
    _itemKey = @"";
  }
  return self;
}

- (void)updateProps:(const Props::Shared &)props oldProps:(const Props::Shared &)oldProps
{
  const auto &next = *std::static_pointer_cast<const FlumeSlotViewProps>(props);
  _slot = next.slot;
  _kind = next.kind;
  _itemKey = [NSString stringWithUTF8String:next.itemKey.c_str()] ?: @"";
  [super updateProps:props oldProps:oldProps];
}

- (void)updateLayoutMetrics:(const LayoutMetrics &)layoutMetrics oldLayoutMetrics:(const LayoutMetrics &)oldLayoutMetrics
{
  [super updateLayoutMetrics:layoutMetrics oldLayoutMetrics:oldLayoutMetrics];
  // Fabric places the view at its Yoga origin in whatever superview it has, and a cell shows it at zero.
  const CGSize size = self.bounds.size;
  self.center = CGPointMake(size.width / 2, size.height / 2);
  if (!CGSizeEqualToSize(size, _layoutSize)) {
    _layoutSize = size;
    [self.host slotViewDidResize:self];
  }
}

- (void)updateState:(const State::Shared &)state oldState:(const State::Shared &)oldState
{
  _state = std::static_pointer_cast<const FlumeSlotViewShadowNode::ConcreteState>(state);
}

- (void)writeOrigin:(CGPoint)origin
{
  if (_state == nullptr || (_written && CGPointEqualToPoint(origin, _writtenOrigin))) {
    return;
  }
  _written = YES;
  _writtenOrigin = origin;
  const facebook::react::Point point{static_cast<Float>(origin.x), static_cast<Float>(origin.y)};
  _state->updateState([point](const FlumeSlotViewState &old) -> StateData::Shared {
    if (old.placed && old.origin == point) {
      return nullptr;
    }
    auto next = std::make_shared<FlumeSlotViewState>(old);
    next->origin = point;
    next->placed = true;
    return next;
  });
}

@end
