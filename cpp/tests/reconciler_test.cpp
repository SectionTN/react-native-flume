#include "doctest.h"

#include <algorithm>
#include <array>
#include <vector>

#include "batch.h"
#include "core/Layout.h"
#include "core/Reconciler.h"
#include "core/TreeParser.h"
#include "fake_measurer.h"
#include "values.h"

namespace {

using Frame = std::array<float, 4>;

// One holder bound the way the engine does it: reconcile, layout, frames, then the text ops.
struct Rig {
  FakeMeasurer fake;
  flume::Layout layout{fake, 1};
  flume::Reconciler rec{layout};
  flume::Holder holder;
  flume::OpBatch batch;

  void bind(const flume::Value& tree, float width = 360) {
    batch.clear();
    auto parsed = flume::parseTree(tree);
    REQUIRE(parsed.root);
    rec.reconcile(holder, std::move(parsed.root), batch);
    layout.layout(*holder.root, width, NAN);
    layout.readFrames(*holder.root, batch);
    rec.emitAfterLayout(holder, batch);
  }
};

flume::Value imageNode(const char* uri) {
  return obj({{"type", V("image")},
              {"source", V(uri)},
              {"style", obj({{"width", V(40)}, {"height", V(40)}, {"borderRadius", V(20)}})}});
}

flume::Value textNode(const char* text, int fontSize, const char* color) {
  return obj({{"type", V("text")}, {"text", V(text)}, {"style", obj({{"fontSize", V(fontSize)}, {"color", V(color)}})}});
}

flume::Value rowCell(flume::Value first, std::vector<flume::Value> column, const char* background = nullptr) {
  flume::Value::Object style{{"flexDirection", V("row")}, {"padding", V(12)}, {"gap", V(12)}, {"alignItems", V("center")}};
  if (background != nullptr) style.emplace_back("backgroundColor", V(background));
  flume::Value::Array children{std::move(first)};
  if (!column.empty()) {
    children.push_back(obj({{"type", V("view")},
                            {"style", obj({{"flex", V(1)}})},
                            {"children", flume::Value(flume::Value::Array(column.begin(), column.end()))}}));
  }
  return flume::Value(flume::Value::Object{{"type", V("view")},
                                           {"style", flume::Value(std::move(style))},
                                           {"children", flume::Value(std::move(children))}});
}

flume::Value chatLike() {
  return rowCell(imageNode("https://x/a.png"), {textNode("hello", 16, "#111"), textNode("9:41", 12, "#666")});
}

std::vector<int> idsOf(const flume::OpBatch& batch, size_t count) {
  std::vector<int> ids;
  for (const auto& op : decode(batch)) {
    if (ids.size() == count) break;
    ids.push_back(op.id);
  }
  return ids;
}

}  // namespace

TEST_CASE("first bind creates every view in document order") {
  Rig rig;
  rig.bind(chatCell());
  CHECK(opsOf(rig.batch) == std::vector<int>{1, 6, 1, 1, 1, 3, 3, 3, 3, 3, 5, 5, 8});
  CHECK(rig.holder.root->children[1]->children[1]->viewId == 4);
  CHECK(createdParent(rig.batch, 3) == 2);
  CHECK(textMaxWidth(rig.batch, 3) == 284.0f);
  CHECK(rig.batch.rootHeight == 64.0f);
}

TEST_CASE("an equal tree emits nothing") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(chatLike());
  CHECK(rig.batch.opCount() == 0);
  CHECK(rig.batch.rootHeight == 64.0f);
}

TEST_CASE("a changed text emits one Text op") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(rowCell(imageNode("https://x/a.png"), {textNode("hello", 16, "#111"), textNode("9:42", 12, "#666")}));
  CHECK(opsOf(rig.batch) == std::vector<int>{5, 8});
  CHECK(rig.batch.strings[textOp(rig.batch, 4).ints[0]] == "9:42");
}

TEST_CASE("fewer children remove from the end") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(rowCell(imageNode("https://x/a.png"), {textNode("hello", 16, "#111")}));
  CHECK(opsOf(rig.batch) == std::vector<int>{2, 3, 8});
  CHECK(idsOf(rig.batch, 1) == std::vector<int>{4});
  CHECK(frameOf(rig.batch, 2) == Frame{64, 22, 284, 20});
  CHECK_FALSE(frameOf(rig.batch, 3).has_value());
}

