#pragma once

#include <memory>
#include <vector>

#include "core/Node.h"

namespace flume {

// The first index of the last grid row, whose rows draw no separator. A full-span index ends a run of lanes.
int lastRowStart(int count, int lanes, const std::vector<int>& sortedFullSpans);

// The row and its separator (first when inverted) under a root that takes the row root's press, label, id and role.
std::unique_ptr<Node> composeRow(std::unique_ptr<Node> row, std::unique_ptr<Node> separator, bool horizontal,
                                 bool inverted);

}  // namespace flume
