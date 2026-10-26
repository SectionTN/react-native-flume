#import <UIKit/UIKit.h>

#include <cstdint>

// The core's colors are ARGB, as React Native's processColor gives them.
inline UIColor *FLMColor(uint32_t argb)
{
  return [UIColor colorWithRed:((argb >> 16) & 0xFF) / 255.0
                         green:((argb >> 8) & 0xFF) / 255.0
                          blue:(argb & 0xFF) / 255.0
                         alpha:((argb >> 24) & 0xFF) / 255.0];
}
