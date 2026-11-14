#import "FLMSlots.h"

#include <cmath>
#include <utility>
#include <vector>

using namespace facebook;

@implementation FLMSlots {
  UIView *_bay;
  NSMutableArray<UIView *> *_children;  // Fabric addresses its children by index here
  NSMutableDictionary<NSNumber *, FLMSlotView *> *_bySlot;
  NSMapTable<NSNumber *, FLMSlotCell *> *_cells;  // the cell bound to each slot
  flume::SlotPool _pool;
  BOOL _registered;
  CGFloat _sentCellWidth;
  CGFloat _sentWidth;
}

- (instancetype)initWithBay:(UIView *)bay
{
  if (self = [super init]) {
    _bay = bay;
    _children = [NSMutableArray array];
    _bySlot = [NSMutableDictionary dictionary];
    _cells = [NSMapTable strongToWeakObjectsMapTable];
    _sentCellWidth = NAN;
    _sentWidth = NAN;
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
    [self adoptForWaitingCell:view];
  }
  FLMSlotCell *cell = [_cells objectForKey:@(view.slot)];
  if (cell != nil) {
    [cell show:view];
  } else {
    [_bay addSubview:view];
  }
  if (self.onRendered != nil) {
    self.onRendered(view.slot);
  }
}

// A pre-rendered view takes the place of a slot its key still waits on, so the cell shows it with no blank frame.
- (void)adoptForWaitingCell:(FLMSlotView *)view
{
  const std::string key = view.itemKey.UTF8String;
  for (FLMSlotCell *cell in _cells.objectEnumerator) {
    const std::optional<flume::SlotEntry> waiting = _pool.entry(cell.slot);
    if (cell.slot <= 0 || _bySlot[@(cell.slot)] != nil || !waiting || waiting->kind != view.kind || waiting->key != key) {
      continue;
    }
    const int old = cell.slot;
    if (_pool.replace(cell.owner, old, view.slot)) {
      [_cells removeObjectForKey:@(old)];
      cell.slot = view.slot;
      [_cells setObject:cell forKey:@(view.slot)];
    }
    return;
  }
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
    FLMSlotCell *cell = [_cells objectForKey:@(view.slot)];
    if (cell.view == view) {
      [cell show:nil];
    }
  }
  [child removeFromSuperview];
}

- (nullable FLMSlotView *)viewForSlot:(int)slot
{
  return _bySlot[@(slot)];
}

- (nullable FLMSlotCell *)cellForSlot:(int)slot
{
  return [_cells objectForKey:@(slot)];
}

- (BOOL)takeRegisteredPrerendered
{
  const BOOL registered = _registered;
  _registered = NO;
  return registered;
}

- (void)bindCell:(FLMSlotCell *)cell group:(flume::SlotGroup)group kind:(int)kind key:(NSString *)key index:(int)index
{
  const std::string itemKey = key.UTF8String;
  const std::optional<flume::SlotEntry> shown = cell.slot != 0 ? _pool.entry(cell.slot) : std::nullopt;
  if (cell.slot != 0 && (!shown || shown->key != itemKey || shown->kind != kind)) {
    [self releaseCell:cell];
  }
  const int slot = _pool.acquire(cell.owner, group, kind, itemKey, index);
  cell.slot = slot;
  [_cells setObject:cell forKey:@(slot)];
  [cell show:_bySlot[@(slot)]];
}

- (void)releaseCell:(FLMSlotCell *)cell
{
  const int slot = cell.slot;
  if (slot == 0) {
    return;
  }
  _pool.release(cell.owner, slot);
  if ([_cells objectForKey:@(slot)] == cell) {
    [_cells removeObjectForKey:@(slot)];
  }
  [self park:cell];
  cell.slot = 0;
}

// The view goes back to the bay only from the cell that still holds it, since another cell may have taken it.
- (void)park:(FLMSlotCell *)cell
{
  FLMSlotView *view = cell.view;
  if (view != nil && view.superview == cell.contentView) {
    [_bay addSubview:view];
  }
  [cell show:nil];
}

- (void)reindexCell:(FLMSlotCell *)cell index:(int)index
{
  if (cell.slot != 0 && index >= 0) {
    _pool.setIndex(cell.slot, index);
  }
}

- (void)detachAll
{
  for (FLMSlotCell *cell in [self cells]) {
    _pool.lend(cell.owner);
    [self park:cell];
    cell.slot = 0;
  }
  [_cells removeAllObjects];
}

- (NSArray<FLMSlotCell *> *)cells
{
  return _cells.objectEnumerator.allObjects;
}

- (void)flushTo:(const react::FlumeViewEventEmitter &)emitter cellWidth:(CGFloat)cellWidth width:(CGFloat)width
{
  if (!_pool.dirty() && cellWidth == _sentCellWidth && width == _sentWidth) {
    return;
  }
  _sentCellWidth = cellWidth;
  _sentWidth = width;
  std::vector<flume::SlotEntry> entries = _pool.snapshot();
  const int jsSeen = _pool.jsSeen();
  const double column = cellWidth;
  const double listWidth = width;
  emitter.dispatchUniqueEvent("slotsChanged", [entries = std::move(entries), jsSeen, column, listWidth](jsi::Runtime &runtime) {
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
    payload.setProperty(runtime, "cellWidth", column);
    payload.setProperty(runtime, "width", listWidth);
    return payload;
  });
}

@end
