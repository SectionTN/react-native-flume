#import <UIKit/UIKit.h>

#import "FLMBoxView.h"

#include "adapter/CellTree.h"

// A cell image: loads its URI, shows only the image it asked for last, inside its padding and border.
@interface FLMImageView : UIView <FLMShaped>

@property (nonatomic) std::array<float, 4> radii;
@property (nonatomic) BOOL opaqueShadow;

- (void)setSpec:(const flume::ImageSpec &)spec;
- (void)setInsets:(UIEdgeInsets)insets;

@end
