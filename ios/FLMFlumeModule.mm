#import "FLMFlumeModule.h"

#import "FLMBindStats.h"
#import "FLMImages.h"

#include "bindings/Engine.h"
#include "bindings/FlumeHost.h"

@implementation FLMFlumeModule

RCT_EXPORT_MODULE(NativeFlume)

@synthesize moduleRegistry = _moduleRegistry;

+ (BOOL)requiresMainQueueSetup
{
  return NO;
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
  return std::make_shared<facebook::react::NativeFlumeSpecJSI>(params);
}

- (void)installJSIBindingsWithRuntime:(facebook::jsi::Runtime &)runtime
                          callInvoker:(const std::shared_ptr<facebook::react::CallInvoker> &)callInvoker
{
  flume::Engine::instance().setCallInvoker(callInvoker);
  flume::FlumeHost::install(runtime);
  FLMImages.shared.loader = [self.moduleRegistry moduleForName:"ImageLoader"];
}

// Binds touch the stats rings on the main thread.
- (void)setStats:(BOOL)enabled
{
  dispatch_async(dispatch_get_main_queue(), ^{
    flume::Engine::instance().setStatsEnabled(enabled);
    FLMBindStats.enabled = enabled;
  });
}

- (void)logStats
{
  dispatch_async(dispatch_get_main_queue(), ^{
    [FLMBindStats log];
  });
}

@end
