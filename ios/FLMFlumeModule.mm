#import "FLMFlumeModule.h"

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

- (void)setStats:(BOOL)enabled
{
  flume::Engine::instance().setStatsEnabled(enabled);
}

- (void)logStats
{
}

@end
