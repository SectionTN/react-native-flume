#pragma once

#include <array>
#include <optional>
#include <vector>

#include "core/OpBatch.h"

// Decodes the typed arrays the way the Kotlin side does, for assertions.
struct DecodedOp {
  int op = 0;
  int id = 0;
  std::vector<int> ints;
  std::vector<float> floats;
};

inline std::vector<DecodedOp> decode(const flume::OpBatch& batch) {
  std::vector<DecodedOp> ops;
  size_t i = 0;
  size_t f = 0;
  auto takeInts = [&](DecodedOp& op, int count) {
    for (int k = 0; k < count; k++) op.ints.push_back(batch.ints[i++]);
  };
  auto takeFloats = [&](DecodedOp& op, int count) {
    for (int k = 0; k < count; k++) op.floats.push_back(batch.floats[f++]);
  };
  while (i < batch.ints.size()) {
    DecodedOp op;
    op.op = batch.ints[i++];
    op.id = batch.ints[i++];
    switch (op.op) {
      case 1: takeInts(op, 3); break;
      case 2: break;
      case 3: takeFloats(op, 4); break;
      case 4: takeInts(op, 1); takeFloats(op, 2); break;
      case 5: takeInts(op, 4); takeFloats(op, 2); break;
      case 6: takeInts(op, 1); takeFloats(op, 3); break;
      default: return ops;
    }
    ops.push_back(std::move(op));
  }
  return ops;
}

inline std::vector<int> opsOf(const flume::OpBatch& batch) {
  std::vector<int> codes;
  for (const auto& op : decode(batch)) codes.push_back(op.op);
  return codes;
}

inline std::optional<std::array<float, 4>> frameOf(const flume::OpBatch& batch, int id) {
  std::optional<std::array<float, 4>> frame;
  for (const auto& op : decode(batch)) {
    if (op.op == 3 && op.id == id) frame = {op.floats[0], op.floats[1], op.floats[2], op.floats[3]};
  }
  return frame;
}

inline int createdParent(const flume::OpBatch& batch, int id) {
  for (const auto& op : decode(batch)) {
    if (op.op == 1 && op.id == id) return op.ints[1];
  }
  return -1;
}

inline float textMaxWidth(const flume::OpBatch& batch, int id) {
  for (const auto& op : decode(batch)) {
    if (op.op == 5 && op.id == id) return op.floats[1];
  }
  return -1;
}
