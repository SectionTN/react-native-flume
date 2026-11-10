#import <UIKit/UIKit.h>

// The list's collection view: reports each layout pass, scrolling included, and each new touch before UIKit acts on it.
@interface FLMCollectionView : UICollectionView

@property (nonatomic, copy, nullable) void (^didLayout)(void);
@property (nonatomic, copy, nullable) void (^touchDown)(void);

@end
