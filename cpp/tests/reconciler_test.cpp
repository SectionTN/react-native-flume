#include "doctest.h"

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

  void bind(const flume::Value& tree) {
    batch.clear();
    auto parsed = flume::parseTree(tree);
    REQUIRE(parsed.root);
    rec.reconcile(holder, std::move(parsed.root), batch);
    layout.layout(*holder.root, 360);
    layout.readFrames(*holder.root, batch);
    rec.emitText(holder, batch);
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
  CHECK(opsOf(rig.batch) == std::vector<int>{1, 6, 1, 1, 1, 3, 3, 3, 3, 3, 5, 5});
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
  CHECK(opsOf(rig.batch) == std::vector<int>{5});
  CHECK(rig.batch.strings.back() == "9:42");
}

TEST_CASE("fewer children remove from the end") {
  Rig rig;
  rig.bind(chatCell());
  rig.bind(rowCell(imageNode("https://x/a.png"), {textNode("hello", 16, "#111")}));
  CHECK(opsOf(rig.batch) == std::vector<int>{2, 3});
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
  CHECK(ops.back() == 5);
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
