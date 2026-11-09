#import "FLMCollectionView.h"

@implementation FLMCollectionView

- (void)layoutSubviews
{
  [super layoutSubviews];
  if (_didLayout != nil) {
    _didLayout();
  }
}

@end
