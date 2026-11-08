#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <vector>

#include "core/OpBatch.h"
#include "doctest.h"

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
    for (int k = 0; k < count; k++) op.ints.push_back(i < batch.ints.size() ? batch.ints[i++] : -1);
  };
  auto takeFloats = [&](DecodedOp& op, int count) {
    for (int k = 0; k < count; k++) op.floats.push_back(f < batch.floats.size() ? batch.floats[f++] : NAN);
  };
  while (i + 1 < batch.ints.size()) {
    DecodedOp op;
    op.op = batch.ints[i++];
    op.id = batch.ints[i++];
    switch (op.op) {
      case 1: takeInts(op, 3); break;
      case 2: break;
      case 3: takeFloats(op, 4); break;
      case 4: takeInts(op, 3); takeFloats(op, 11); break;
      case 5: takeInts(op, 9); takeFloats(op, 6); break;
      case 6: takeInts(op, 5); takeInts(op, 2 * std::max(op.ints[4], 0)); takeFloats(op, 6); break;
      case 7: takeInts(op, 1); takeFloats(op, 1); break;
      case 8: takeInts(op, 4); break;
      case 9: takeInts(op, 1); break;
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

// The first op with that code and id; a missing op fails the check and returns a sentinel-filled op.
inline DecodedOp firstOp(const flume::OpBatch& batch, int code, int id) {
  for (const auto& op : decode(batch)) {
    if (op.op == code && op.id == id) return op;
  }
  FAIL_CHECK("no op " << code << " for id " << id);
  DecodedOp missing;
  missing.ints.assign(32, -1);
  missing.floats.assign(32, -1);
  return missing;
}

inline int createdParent(const flume::OpBatch& batch, int id) { return firstOp(batch, 1, id).ints[1]; }

inline DecodedOp textOp(const flume::OpBatch& batch, int id) { return firstOp(batch, 5, id); }

inline float textMaxWidth(const flume::OpBatch& batch, int id) {
  for (const auto& op : decode(batch)) {
    if (op.op == 5 && op.id == id) return op.floats[1];
  }
  return -1;
}
