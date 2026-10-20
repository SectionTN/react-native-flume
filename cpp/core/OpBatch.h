#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace flume {

enum class Op : int32_t { Create = 1, Remove = 2, Frame = 3, ViewStyle = 4, Text = 5, Image = 6 };
enum class ViewKind : int32_t { View = 0, Text = 1, Image = 2 };

// The view operations of one bind as typed arrays, so a platform applies them in one call. Units are dp.
struct OpBatch {
  std::vector<int32_t> ints;
  std::vector<float> floats;
  std::vector<std::string> strings;
  float rootWidth = 0;
  float rootHeight = 0;
  std::array<int64_t, 6> stats{};  // workletNs, reconcileNs, layoutNs, measureCalls, measureHits, first

  void create(int id, ViewKind kind, int parentId, int index);
  void remove(int id);
  void frame(int id, float x, float y, float w, float h);
  void viewStyle(int id, uint32_t backgroundColor, float opacity, float borderRadius);
  void text(int id, const std::string& text, uint32_t color, int fontWeight, int numberOfLines, float fontSize,
            float maxWidth);
  void image(int id, const std::string& uri, float width, float height, float borderRadius);
  int opCount() const { return count_; }
  void clear();

 private:
  int32_t addString(const std::string& s);
  int count_ = 0;
};

}  // namespace flume