TEST_CASE("more children create at the end") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(rowCell(imageNode("https://x/a.png"),
                   {textNode("hello", 16, "#111"), textNode("9:41", 12, "#666"), textNode("x", 12, "#666")}));
  auto ops = opsOf(rig.batch);
  CHECK(ops.front() == 1);
  CHECK(ops.back() == 8);
  CHECK(createdParent(rig.batch, 5) == 2);
  CHECK(decode(rig.batch).front().ints[2] == 2);
  CHECK(rig.batch.rootHeight == 76.0f);
}

TEST_CASE("a type change at a position replaces the view") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(rowCell(obj({{"type", V("view")}, {"style", obj({{"width", V(40)}, {"height", V(40)}})}}),
                   {textNode("hello", 16, "#111"), textNode("9:41", 12, "#666")}));
  auto ops = opsOf(rig.batch);
  REQUIRE(ops.size() >= 2);
  CHECK(ops[0] == 2);
  CHECK(ops[1] == 1);
  CHECK(idsOf(rig.batch, 2) == std::vector<int>{1, 5});
  CHECK(rig.holder.root->children[0]->viewId == 5);
}

TEST_CASE("removing a subtree removes children first") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(rowCell(imageNode("https://x/a.png"), {}));
  auto ops = opsOf(rig.batch);
  REQUIRE(ops.size() >= 3);
  CHECK(std::vector<int>(ops.begin(), ops.begin() + 3) == std::vector<int>{2, 2, 2});
  CHECK(idsOf(rig.batch, 3) == std::vector<int>{3, 4, 2});
}

TEST_CASE("visual changes emit style and image ops") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(rowCell(imageNode("https://x/a.png"), {textNode("hello", 16, "#111"), textNode("9:41", 12, "#666")}, "#fff"));
  CHECK(opsOf(rig.batch) == std::vector<int>{4});
  CHECK(decode(rig.batch).front().id == 0);
  rig.bind(rowCell(imageNode("https://x/b.png"), {textNode("hello", 16, "#111"), textNode("9:41", 12, "#666")}, "#fff"));
  CHECK(opsOf(rig.batch) == std::vector<int>{6});
  CHECK(rig.batch.strings.back() == "https://x/b.png");
}

TEST_CASE("release frees the tree") {
  Rig rig;
  rig.bind(chatCell());
  rig.rec.release(rig.holder);
  CHECK(rig.holder.root == nullptr);
}

TEST_CASE("a narrower width re-emits the text op with the new max width") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(chatCell(), 300);
  CHECK(textMaxWidth(rig.batch, 3) == 224.0f);
  CHECK(textMaxWidth(rig.batch, 4) == 224.0f);
}

TEST_CASE("invalidated text is measured and sent again at the same width") {
  Rig rig;
  rig.bind(chatCell());
  rig.layout.cache().clear();
  rig.layout.invalidateText(*rig.holder.root);
  int calls = rig.layout.measureCalls;
  rig.bind(chatCell());
  CHECK(rig.layout.measureCalls > calls);
  CHECK(textMaxWidth(rig.batch, 3) == 284.0f);
}

TEST_CASE("a width laid out before still re-measures and re-sends the text") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(chatCell(), 300);
  int calls = rig.layout.measureCalls;
  rig.bind(chatCell());
  CHECK(rig.layout.measureCalls > calls);
  CHECK(textMaxWidth(rig.batch, 3) == 284.0f);
}

TEST_CASE("a pressable mounts with its id and pressed opacity") {
  Rig rig;
  rig.bind(rowCell(obj({{"type", V("pressable")}, {"id", V("like")}, {"children", arr({textNode("Like", 14, "#0066ff")})}}), {}));
  auto op = firstOp(rig.batch, 7, 1);
  CHECK(rig.batch.strings[op.ints[0]] == "like");
  CHECK(op.floats[0] == 0.7f);
  CHECK(createdParent(rig.batch, 2) == 1);
}

TEST_CASE("a text op carries the font options") {
  Rig rig;
  rig.bind(obj({{"type", V("text")}, {"text", V("a")}, {"allowFontScaling", V(false)},
                {"style", obj({{"fontFamily", V("Inter")}, {"fontStyle", V("italic")}, {"lineHeight", V(20)},
                               {"letterSpacing", V(0.5)}, {"textAlign", V("center")}, {"textDecorationLine", V("underline line-through")}})}}));
  auto op = textOp(rig.batch, 0);
  CHECK(rig.batch.strings[op.ints[4]] == "Inter");
  CHECK(op.ints[5] == 1);
  CHECK(op.ints[6] == 3);
  CHECK(op.ints[7] == 3);
  CHECK(op.floats[2] == 20.0f);
  CHECK(op.floats[3] == 0.5f);
  CHECK(op.ints[8] == 0);
}

