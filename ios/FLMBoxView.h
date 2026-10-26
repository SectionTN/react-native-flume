#import <UIKit/UIKit.h>

#include <array>

// A cell view with corner radii and a shadow that follow its bounds.
@protocol FLMShaped <NSObject>
@property (nonatomic) std::array<float, 4> radii;  // topLeft, topRight, bottomRight, bottomLeft
@property (nonatomic) BOOL opaqueShadow;
@end

// One radius goes through the layer's cornerRadius and maskedCorners; differing ones through a mask.
void FLMSetRadii(UIView<FLMShaped> *view, const std::array<float, 4> &radii);

// Refits the corner mask and the shadow path to the bounds; called from layoutSubviews.
void FLMLayoutShape(UIView<FLMShaped> *view);

@interface FLMBoxView : UIView <FLMShaped>
@property (nonatomic) std::array<float, 4> radii;
@property (nonatomic) BOOL opaqueShadow;
@end
