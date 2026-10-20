#pragma once

#include <fbjni/fbjni.h>

#include "bindings/Store.h"

namespace flume::android {

struct JFlumeStoreListener : facebook::jni::JavaClass<JFlumeStoreListener> {
  static constexpr auto kJavaDescriptor = "Lme/sectiontn/flume/FlumeStoreListener;";

  void onDataChanged(int count) const {
    static const auto method = javaClassStatic()->getMethod<void(jint)>("onDataChanged");
    method(self(), count);
  }
};

class AndroidStoreListener : public StoreListener {
 public:
  explicit AndroidStoreListener(facebook::jni::global_ref<JFlumeStoreListener> listener) : listener_(std::move(listener)) {}

  void onDataChanged(int count) override {
    facebook::jni::ThreadScope scope;
    listener_->onDataChanged(count);
  }

 private:
  facebook::jni::global_ref<JFlumeStoreListener> listener_;
};

}  // namespace flume::android
