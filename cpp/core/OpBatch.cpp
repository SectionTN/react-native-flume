#include "core/OpBatch.h"

namespace flume {

void OpBatch::create(int id, ViewKind kind, int parentId, int index) {
  ints.insert(ints.end(), {static_cast<int32_t>(Op::Create), id, static_cast<int32_t>(kind), parentId, index});
  count_++;
}

void OpBatch::remove(int id) {
  ints.insert(ints.end(), {static_cast<int32_t>(Op::Remove), id});
  count_++;
}

void OpBatch::frame(int id, float x, float y, float w, float h) {
  ints.insert(ints.end(), {static_cast<int32_t>(Op::Frame), id});
  floats.insert(floats.end(), {x, y, w, h});
  count_++;
}

void OpBatch::viewStyle(int id, uint32_t backgroundColor, float opacity, float borderRadius) {
  ints.insert(ints.end(), {static_cast<int32_t>(Op::ViewStyle), id, static_cast<int32_t>(backgroundColor)});
  floats.insert(floats.end(), {opacity, borderRadius});
  count_++;
}

void OpBatch::text(int id, const std::string& text, uint32_t color, int fontWeight, int numberOfLines,
                   float fontSize, float maxWidth) {
  ints.insert(ints.end(), {static_cast<int32_t>(Op::Text), id, addString(text), static_cast<int32_t>(color),
                           fontWeight, numberOfLines});
  floats.insert(floats.end(), {fontSize, maxWidth});
  count_++;
}

void OpBatch::image(int id, const std::string& uri, float width, float height, float borderRadius) {
  ints.insert(ints.end(), {static_cast<int32_t>(Op::Image), id, addString(uri)});
  floats.insert(floats.end(), {width, height, borderRadius});
  count_++;
}

void OpBatch::clear() {
  ints.clear();
  floats.clear();
  strings.clear();
  rootWidth = 0;
  rootHeight = 0;
  stats.fill(0);
  count_ = 0;
}

int32_t OpBatch::addString(const std::string& s) {
  strings.push_back(s);
  return static_cast<int32_t>(strings.size() - 1);
}

}  // namespace flume
