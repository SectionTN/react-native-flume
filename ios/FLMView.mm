#import "FLMView.h"

#import <react/renderer/components/FlumeSpec/ComponentDescriptors.h>
#import <react/renderer/components/FlumeSpec/Props.h>
#import <react/renderer/components/FlumeSpec/RCTComponentViewHelpers.h>

using namespace facebook::react;

@interface FLMView () <RCTFlumeViewViewProtocol>
@end

@implementation FLMView

+ (ComponentDescriptorProvider)componentDescriptorProvider
{
  return concreteComponentDescriptorProvider<FlumeViewComponentDescriptor>();
}

// A list owns a store and the cells bound to it, so it is never reused for another list.
+ (BOOL)shouldBeRecycled
{
  return NO;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const FlumeViewProps>();
    _props = defaultProps;
  }
  return self;
}

- (void)handleCommand:(const NSString *)commandName args:(const NSArray *)args
{
  RCTFlumeViewHandleCommand(self, commandName, args);
}

// The scroll commands do nothing on iOS yet.
- (void)scrollToIndex:(NSInteger)index animated:(BOOL)animated viewPosition:(double)viewPosition viewOffset:(double)viewOffset
{
}

- (void)scrollToOffset:(double)offset animated:(BOOL)animated
{
}

- (void)scrollToEnd:(BOOL)animated
{
}

@end
