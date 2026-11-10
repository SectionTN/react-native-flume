#import "FLMSlots.h"

#include <cmath>
#include <utility>
#include <vector>

using namespace facebook;

@implementation FLMSlots {
  UIView *_bay;
  NSMutableArray<UIView *> *_children;  // Fabric addresses its children by index here
  NSMutableDictionary<NSNumber *, FLMSlotView *> *_bySlot;
  flume::SlotPool _pool;
  BOOL _registered;
  CGFloat _sentCellWidth;
}

- (instancetype)initWithBay:(UIView *)bay
{
  if (self = [super init]) {
    _bay = bay;
    _children = [NSMutableArray array];
    _bySlot = [NSMutableDictionary dictionary];
    _sentCellWidth = NAN;
  }
  return self;
}

- (flume::SlotPool &)pool
{
  return _pool;
}

- (void)insertChild:(UIView *)child atIndex:(NSInteger)index
{
  [_children insertObject:child atIndex:std::min<NSUInteger>(std::max<NSInteger>(0, index), _children.count)];
  [child removeFromSuperview];
  if (![child isKindOfClass:FLMSlotView.class]) {
    [_bay addSubview:child];
    return;
  }
  FLMSlotView *view = (FLMSlotView *)child;
  view.host = _viewHost;
  _bySlot[@(view.slot)] = view;
  // A negative id is a slot JS rendered ahead for a row native has not bound yet.
  if (view.slot < 0) {
    _pool.registerPrerendered(view.slot, view.kind, view.itemKey.UTF8String);
    _registered = YES;
  }
  [_bay addSubview:view];
}

- (void)removeChild:(UIView *)child atIndex:(NSInteger)index
{
  if (index >= 0 && index < static_cast<NSInteger>(_children.count) && _children[index] == child) {
    [_children removeObjectAtIndex:index];
  } else {
    [_children removeObjectIdenticalTo:child];
  }
  if ([child isKindOfClass:FLMSlotView.class]) {
    FLMSlotView *view = (FLMSlotView *)child;
    view.host = nil;
    if (_bySlot[@(view.slot)] == view) {
      [_bySlot removeObjectForKey:@(view.slot)];
    }
  }
  [child removeFromSuperview];
}

- (nullable FLMSlotView *)viewForSlot:(int)slot
{
  return _bySlot[@(slot)];
}

- (BOOL)takeRegisteredPrerendered
{
  const BOOL registered = _registered;
  _registered = NO;
  return registered;
}

- (void)flushTo:(const react::FlumeViewEventEmitter &)emitter cellWidth:(CGFloat)cellWidth
{
  if (!_pool.dirty() && cellWidth == _sentCellWidth) {
    return;
  }
  _sentCellWidth = cellWidth;
  std::vector<flume::SlotEntry> entries = _pool.snapshot();
  const int jsSeen = _pool.jsSeen();
  const double width = cellWidth;
  emitter.dispatchUniqueEvent("slotsChanged", [entries = std::move(entries), jsSeen, width](jsi::Runtime &runtime) {
    auto slots = jsi::Array(runtime, entries.size());
    for (size_t i = 0; i < entries.size(); i++) {
      const flume::SlotEntry &entry = entries[i];
      auto object = jsi::Object(runtime);
      object.setProperty(runtime, "slot", entry.slot);
      object.setProperty(runtime, "kind", entry.kind);
      object.setProperty(runtime, "key", jsi::String::createFromUtf8(runtime, entry.key));
      object.setProperty(runtime, "index", entry.index);
      object.setProperty(runtime, "bound", entry.bound);
      object.setProperty(runtime, "minMain", static_cast<double>(entry.minMain));
      slots.setValueAtIndex(runtime, i, object);
    }
    auto payload = jsi::Object(runtime);
    payload.setProperty(runtime, "slots", slots);
    payload.setProperty(runtime, "jsSeen", jsSeen);
    payload.setProperty(runtime, "cellWidth", width);
    return payload;
  });
}

@end
