#include "doctest.h"

#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "core/MeasureCache.h"

TEST_CASE("the measure cache tells apart requests that differ in any field") {
  std::string text = "Hello there";
  flume::TextMeasureRequest base{text, 14, 400, 300, 0, "Inter", false, 0, 0, 0, 0, true};
  flume::MeasureCache cache;
  cache.put(base, {120, 17});

  std::string copy = text;
  flume::TextMeasureRequest same = base;
  same.text = copy;
  REQUIRE(cache.find(same) != nullptr);
  CHECK(cache.find(same)->width == 120);

  std::vector<std::function<void(flume::TextMeasureRequest&)>> changes = {
      [](auto& r) { r.text = "Hello there!"; },     [](auto& r) { r.fontSize = 15; },
      [](auto& r) { r.fontWeight = 700; },          [](auto& r) { r.maxWidth = 299.5f; },
      [](auto& r) { r.numberOfLines = 2; },         [](auto& r) { r.fontFamily = "Inter-Bold"; },
      [](auto& r) { r.italic = true; },             [](auto& r) { r.lineHeight = 20; },
      [](auto& r) { r.letterSpacing = 0.5f; },      [](auto& r) { r.textAlign = 3; },
      [](auto& r) { r.decoration = 1; },            [](auto& r) { r.allowFontScaling = false; },
  };
  for (size_t i = 0; i < changes.size(); i++) {
    flume::TextMeasureRequest changed = base;
    changes[i](changed);
    CAPTURE(i);
    CHECK(cache.find(changed) == nullptr);
  }

  flume::TextMeasureRequest nan = base;
  nan.fontSize = NAN;
  cache.put(nan, {10, 10});
  CHECK(cache.find(nan) != nullptr);
}
