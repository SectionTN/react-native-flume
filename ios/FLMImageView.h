#import <UIKit/UIKit.h>

#import "FLMBoxView.h"

#include "adapter/CellTree.h"

// A cell image: loads its URI, and shows only the image it asked for last.
@interface FLMImageView : UIImageView <FLMShaped>

@property (nonatomic) std::array<float, 4> radii;
@property (nonatomic) BOOL opaqueShadow;

- (void)setSpec:(const flume::ImageSpec &)spec;

@end