TEST_CASE("an image op carries resize mode, tint and headers") {
  Rig rig;
  rig.bind(rowCell(obj({{"type", V("image")}, {"source", obj({{"uri", V("u")}, {"headers", obj({{"A", V("b")}})}})},
                        {"resizeMode", V("stretch")}, {"tintColor", V("#f00")},
                        {"style", obj({{"width", V(40)}, {"height", V(40)}, {"borderTopLeftRadius", V(8)}})}}), {}));
  auto op = firstOp(rig.batch, 6, 1);
  CHECK(op.ints[1] == 2);
  CHECK(op.ints[2] == 1);
  CHECK(static_cast<uint32_t>(op.ints[3]) == 0xFFFF0000u);
  CHECK(op.ints[4] == 1);
  CHECK(rig.batch.strings[op.ints[5]] == "A");
  CHECK(op.floats[2] == 8.0f);
  CHECK(op.floats[3] == 0.0f);
}

TEST_CASE("the root label joins the texts in order") {
  Rig rig;
  rig.bind(chatCell());
  auto op = firstOp(rig.batch, 8, 0);
  CHECK(rig.batch.strings[op.ints[0]] == "hello 9:41");
  rig.bind(chatCell());
  auto ops = opsOf(rig.batch);
  CHECK(std::count(ops.begin(), ops.end(), 8) == 0);
}

TEST_CASE("a border change re-emits the view style") {
  Rig rig;
  rig.bind(rowCell(imageNode("u"), {}, "#fff"));
  rig.bind(obj({{"type", V("view")},
                {"style", obj({{"flexDirection", V("row")}, {"padding", V(12)}, {"gap", V(12)}, {"alignItems", V("center")},
                               {"backgroundColor", V("#fff")}, {"borderWidth", V(2)}, {"borderColor", V("#000")}})},
                {"children", arr({imageNode("u")})}}));
  auto op = firstOp(rig.batch, 4, 0);
  CHECK(op.floats[5] == 2.0f);
  CHECK(static_cast<uint32_t>(op.ints[1]) == 0xFF000000u);
}

namespace {

flume::Value keyed(const char* key, const char* text) {
  return obj({{"type", V("text")}, {"key", V(key)}, {"text", V(text)}, {"style", obj({{"fontSize", V(16)}})}});
}

flume::Value column(std::vector<flume::Value> children) {
  return flume::Value(flume::Value::Object{
      {"type", V("view")}, {"children", flume::Value(flume::Value::Array(children.begin(), children.end()))}});
}

}  // namespace

TEST_CASE("keyed children reorder with move ops") {
  Rig rig;
  rig.bind(column({keyed("a", "A"), keyed("b", "B"), keyed("c", "C")}));
  rig.bind(column({keyed("c", "C"), keyed("a", "A"), keyed("b", "B")}));
  auto ops = opsOf(rig.batch);
  CHECK(std::count(ops.begin(), ops.end(), 1) == 0);
  CHECK(std::count(ops.begin(), ops.end(), 2) == 0);
  CHECK(std::count(ops.begin(), ops.end(), 9) == 1);
  auto move = firstOp(rig.batch, 9, 3);
  CHECK(move.ints[0] == 0);
  CHECK(frameOf(rig.batch, 3) == Frame{0, 0, 360, 20});
  CHECK(frameOf(rig.batch, 1) == Frame{0, 20, 360, 20});
}

TEST_CASE("a keyed child that leaves is removed and a new key mounts at its index") {
  Rig rig;
  rig.bind(column({keyed("a", "A"), keyed("b", "B"), keyed("c", "C")}));
  rig.bind(column({keyed("a", "A"), keyed("x", "X"), keyed("c", "C")}));
  auto ops = opsOf(rig.batch);
  CHECK(firstOp(rig.batch, 2, 2).id == 2);
  CHECK(firstOp(rig.batch, 1, 4).ints[2] == 1);
  CHECK(std::count(ops.begin(), ops.end(), 9) == 0);
}

TEST_CASE("a keyed child whose type changes remounts") {
  Rig rig;
  rig.bind(column({keyed("a", "A")}));
  rig.bind(column({obj({{"type", V("view")}, {"key", V("a")}, {"style", obj({{"height", V(10)}})}})}));
  auto ops = opsOf(rig.batch);
  CHECK(std::count(ops.begin(), ops.end(), 2) == 1);
  CHECK(std::count(ops.begin(), ops.end(), 1) == 1);
}

TEST_CASE("a text scales with the font by default") {
  Rig rig;
  rig.bind(obj({{"type", V("text")}, {"text", V("a")}}));
  CHECK(textOp(rig.batch, 0).ints[8] == 1);
}
