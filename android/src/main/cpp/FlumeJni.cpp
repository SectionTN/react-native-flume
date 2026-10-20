#include <ReactCommon/BindingsInstallerHolder.h>
#include <ReactCommon/CallInvoker.h>
#include <fbjni/fbjni.h>
#include <jsi/jsi.h>

#include <memory>

#include "bindings/FlumeHost.h"

namespace facebook::react {

// Static natives of me.sectiontn.flume.FlumeJni.
struct JFlumeJni : jni::JavaClass<JFlumeJni> {
  static constexpr auto kJavaDescriptor = "Lme/sectiontn/flume/FlumeJni;";

  static jni::local_ref<BindingsInstallerHolder::javaobject> createBindingsInstaller(jni::alias_ref<jclass>) {
    return BindingsInstallerHolder::newObjectCxxArgs(
        [](jsi::Runtime& runtime, const std::shared_ptr<CallInvoker>&) { flume::FlumeHost::install(runtime); });
  }

  static void registerNatives() {
    javaClassStatic()->registerNatives({
        makeNativeMethod("createBindingsInstaller", JFlumeJni::createBindingsInstaller),
    });
  }
};

}  // namespace facebook::react

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::jni::initialize(vm, [] { facebook::react::JFlumeJni::registerNatives(); });
}
