#import "FLMBoxView.h"

// Draws a cell text from the same TextKit stack its measure used; the inset is the node's padding plus border.
@interface FLMTextView : FLMBoxView

- (void)setText:(NSAttributedString *)text lines:(int)lines maxWidth:(CGFloat)maxWidth inset:(CGPoint)inset;

@end
