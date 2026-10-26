#import "FLMFlumeModule.h"

#include "bindings/Engine.h"
#include "bindings/FlumeHost.h"

@implementation FLMFlumeModule

RCT_EXPORT_MODULE(NativeFlume)

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
}

- (void)setStats:(BOOL)enabled
{
  flume::Engine::instance().setStatsEnabled(enabled);
}

- (void)logStats
{
}

@end
