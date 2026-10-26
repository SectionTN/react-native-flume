#import "FLMCell.h"

#import "FLMCellViews.h"

#include <memory>

#include "bindings/Engine.h"

@implementation FLMCell {
  std::unique_ptr<FLMCellViews> _views;
  std::unique_ptr<flume::CellTree> _tree;
  flume::OpBatch _batch;
  BOOL _bound;
  BOOL _horizontal;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    _views = std::make_unique<FLMCellViews>(self.contentView);
    _tree = std::make_unique<flume::CellTree>(*_views);
  }
  return self;
}

- (BOOL)bindStore:(int)storeId index:(NSInteger)index width:(CGFloat)width height:(CGFloat)height horizontal:(BOOL)horizontal
{
  _horizontal = horizontal;
  if (!flume::Engine::instance().bind(storeId, _holderId, static_cast<int>(index), width, height, _batch)) {
    return NO;
  }
  _tree->apply(_batch);
  _bound = YES;
  return YES;
}

- (UICollectionViewLayoutAttributes *)preferredLayoutAttributesFittingAttributes:
    (UICollectionViewLayoutAttributes *)layoutAttributes
{
  if (!_bound) {
    return layoutAttributes;
  }
  UICollectionViewLayoutAttributes *attributes = [layoutAttributes copy];
  CGSize size = attributes.size;
  if (_horizontal) {
    size.width = _tree->rootWidth();
  } else {
    size.height = _tree->rootHeight();
  }
  attributes.size = size;
  return attributes;
}

@end
