#import <FlumeSpec/FlumeSpec.h>
#import <ReactCommon/RCTTurboModuleWithJSIBindings.h>

// NativeFlume: requiring it installs the __flume host object on the React runtime.
@interface FLMFlumeModule : NSObject <NativeFlumeSpec, RCTTurboModuleWithJSIBindings>
@end
