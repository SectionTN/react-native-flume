#import <React/RCTImageLoaderProtocol.h>
#import <UIKit/UIKit.h>

// Cell image loads through React Native's image loader, or a plain URL session when there is none, behind one decoded cache.
@interface FLMImages : NSObject

+ (instancetype)shared;

@property (nonatomic, weak) id<RCTImageLoaderProtocol> loader;

// Calls back on the main queue, in the same frame when the image is already decoded; returns a cancel block.
- (dispatch_block_t)load:(NSURLRequest *)request
                    size:(CGSize)size
              resizeMode:(RCTResizeMode)mode
              completion:(void (^)(UIImage *image))completion;

@end
