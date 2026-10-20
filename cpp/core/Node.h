#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/Style.h"

namespace flume {

enum class NodeType : uint8_t { View, Text, Image };

// One parsed element from renderCell, before it is matched against a holder's mounted tree.
struct Node {
  NodeType type = NodeType::View;
  Style style;
  std::string text;
  bool allowFontScaling = true;
  std::string imageUri;
  std::string key;
  std::vector<std::unique_ptr<Node>> children;
};

}  // namespace flume
