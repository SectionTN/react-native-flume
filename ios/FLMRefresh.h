#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// The list's UIRefreshControl, driven as React Native's RCTPullToRefreshViewComponentView drives its own.
@interface FLMRefresh : NSObject

- (instancetype)initWithScrollView:(UIScrollView *)scrollView onRefresh:(void (^)(void))onRefresh;

@property (nonatomic) BOOL enabled;  // a new control each time it turns on: a reattached control stops firing on iOS 27
@property (nonatomic, strong, nullable) UIColor *tintColor;
@property (nonatomic) CGFloat progressViewOffset;

- (void)setRefreshing:(BOOL)refreshing;  // a begin before the first layout waits for layoutSubviews
- (void)layoutSubviews;

@end

NS_ASSUME_NONNULL_END
