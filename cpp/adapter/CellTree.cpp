#include "adapter/CellTree.h"

namespace flume {

ViewHandle CellTree::target(int id) const {
  if (id == 0) return views_.root();
  auto it = byId_.find(id);
  return it == byId_.end() ? nullptr : it->second;
}

void CellTree::reset() {
  byId_.clear();
  parents_.clear();
  rootWidth_ = 0;
  rootHeight_ = 0;
  views_.removeAll();
}

void CellTree::apply(const OpBatch& batch) {
  if (batch.first) reset();
  const auto& ints = batch.ints;
  const auto& floats = batch.floats;
  const auto& strings = batch.strings;
  auto string = [&](int32_t index) { return index >= 0 ? strings[index] : std::string(); };
  size_t i = 0;
  size_t f = 0;
  while (i + 1 < ints.size()) {
    const int id = ints[i + 1];
    switch (static_cast<Op>(ints[i])) {
      case Op::Create: {
        ViewHandle view = views_.create(static_cast<ViewKind>(ints[i + 2]));
        ViewHandle parent = ints[i + 3] == 0 ? views_.root() : target(ints[i + 3]);
        if (parent == nullptr) parent = views_.root();
        byId_[id] = view;
        parents_[id] = parent;
        views_.add(parent, view, ints[i + 4]);
        i += 5;
        break;
      }
      case Op::Remove: {
        auto view = byId_.find(id);
        auto parent = parents_.find(id);
        if (view != byId_.end() && parent != parents_.end()) views_.remove(parent->second, view->second);
        if (view != byId_.end()) byId_.erase(view);
        if (parent != parents_.end()) parents_.erase(parent);
        i += 2;
        break;
      }
      case Op::Frame: {
        if (id == 0) {
          rootWidth_ = floats[f + 2];
          rootHeight_ = floats[f + 3];
        } else if (ViewHandle view = target(id)) {
          views_.frame(view, floats[f], floats[f + 1], floats[f + 2], floats[f + 3]);
        }
        i += 2;
        f += 4;
        break;
      }
      case Op::ViewStyle: {
        if (ViewHandle view = target(id)) {
          ViewStyleSpec style;
          style.backgroundColor = static_cast<uint32_t>(ints[i + 2]);
          style.borderColor = static_cast<uint32_t>(ints[i + 3]);
          style.shadowColor = static_cast<uint32_t>(ints[i + 4]);
          style.opacity = floats[f];
          style.radii = {floats[f + 1], floats[f + 2], floats[f + 3], floats[f + 4]};
          style.borderWidth = floats[f + 5];
          style.elevation = floats[f + 6];
          style.shadowOpacity = floats[f + 7];
          style.shadowRadius = floats[f + 8];
          style.shadowOffsetX = floats[f + 9];
          style.shadowOffsetY = floats[f + 10];
          views_.viewStyle(view, style);
        }
        i += 5;
        f += 11;
        break;
      }
      case Op::Text: {
        if (ViewHandle view = target(id)) {
          TextSpec text;
          text.text = string(ints[i + 2]);
          text.color = static_cast<uint32_t>(ints[i + 3]);
          text.fontWeight = ints[i + 4];
          text.numberOfLines = ints[i + 5];
          text.fontFamily = string(ints[i + 6]);
          text.italic = ints[i + 7] == 1;
          text.textAlign = ints[i + 8];
          text.decoration = ints[i + 9];
          text.allowFontScaling = ints[i + 10] == 1;
          text.fontSize = floats[f];
          text.maxWidth = floats[f + 1];
          text.lineHeight = floats[f + 2];
          text.letterSpacing = floats[f + 3];
          views_.text(view, text, floats[f + 4], floats[f + 5]);
        }
        i += 11;
        f += 6;
        break;
      }
      case Op::Image: {
        const int headerCount = ints[i + 6];
        if (ViewHandle view = target(id)) {
          ImageSpec image;
          image.uri = string(ints[i + 2]);
          image.resizeMode = ints[i + 3];
          if (ints[i + 4] == 1) image.tint = static_cast<uint32_t>(ints[i + 5]);
          for (int h = 0; h < headerCount; h++) {
            image.headers.emplace_back(string(ints[i + 7 + 2 * h]), string(ints[i + 8 + 2 * h]));
          }
          image.width = floats[f];
          image.height = floats[f + 1];
          image.radii = {floats[f + 2], floats[f + 3], floats[f + 4], floats[f + 5]};
          views_.image(view, image);
        }
        i += 7 + 2 * headerCount;
        f += 6;
        break;
      }
      case Op::ImageInsets: {
        if (ViewHandle view = target(id)) views_.imageInsets(view, floats[f], floats[f + 1], floats[f + 2], floats[f + 3]);
        i += 2;
        f += 4;
        break;
      }
      case Op::Pressable: {
        if (ViewHandle view = target(id)) views_.pressable(view, string(ints[i + 2]), floats[f]);
        i += 3;
        f += 1;
        break;
      }
      case Op::Accessibility: {
        if (ViewHandle view = target(id)) {
          std::optional<std::string> label;
          std::optional<std::string> testID;
          if (ints[i + 2] >= 0) label = strings[ints[i + 2]];
          if (ints[i + 3] >= 0) testID = strings[ints[i + 3]];
          views_.accessibility(view, label, testID, ints[i + 4], ints[i + 5]);
        }
        i += 6;
        break;
      }
      case Op::Move: {
        auto view = byId_.find(id);
        auto parent = parents_.find(id);
        if (view != byId_.end() && parent != parents_.end()) views_.move(parent->second, view->second, ints[i + 2]);
        i += 3;
        break;
      }
      default:
        return;
    }
  }
}

}  // namespace flume
