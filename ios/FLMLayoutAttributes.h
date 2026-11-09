#import <UIKit/UIKit.h>

// A cell's attributes with its separator, which the cell draws in the gap the layout reserves after its row.
@interface FLMLayoutAttributes : UICollectionViewLayoutAttributes

@property (nonatomic) CGRect separatorFrame;  // in the cell's coordinates; empty for none
@property (nonatomic, strong, nullable) UIColor *separatorColor;

@end
