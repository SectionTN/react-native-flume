#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/Style.h"

namespace flume {

enum class Op : int32_t {
  Create = 1,
  Remove = 2,
  Frame = 3,
  ViewStyle = 4,
  Text = 5,
  Image = 6,
  Pressable = 7,
  Accessibility = 8,
  Move = 9
};
enum class ViewKind : int32_t { View = 0, Text = 1, Image = 2, Pressable = 3 };
using Headers = std::vector<std::pair<std::string, std::string>>;

// The view operations of one bind as typed arrays, so a platform applies them in one call. Units are dp.
// Create 1: ints [1, id, kind, parentId, index]
// Remove 2: ints [2, id]
// Frame 3: ints [3, id]; floats [x, y, w, h]
// ViewStyle 4: ints [4, id, backgroundColor, borderColor]; floats [opacity, rTL, rTR, rBR, rBL, borderWidth, elevation]
// Text 5: ints [5, id, textIdx, color, weight, lines, familyIdx (-1 none), italic, align, decoration, fontScaling]; floats [fontSize, maxWidth, lineHeight (0 none), letterSpacing]
// Image 6: ints [6, id, uriIdx, resizeMode, hasTint, tint, headerCount, (keyIdx, valueIdx)...]; floats [w, h, rTL, rTR, rBR, rBL]
// Pressable 7: ints [7, id, pressIdIdx]; floats [pressedOpacity]
// Accessibility 8: ints [8, id, labelIdx (-1 none), testIdIdx (-1 none), role (0 none, 1 button), accessible (-1 unset, 0, 1)]
// Move 9: ints [9, id, index]
struct OpBatch {
  std::vector<int32_t> ints;
  std::vector<float> floats;
  std::vector<std::string> strings;
  float rootWidth = 0;
  float rootHeight = 0;
  bool first = false;
  bool timed = false;  // stats are filled only while the engine's stats are on
  std::array<int64_t, 6> stats{};  // workletNs, reconcileNs, layoutNs, measureCalls, measureHits, first

  void create(int id, ViewKind kind, int parentId, int index);
  void remove(int id);
  void frame(int id, float x, float y, float w, float h);
  void viewStyle(int id, const Style& s);
  void text(int id, const std::string& text, const Style& s, float maxWidth, bool allowFontScaling);
  void image(int id, const std::string& uri, const Style& s, ResizeMode mode, std::optional<uint32_t> tint,
             const Headers& headers);
  void pressable(int id, const std::string& pressId, float pressedOpacity);
  void accessibility(int id, const std::string& label, const std::string& testID, int role, std::optional<bool> accessible);
  void move(int id, int index);
  int opCount() const { return count_; }
  void clear();

 private:
  int32_t addString(const std::string& s);
  int32_t addStringOrNone(const std::string& s);  // -1 for an empty string
  int count_ = 0;
};

}  // namespace flume
