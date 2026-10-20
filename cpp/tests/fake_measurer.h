#pragma once

#include <algorithm>
#include <cmath>

#include "core/TextMeasurer.h"

// Every character is fontSize / 2 wide and a line is fontSize + 4 high, so frames are integral.
struct FakeMeasurer : flume::TextMeasurer {
  int calls = 0;

  flume::TextMetrics measure(const flume::TextMeasureRequest& request) override {
    calls++;
    float textWidth = static_cast<float>(request.text.size()) * request.fontSize / 2;
    float lines = std::max(1.0f, std::ceil(textWidth / request.maxWidth));
    return {std::min(request.maxWidth, textWidth), lines * (request.fontSize + 4)};
  }
};
