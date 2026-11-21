#pragma once

#include <fbjni/fbjni.h>

#include <vector>

#include "bindings/Store.h"

namespace flume::android {

namespace jni = facebook::jni;

inline jni::local_ref<jni::JArrayInt> intArray(const std::vector<int>& values) {
  auto array = jni::JArrayInt::newArray(values.size());
  if (!values.empty()) array->setRegion(0, values.size(), values.data());
  return array;
}

inline jni::local_ref<jni::JArrayInt> rangeArray(const std::vector<ListOps::Range>& ranges) {
  std::vector<int> flat;
  flat.reserve(ranges.size() * 2);
  for (const auto& range : ranges) flat.insert(flat.end(), {range.index, range.count});
  return intArray(flat);
}

inline jni::local_ref<jni::JArrayInt> pairArray(const std::vector<std::pair<int, int>>& pairs) {
  std::vector<int> flat;
  flat.reserve(pairs.size() * 2);
  for (const auto& [from, to] : pairs) flat.insert(flat.end(), {from, to});
  return intArray(flat);
}

struct JFlumeStoreListener : jni::JavaClass<JFlumeStoreListener> {
  static constexpr auto kJavaDescriptor = "Lme/sectiontn/flume/FlumeStoreListener;";

  // Ranges and moves travel as flat int pairs: (index, count) and (from, to), then the data's sticky indices.
  void onListChanged(const ListOps& ops, int count) const {
    static const auto method =
        javaClassStatic()
            ->getMethod<void(jboolean, jni::JArrayInt::javaobject, jni::JArrayInt::javaobject, jni::JArrayInt::javaobject,
                             jni::JArrayInt::javaobject, jint, jni::JArrayInt::javaobject)>("onListChanged");
    // A separator neighbor rebinds in place like a changed row.
    method(self(), ops.reset, rangeArray(ops.removed).get(), rangeArray(ops.inserted).get(), pairArray(ops.moved).get(),
           intArray(rebound(ops)).get(), count, intArray(ops.sticky).get());
  }
};

class AndroidStoreListener : public StoreListener {
 public:
  explicit AndroidStoreListener(jni::global_ref<JFlumeStoreListener> listener) : listener_(std::move(listener)) {}

  void onListChanged(const ListOps& ops, int count) override {
    jni::ThreadScope scope;
    listener_->onListChanged(ops, count);
  }

 private:
  jni::global_ref<JFlumeStoreListener> listener_;
};

}  // namespace flume::android
