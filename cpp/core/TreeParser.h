#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/Node.h"
#include "core/Value.h"

namespace flume {

struct ParseResult {
  std::unique_ptr<Node> root;  // null on error
  std::string error;
};

// Validates the element shape; unknown keys are ignored, a pressable parses as a view for now.
ParseResult parseTree(const Value& value, std::vector<std::string>* unknownKeys = nullptr);

}  // namespace flume
