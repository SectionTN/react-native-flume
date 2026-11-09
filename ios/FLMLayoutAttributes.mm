#import "FLMLayoutAttributes.h"

@implementation FLMLayoutAttributes

- (id)copyWithZone:(NSZone *)zone
{
  FLMLayoutAttributes *copy = [super copyWithZone:zone];
  copy.separatorFrame = _separatorFrame;
  copy.separatorColor = _separatorColor;
  return copy;
}

// UIKit skips attributes that compare equal, so a row that stops being the last must compare different.
- (BOOL)isEqual:(id)object
{
  if (![object isKindOfClass:FLMLayoutAttributes.class] || ![super isEqual:object]) {
    return NO;
  }
  FLMLayoutAttributes *other = object;
  return CGRectEqualToRect(_separatorFrame, other.separatorFrame) &&
      (_separatorColor == other.separatorColor || [_separatorColor isEqual:other.separatorColor]);
}

@end
