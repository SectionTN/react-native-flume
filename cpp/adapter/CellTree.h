#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "core/OpBatch.h"

namespace flume {

// A platform view the platform keeps alive while it is in the tree.
using ViewHandle = void*;

struct ViewStyleSpec {
  uint32_t backgroundColor = 0;
  uint32_t borderColor = 0;
  uint32_t shadowColor = 0;
  float opacity = 1;
  std::array<float, 4> radii{};  // topLeft, topRight, bottomRight, bottomLeft
  float borderWidth = 0;
  float elevation = 0;
  float shadowOpacity = 0;
  float shadowRadius = 0;
  float shadowOffsetX = 0;
  float shadowOffsetY = 0;
};

struct TextSpec {
  std::string text;
  uint32_t color = 0;
  int fontWeight = 400;
  int numberOfLines = 0;
  std::string fontFamily;  // empty for the system font
  bool italic = false;
  int textAlign = 0;   // TextAlign
  int decoration = 0;  // 1 underline, 2 line-through
  bool allowFontScaling = true;
  float fontSize = 14;
  float maxWidth = 0;
  float lineHeight = 0;  // 0 for the font's own
  float letterSpacing = 0;
};

struct ImageSpec {
  std::string uri;
  int resizeMode = 0;  // ResizeMode
  std::optional<uint32_t> tint;
  Headers headers;
  float width = 0;
  float height = 0;
  std::array<float, 4> radii{};
};

// What a cell tree needs from the platform's views; the tests drive it with fakes.
class CellViews {
 public:
  virtual ~CellViews() = default;
  virtual ViewHandle root() = 0;
  virtual ViewHandle create(ViewKind kind) = 0;
  virtual void add(ViewHandle parent, ViewHandle child, int index) = 0;
  virtual void remove(ViewHandle parent, ViewHandle child) = 0;
  virtual void move(ViewHandle parent, ViewHandle child, int index) = 0;
  virtual void removeAll() = 0;
  virtual void frame(ViewHandle view, float x, float y, float width, float height) = 0;
  virtual void viewStyle(ViewHandle view, const ViewStyleSpec& style) = 0;
  virtual void text(ViewHandle view, const TextSpec& text, float insetLeft, float insetTop) = 0;
  virtual void image(ViewHandle view, const ImageSpec& image) = 0;
  virtual void pressable(ViewHandle view, const std::string& pressId, float pressedOpacity) = 0;
  virtual void accessibility(ViewHandle view, const std::optional<std::string>& label,
                             const std::optional<std::string>& testID, int role, int accessible) = 0;
};

// Applies op batches to a cell's views in process; units are points, which equal the core's dp.
class CellTree {
 public:
  explicit CellTree(CellViews& views) : views_(views) {}
  void apply(const OpBatch& batch);
  float rootWidth() const { return rootWidth_; }
  float rootHeight() const { return rootHeight_; }

 private:
  ViewHandle target(int id) const;
  void reset();

  CellViews& views_;
  std::unordered_map<int, ViewHandle> byId_;
  std::unordered_map<int, ViewHandle> parents_;
  float rootWidth_ = 0;
  float rootHeight_ = 0;
};

}  // namespace flume
