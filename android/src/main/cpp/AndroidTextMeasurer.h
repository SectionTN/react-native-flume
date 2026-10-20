#pragma once

#include <fbjni/fbjni.h>

#include <cstring>
#include <string>

#include "core/TextMeasurer.h"

namespace flume::android {

struct JFlumeTextMeasurer : facebook::jni::JavaClass<JFlumeTextMeasurer> {
  static constexpr auto kJavaDescriptor = "Lme/sectiontn/flume/FlumeTextMeasurer;";

  // Width float bits in the high 32 bits, height in the low 32, both dp.
  jlong measure(const std::string& text, float fontSize, int fontWeight, float maxWidth, int numberOfLines) const {
    static const auto method = javaClassStatic()->getMethod<jlong(jstring, jfloat, jint, jfloat, jint)>("measure");
    return method(self(), facebook::jni::make_jstring(text).get(), fontSize, fontWeight, maxWidth, numberOfLines);
  }
};

class AndroidTextMeasurer : public TextMeasurer {
 public:
  explicit AndroidTextMeasurer(facebook::jni::global_ref<JFlumeTextMeasurer> measurer) : measurer_(std::move(measurer)) {}

  TextMetrics measure(const TextMeasureRequest& request) override {
    jlong packed = measurer_->measure(std::string(request.text), request.fontSize, request.fontWeight, request.maxWidth,
                                      request.numberOfLines);
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
