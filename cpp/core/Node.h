#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/Style.h"

namespace flume {

enum class NodeType : uint8_t { View, Text, Image, Pressable };

// One parsed element from renderCell, before it is matched against a holder's mounted tree.
struct Node {
  NodeType type = NodeType::View;
  Style style;
  std::string text;
  bool allowFontScaling = true;
  std::string imageUri;
  ResizeMode resizeMode = ResizeMode::Cover;
  std::optional<uint32_t> tintColor;
  std::vector<std::pair<std::string, std::string>> imageHeaders;
  std::string pressId;
  float pressedOpacity = 0.7f;
  std::string key;
  std::string testID;
  std::string accessibilityLabel;
  std::optional<bool> accessible;
  std::vector<std::unique_ptr<Node>> children;  // view and pressable only
};

}  // namespace flume
