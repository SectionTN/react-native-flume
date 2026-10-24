#include <ReactCommon/BindingsInstallerHolder.h>
#include <ReactCommon/CallInvoker.h>
#include <fbjni/fbjni.h>
#include <jsi/jsi.h>

#include <memory>
#include <unordered_map>

#include "AndroidStoreListener.h"
#include "AndroidTextMeasurer.h"
#include "bindings/Engine.h"
#include "bindings/FlumeHost.h"
#include "core/OpBatch.h"

namespace facebook::react {

namespace jni = facebook::jni;

struct JFlumeBatch : jni::JavaClass<JFlumeBatch> {
  static constexpr auto kJavaDescriptor = "Lme/sectiontn/flume/FlumeBatch;";

  static jni::local_ref<JFlumeBatch> create(const flume::OpBatch& batch) {
    auto ints = jni::JArrayInt::newArray(batch.ints.size());
    if (!batch.ints.empty()) ints->setRegion(0, batch.ints.size(), batch.ints.data());
    auto floats = jni::JArrayFloat::newArray(batch.floats.size());
    if (!batch.floats.empty()) floats->setRegion(0, batch.floats.size(), batch.floats.data());
    auto strings = jni::JArrayClass<jni::JString::javaobject>::newArray(batch.strings.size());
    for (size_t i = 0; i < batch.strings.size(); i++) strings->setElement(i, jni::make_jstring(batch.strings[i]).get());
    jni::local_ref<jni::JArrayLong> stats;
    if (batch.timed) {
      stats = jni::JArrayLong::newArray(batch.stats.size());
      stats->setRegion(0, batch.stats.size(), batch.stats.data());
    }
    return newInstance(ints, floats, strings, batch.rootWidth, batch.rootHeight, jboolean(batch.first), stats);
  }
};

// Static natives of me.sectiontn.flume.FlumeJni; the listeners live as long as their store is attached.
struct JFlumeJni : jni::JavaClass<JFlumeJni> {
  static constexpr auto kJavaDescriptor = "Lme/sectiontn/flume/FlumeJni;";

  static jni::local_ref<BindingsInstallerHolder::javaobject> createBindingsInstaller(jni::alias_ref<jclass>) {
    return BindingsInstallerHolder::newObjectCxxArgs(
        [](jsi::Runtime& runtime, const std::shared_ptr<CallInvoker>& invoker) {
          flume::Engine::instance().setCallInvoker(invoker);
          flume::FlumeHost::install(runtime);
        });
  }

  static jint attach(jni::alias_ref<jclass>, jint storeId, jni::alias_ref<flume::android::JFlumeStoreListener> listener,
                     jni::alias_ref<flume::android::JFlumeTextMeasurer> measurer, jfloat pointScale) {
    auto& listeners = listenersByStore();
    listeners[storeId] = std::make_unique<flume::android::AndroidStoreListener>(jni::make_global(listener));
    return flume::Engine::instance().attach(
        storeId, listeners[storeId].get(),
        std::make_unique<flume::android::AndroidTextMeasurer>(jni::make_global(measurer)), pointScale);
  }

  static void detach(jni::alias_ref<jclass>, jint storeId) {
    flume::Engine::instance().detach(storeId);
    listenersByStore().erase(storeId);
  }

  static jint count(jni::alias_ref<jclass>, jint storeId) { return flume::Engine::instance().count(storeId); }

  static jint viewType(jni::alias_ref<jclass>, jint storeId, jint position) {
    return flume::Engine::instance().viewType(storeId, position);
  }

  static jlong itemId(jni::alias_ref<jclass>, jint storeId, jint position) {
    return flume::Engine::instance().itemId(storeId, position);
  }

  static jni::local_ref<jni::JString> keyAt(jni::alias_ref<jclass>, jint storeId, jint position) {
    return jni::make_jstring(flume::Engine::instance().keyAt(storeId, position));
  }

  static jni::local_ref<JFlumeBatch> bind(jni::alias_ref<jclass>, jint storeId, jint holderId, jint index,
                                          jfloat widthDp, jfloat heightDp) {
    flume::OpBatch batch;
    if (!flume::Engine::instance().bind(storeId, holderId, index, widthDp, heightDp, batch)) return nullptr;
    return JFlumeBatch::create(batch);
  }

  static void releaseHolder(jni::alias_ref<jclass>, jint storeId, jint holderId) {
    flume::Engine::instance().releaseHolder(storeId, holderId);
  }

  static void invalidateMeasures(jni::alias_ref<jclass>, jint storeId) {
    flume::Engine::instance().invalidateMeasures(storeId);
  }

  static void refresh(jni::alias_ref<jclass>, jint storeId) { flume::Engine::instance().refresh(storeId); }

  static void setStatsEnabled(jni::alias_ref<jclass>, jboolean enabled) {
    flume::Engine::instance().setStatsEnabled(enabled);
  }

  static std::unordered_map<int, std::unique_ptr<flume::android::AndroidStoreListener>>& listenersByStore() {
    static std::unordered_map<int, std::unique_ptr<flume::android::AndroidStoreListener>> listeners;
    return listeners;
  }

  static void registerNatives() {
    javaClassStatic()->registerNatives({
        makeNativeMethod("createBindingsInstaller", JFlumeJni::createBindingsInstaller),
        makeNativeMethod("attach", JFlumeJni::attach),
        makeNativeMethod("detach", JFlumeJni::detach),
        makeNativeMethod("count", JFlumeJni::count),
        makeNativeMethod("viewType", JFlumeJni::viewType),
        makeNativeMethod("itemId", JFlumeJni::itemId),
        makeNativeMethod("keyAt", JFlumeJni::keyAt),
        makeNativeMethod("bind", JFlumeJni::bind),
        makeNativeMethod("releaseHolder", JFlumeJni::releaseHolder),
        makeNativeMethod("invalidateMeasures", JFlumeJni::invalidateMeasures),
        makeNativeMethod("refresh", JFlumeJni::refresh),
        makeNativeMethod("setStatsEnabled", JFlumeJni::setStatsEnabled),
    });
  }
};

}  // namespace facebook::react

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  return facebook::jni::initialize(vm, [] { facebook::react::JFlumeJni::registerNatives(); });
}
