#pragma once

#include <fbjni/fbjni.h>

#include <cstring>
#include <string>

#include "core/TextMeasurer.h"

namespace flume::android {

struct JFlumeTextMeasurer : facebook::jni::JavaClass<JFlumeTextMeasurer> {
  static constexpr auto kJavaDescriptor = "Lme/sectiontn/flume/FlumeTextMeasurer;";

  // Width float bits in the high 32 bits, height in the low 32, both dp.
  jlong measure(const TextMeasureRequest& r) const {
    static const auto method =
        javaClassStatic()
            ->getMethod<jlong(jstring, jfloat, jint, jfloat, jint, jstring, jboolean, jfloat, jfloat, jint, jint)>("measure");
    auto text = facebook::jni::make_jstring(std::string(r.text));
    auto family = facebook::jni::make_jstring(std::string(r.fontFamily));
    return method(self(), text.get(), r.fontSize, r.fontWeight, r.maxWidth, r.numberOfLines, family.get(), r.italic,
                  r.lineHeight, r.letterSpacing, r.textAlign, r.decoration);
  }
};

class AndroidTextMeasurer : public TextMeasurer {
 public:
  explicit AndroidTextMeasurer(facebook::jni::global_ref<JFlumeTextMeasurer> measurer) : measurer_(std::move(measurer)) {}

  TextMetrics measure(const TextMeasureRequest& request) override {
    jlong packed = measurer_->measure(request);
    uint32_t hi = static_cast<uint32_t>(static_cast<uint64_t>(packed) >> 32);
    uint32_t lo = static_cast<uint32_t>(static_cast<uint64_t>(packed) & 0xffffffffu);
    TextMetrics metrics{};
    std::memcpy(&metrics.width, &hi, sizeof(float));
    std::memcpy(&metrics.height, &lo, sizeof(float));
    return metrics;
  }

 private:
  facebook::jni::global_ref<JFlumeTextMeasurer> measurer_;
};

}  // namespace flume::android
