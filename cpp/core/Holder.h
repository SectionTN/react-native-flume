#pragma once

#include <memory>

#include "core/Mounted.h"

namespace flume {

// One recycled platform holder: its mounted tree survives binds, view ids are unique within it.
struct Holder {
  int id = 0;
  int nextViewId = 1;
  std::unique_ptr<Mounted> root;
  std::string rootLabel;  // the derived accessibility label last sent for the root
};

}  // namespace flume
