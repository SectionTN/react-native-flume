#import <UIKit/UIKit.h>

// The list's collection view: reports each layout pass, scrolling included.
@interface FLMCollectionView : UICollectionView

@property (nonatomic, copy, nullable) void (^didLayout)(void);

@end
