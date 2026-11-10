#import "FLMSlotView.h"

#include "fabric/FlumeComponentDescriptors.h"
#import <react/renderer/components/FlumeSpec/Props.h>

using namespace facebook::react;

@implementation FLMSlotView

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<FlumeSlotViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const FlumeSlotViewProps>();
    _props = defaultProps;
  }
  return self;
}

@end
