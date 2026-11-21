#include "doctest.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "batch.h"
#include "core/Layout.h"
#include "core/Reconciler.h"
#include "core/SeparatorRule.h"
#include "core/TreeParser.h"
#include "fake_measurer.h"
#include "values.h"

namespace {

// The same table as src/__tests__/separators.test.ts.
struct Case {
  int count, columns;
  std::vector<int> spans;
  int start;
};

struct Bound {
  FakeMeasurer fake;
  flume::Layout layout{fake, 1};
  flume::Reconciler rec{layout};
  flume::Holder holder;
  flume::OpBatch batch;

  void bind(std::unique_ptr<flume::Node> root, float width = 360) {
    batch.clear();
    rec.reconcile(holder, std::move(root), batch);
    layout.layout(*holder.root, width, NAN);
    layout.readFrames(*holder.root, batch);
    rec.emitAfterLayout(holder, batch);
  }
};

std::unique_ptr<flume::Node> parsed(const flume::Value& tree) {
  auto result = flume::parseTree(tree);
  REQUIRE(result.root);
  return std::move(result.root);
}

flume::Value row(const char* label = nullptr) {
  flume::Value::Object row{
      {"type", V("pressable")},
      {"id", V("open")},
      {"testID", V("row-a")},
      {"style", obj({{"margin", V(10)}, {"padding", V(12)}, {"backgroundColor", V("#ffffff")},
                     {"shadowOpacity", V(0.3)}, {"shadowRadius", V(4)}})},
      {"children", arr({obj({{"type", V("text")}, {"text", V("hello")}}),
                        obj({{"type", V("text")}, {"text", V("9:41")}})})}};
  if (label != nullptr) row.emplace_back("accessibilityLabel", V(label));
  return flume::Value(std::move(row));
}

flume::Value separator() {
  return obj({{"type", V("view")},
              {"style", obj({{"height", V(1)}, {"backgroundColor", V("#cccccc")}})},
              {"children", arr({obj({{"type", V("text")}, {"text", V("divider")}})})}});
}

// The last accessibility op of a view: label, testID, role.
struct Access {
  std::string label, testID;
  int role;
};
std::optional<Access> accessOf(const flume::OpBatch& batch, int id) {
  std::optional<Access> found;
  for (const auto& op : decode(batch)) {
    if (op.op != 8 || op.id != id) continue;
    auto text = [&](int index) { return index < 0 ? std::string() : batch.strings[index]; };
    found = Access{text(op.ints[0]), text(op.ints[1]), op.ints[2]};
  }
  return found;
}

}  // namespace

TEST_CASE("the last grid row starts where the table says") {
  const std::vector<Case> cases{{0, 1, {}, 0},  {5, 1, {}, 4},  {9, 3, {}, 6},       {10, 3, {}, 9},
                                {10, 3, {4}, 8}, {10, 3, {9}, 9}, {6, 2, {0, 3}, 4}};
  for (const Case& c : cases) {
    CAPTURE(c.count);
    CAPTURE(c.columns);
    CHECK(flume::lastRowStart(c.count, c.columns, c.spans) == c.start);
  }
}

TEST_CASE("a composed row draws like the plain row") {
  Bound plain;
  plain.bind(parsed(row()));
  Bound composed;
  composed.bind(flume::composeRow(parsed(row()), parsed(separator()), false, false));
  // Plain: the root is view 0 and its texts views 1 and 2. Composed: the row is view 1, its texts 2 and 3.
  auto plainRoot = frameOf(plain.batch, 0);
  auto composedRow = frameOf(composed.batch, 1);
  REQUIRE(plainRoot);
  REQUIRE(composedRow);
  // A root margin is ignored, so the row spans the bind width in both.
  CHECK((*plainRoot)[2] == 360);
  CHECK((*composedRow)[0] == 0);
  CHECK((*composedRow)[1] == 0);
  CHECK((*composedRow)[2] == (*plainRoot)[2]);
  CHECK((*composedRow)[3] == (*plainRoot)[3]);
  CHECK(frameOf(composed.batch, 2) == frameOf(plain.batch, 1));
  CHECK(frameOf(composed.batch, 3) == frameOf(plain.batch, 2));
  // The separator is view 4, under the row and as wide as the bind width.
  auto divider = frameOf(composed.batch, 4);
  REQUIRE(divider);
  CHECK((*divider)[0] == 0);
  CHECK((*divider)[1] == (*plainRoot)[3]);
  CHECK((*divider)[2] == 360);
  CHECK((*divider)[3] == 1);
  auto plainAccess = accessOf(plain.batch, 0);
  auto composedAccess = accessOf(composed.batch, 0);
  REQUIRE(plainAccess);
  REQUIRE(composedAccess);
  CHECK(composedAccess->label == plainAccess->label);
  CHECK(composedAccess->label.find("divider") == std::string::npos);
  CHECK(composedAccess->testID == "row-a");
  CHECK(composedAccess->role == 1);
  CHECK_FALSE(accessOf(composed.batch, 1).has_value());
  CHECK(firstOp(composed.batch, 7, 0).id == 0);
}

TEST_CASE("a composed row keeps the row's own label on the cell") {
  Bound composed;
  composed.bind(flume::composeRow(parsed(row("Open chat")), parsed(separator()), false, false));
  auto access = accessOf(composed.batch, 0);
  REQUIRE(access);
  CHECK(access->label == "Open chat");
  CHECK(access->testID == "row-a");
  CHECK_FALSE(accessOf(composed.batch, 1).has_value());
}

TEST_CASE("an inverted composed row puts the separator first") {
  Bound composed;
  composed.bind(flume::composeRow(parsed(row()), parsed(separator()), false, true));
  // The separator is created first, so it is view 1 and the row view 3.
  auto divider = frameOf(composed.batch, 1);
  REQUIRE(divider);
  CHECK((*divider)[1] == 0);
  CHECK((*divider)[3] == 1);
}

TEST_CASE("a composed row without a separator keeps the row alone") {
  Bound composed;
  composed.bind(flume::composeRow(parsed(row()), nullptr, false, false));
  CHECK(frameOf(composed.batch, 1).has_value());
  CHECK_FALSE(frameOf(composed.batch, 4).has_value());
}
