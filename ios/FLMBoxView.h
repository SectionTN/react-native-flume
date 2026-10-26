#import <UIKit/UIKit.h>

#include <array>

// A cell view that keeps its corner mask and shadow path in step with its bounds.
@interface FLMBoxView : UIView
@property (nonatomic) std::array<float, 4> radii;  // topLeft, topRight, bottomRight, bottomLeft
@property (nonatomic) BOOL opaqueShadow;
@end
