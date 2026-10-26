#include "core/OpBatch.h"

namespace flume {

namespace {

int32_t code(Op op) { return static_cast<int32_t>(op); }
int32_t color(std::optional<uint32_t> c, uint32_t fallback) { return static_cast<int32_t>(c.value_or(fallback)); }
float radius(const Style& s, size_t corner) { return s.borderRadii[corner].value_or(0); }

}  // namespace

void OpBatch::create(int id, ViewKind kind, int parentId, int index) {
  ints.insert(ints.end(), {code(Op::Create), id, static_cast<int32_t>(kind), parentId, index});
  count_++;
}

void OpBatch::remove(int id) {
  ints.insert(ints.end(), {code(Op::Remove), id});
  count_++;
}

void OpBatch::frame(int id, float x, float y, float w, float h) {
  ints.insert(ints.end(), {code(Op::Frame), id});
  floats.insert(floats.end(), {x, y, w, h});
  count_++;
}

void OpBatch::viewStyle(int id, const Style& s) {
  ints.insert(ints.end(), {code(Op::ViewStyle), id, color(s.backgroundColor, 0), color(s.borderColor, 0xFF000000u),
                           color(s.shadowColor, 0xFF000000u)});
  const auto offset = s.shadowOffset.value_or(std::pair<float, float>{0, -3});
  floats.insert(floats.end(), {s.opacity.value_or(1), radius(s, 0), radius(s, 1), radius(s, 2), radius(s, 3),
                               s.borderWidth.value_or(0), s.elevation.value_or(0), s.shadowOpacity.value_or(0),
                               s.shadowRadius.value_or(3), offset.first, offset.second});
  count_++;
}

void OpBatch::text(int id, const std::string& text, const Style& s, float maxWidth, bool allowFontScaling,
                   float insetLeft, float insetTop) {
  ints.insert(ints.end(), {code(Op::Text), id, addString(text), color(s.color, 0xFF000000u), s.fontWeight, s.numberOfLines,
                           addStringOrNone(s.fontFamily), s.italic ? 1 : 0, static_cast<int32_t>(s.textAlign),
                           static_cast<int32_t>(s.decoration), allowFontScaling ? 1 : 0});
  floats.insert(floats.end(),
                {s.fontSize, maxWidth, s.lineHeight.value_or(0), s.letterSpacing.value_or(0), insetLeft, insetTop});
  count_++;
}

void OpBatch::image(int id, const std::string& uri, const Style& s, ResizeMode mode, std::optional<uint32_t> tint,
                    const Headers& headers) {
  ints.insert(ints.end(), {code(Op::Image), id, addString(uri), static_cast<int32_t>(mode), tint ? 1 : 0, color(tint, 0),
                           static_cast<int32_t>(headers.size())});
  for (const auto& [key, value] : headers) ints.insert(ints.end(), {addString(key), addString(value)});
  floats.insert(floats.end(), {s.width.pointsOr(0), s.height.pointsOr(0), radius(s, 0), radius(s, 1), radius(s, 2),
                               radius(s, 3)});
  count_++;
}

void OpBatch::pressable(int id, const std::string& pressId, float pressedOpacity) {
  ints.insert(ints.end(), {code(Op::Pressable), id, addString(pressId)});
  floats.push_back(pressedOpacity);
  count_++;
}

void OpBatch::accessibility(int id, const std::string& label, const std::string& testID, int role,
                            std::optional<bool> accessible) {
  ints.insert(ints.end(), {code(Op::Accessibility), id, addStringOrNone(label), addStringOrNone(testID), role,
                           accessible ? (*accessible ? 1 : 0) : -1});
  count_++;
}

void OpBatch::move(int id, int index) {
  ints.insert(ints.end(), {code(Op::Move), id, index});
  count_++;
}

void OpBatch::clear() {
  ints.clear();
  floats.clear();
  strings.clear();
  rootWidth = 0;
  rootHeight = 0;
  first = false;
  timed = false;
  stats.fill(0);
  count_ = 0;
}

int32_t OpBatch::addString(const std::string& s) {
  strings.push_back(s);
  return static_cast<int32_t>(strings.size() - 1);
}

int32_t OpBatch::addStringOrNone(const std::string& s) { return s.empty() ? -1 : addString(s); }

}  // namespace flume
