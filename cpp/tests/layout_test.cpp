#include "doctest.h"

#include <memory>
#include <string>

#include "batch.h"
#include "core/Layout.h"
#include "core/TreeParser.h"
#include "fake_measurer.h"
#include "values.h"

namespace {

using Frame = std::array<float, 4>;

// Mounts a parsed tree by hand; the reconciler takes this job over in Task 8.
std::unique_ptr<flume::Mounted> mount(flume::Layout& layout, const flume::Node& node, int& nextId, bool isRoot) {
  auto mounted = std::make_unique<flume::Mounted>();
  mounted->type = node.type;
  mounted->style = node.style;
  mounted->text = node.text;
  mounted->imageUri = node.imageUri;
  mounted->viewId = isRoot ? 0 : nextId++;
  mounted->yoga = layout.createNode(*mounted);
  layout.applyStyle(mounted->yoga, mounted->style);
  for (const auto& child : node.children) {
    auto childMounted = mount(layout, *child, nextId, false);
    YGNodeInsertChild(mounted->yoga, childMounted->yoga, YGNodeGetChildCount(mounted->yoga));
    mounted->children.push_back(std::move(childMounted));
  }
  return mounted;
}

flume::Value chatCellWithText(const std::string& textA) {
  auto cell = chatCell();
  auto parsedTemplate = flume::parseTree(cell);
  REQUIRE(parsedTemplate.root);
  return obj({{"type", V("view")},
              {"style", obj({{"flexDirection", V("row")}, {"padding", V(12)}, {"gap", V(12)}, {"alignItems", V("center")}})},
              {"children",
               arr({obj({{"type", V("image")},
                         {"source", V("https://x/a.png")},
                         {"style", obj({{"width", V(40)}, {"height", V(40)}, {"borderRadius", V(20)}})}}),
                    obj({{"type", V("view")},
                         {"style", obj({{"flex", V(1)}})},
                         {"children", arr({obj({{"type", V("text")}, {"text", V(textA.c_str())}, {"style", obj({{"fontSize", V(16)}})}}),
                                           obj({{"type", V("text")}, {"text", V("9:41")}, {"style", obj({{"fontSize", V(12)}})}})})}})})}});
}

}  // namespace

TEST_CASE("lays out the chat cell at 360") {
  FakeMeasurer fake;
  flume::Layout layout(fake, 1);
  int nextId = 1;
  auto parsed = flume::parseTree(chatCell());
  auto root = mount(layout, *parsed.root, nextId, true);
  flume::OpBatch batch;
  layout.layout(*root, 360, NAN);
  layout.readFrames(*root, batch);
  CHECK(batch.rootWidth == 360.0f);
  CHECK(batch.rootHeight == 64.0f);
  CHECK(frameOf(batch, 0) == Frame{0, 0, 360, 64});
  CHECK(frameOf(batch, 1) == Frame{12, 12, 40, 40});
  CHECK(frameOf(batch, 2) == Frame{64, 14, 284, 36});
  CHECK(frameOf(batch, 3) == Frame{0, 0, 284, 20});
  CHECK(frameOf(batch, 4) == Frame{0, 20, 284, 16});
  CHECK(root->children[1]->children[0]->measuredMaxWidth == 284.0f);
  CHECK(layout.measureCalls == 2);
  CHECK(layout.measureHits == 0);
}

TEST_CASE("a three-line text grows the row past the image") {
  FakeMeasurer fake;
  flume::Layout layout(fake, 1);
  int nextId = 1;
  auto parsed = flume::parseTree(chatCellWithText(std::string(100, 'a')));
  auto root = mount(layout, *parsed.root, nextId, true);
  flume::OpBatch batch;
  layout.layout(*root, 360, NAN);
  layout.readFrames(*root, batch);
  CHECK(batch.rootHeight == 100.0f);
  CHECK(frameOf(batch, 1) == Frame{12, 30, 40, 40});
  CHECK(frameOf(batch, 2) == Frame{64, 12, 284, 76});
  CHECK(frameOf(batch, 3) == Frame{0, 0, 284, 60});
}

TEST_CASE("an unchanged layout emits no frames") {
  FakeMeasurer fake;
  flume::Layout layout(fake, 1);
  int nextId = 1;
  auto parsed = flume::parseTree(chatCell());
  auto root = mount(layout, *parsed.root, nextId, true);
  flume::OpBatch first;
  layout.layout(*root, 360, NAN);
  layout.readFrames(*root, first);
  flume::OpBatch second;
  layout.layout(*root, 360, NAN);
  layout.readFrames(*root, second);
  CHECK(first.opCount() == 5);
  CHECK(second.opCount() == 0);
  CHECK(second.rootHeight == 64.0f);
}

TEST_CASE("the measure cache answers a repeated measure") {
  FakeMeasurer fake;
  flume::Layout layout(fake, 1);
  int nextId = 1;
  auto parsed = flume::parseTree(chatCell());
  auto root = mount(layout, *parsed.root, nextId, true);
  layout.layout(*root, 360, NAN);
  int callsAfterFirst = fake.calls;
  layout.measureCalls = 0;
  layout.measureHits = 0;
  layout.markTextDirty(root->children[1]->children[0]->yoga);
  layout.layout(*root, 360, NAN);
  CHECK(fake.calls == callsAfterFirst);
  CHECK(layout.measureCalls == 1);
  CHECK(layout.measureHits == 1);
}

namespace {

// Mounts a tree, lays it out at the width and returns its frames.
flume::OpBatch framesAt(const flume::Value& tree, float width) {
  FakeMeasurer fake;
  flume::Layout layout(fake, 1);
  int nextId = 1;
  auto parsed = flume::parseTree(tree);
  REQUIRE(parsed.root);
  auto root = mount(layout, *parsed.root, nextId, true);
  flume::OpBatch batch;
  layout.layout(*root, width, NAN);
  layout.readFrames(*root, batch);
  return batch;
}

flume::Value styled(flume::Value style, std::vector<flume::Value> children = {}) {
  flume::Value::Object node{{"type", V("view")}, {"style", std::move(style)}};
  if (!children.empty()) node.emplace_back("children", flume::Value(flume::Value::Array(children.begin(), children.end())));
  return flume::Value(std::move(node));
}

}  // namespace

TEST_CASE("percent width and aspect ratio") {
  auto batch = framesAt(styled(obj({}), {styled(obj({{"width", V("50%")}, {"aspectRatio", V(2)}}))}), 360);
  CHECK(frameOf(batch, 1) == Frame{0, 0, 180, 90});
}

TEST_CASE("display none takes no space") {
  auto batch = framesAt(styled(obj({}), {styled(obj({{"height", V(10)}, {"display", V("none")}})),
                                         styled(obj({{"height", V(20)}}))}),
                        360);
  CHECK(frameOf(batch, 2) == Frame{0, 0, 360, 20});
}

TEST_CASE("an auto margin pushes a row child to the end") {
  auto batch = framesAt(styled(obj({{"flexDirection", V("row")}}),
                               {styled(obj({{"width", V(100)}, {"height", V(10)}, {"marginLeft", V("auto")}}))}),
                        360);
  CHECK(frameOf(batch, 1) == Frame{260, 0, 100, 10});
}

TEST_CASE("border width insets the children") {
  auto batch = framesAt(styled(obj({{"borderWidth", V(4)}}),
                               {obj({{"type", V("text")}, {"text", V("hello")}, {"style", obj({{"fontSize", V(16)}})}})}),
                        360);
  CHECK(frameOf(batch, 1) == Frame{4, 4, 352, 20});
}

TEST_CASE("a row laid out at a fixed height takes its content width") {
  FakeMeasurer fake;
  flume::Layout layout(fake, 1);
  int nextId = 1;
  auto parsed = flume::parseTree(obj(
      {{"type", V("view")},
       {"style", obj({{"flexDirection", V("row")}, {"padding", V(8)}, {"gap", V(4)}})},
       {"children", arr({obj({{"type", V("text")}, {"text", V("abcd")}, {"style", obj({{"fontSize", V(10)}})}}),
                         obj({{"type", V("text")}, {"text", V("ab")}, {"style", obj({{"fontSize", V(10)}})}})})}}));
  REQUIRE(parsed.root);
  auto root = mount(layout, *parsed.root, nextId, true);
  flume::OpBatch batch;
  layout.layout(*root, NAN, 48);
  layout.readFrames(*root, batch);
  CHECK(batch.rootWidth == 50);
  CHECK(batch.rootHeight == 48);
}

namespace {

// A tree bound at 134 wide, like a grid column, kept for a relayout.
struct Column {
  FakeMeasurer fake;
  flume::Layout layout{fake, 1};
  std::unique_ptr<flume::Mounted> root;

  explicit Column(const flume::Value& tree) {
    int nextId = 1;
    auto parsed = flume::parseTree(tree);
    REQUIRE(parsed.root);
    root = mount(layout, *parsed.root, nextId, true);
    frames([&] { layout.layout(*root, 134, NAN); });
  }

  template <typename F>
  flume::OpBatch frames(F run) {
    run();
    flume::OpBatch batch;
    layout.readFrames(*root, batch);
    return batch;
  }

  flume::OpBatch relayout(float main) {
    return frames([&] { layout.relayout(*root, false, main); });
  }
};

flume::Value tile(flume::Value style) {
  return styled(std::move(style), {styled(obj({{"flex", V(1)}}), {obj({{"type", V("text")}, {"text", V("hello")}})})});
}

}  // namespace

TEST_CASE("relayout stretches an auto root to the row length and keeps its text measures") {
  Column column(tile(obj({{"padding", V(4)}})));
  const float natural = YGNodeLayoutGetHeight(column.root->yoga);
  const int calls = column.layout.measureCalls;
  auto batch = column.relayout(150);
  CHECK(batch.rootHeight == 150);
  CHECK(frameOf(batch, 1) == Frame{4, 4, 126, 142});
  CHECK(column.layout.measureCalls == calls);
  CHECK(column.relayout(NAN).rootHeight == natural);
}

TEST_CASE("relayout keeps a root that sets its own height") {
  Column column(tile(obj({{"height", V(100)}})));
  CHECK(column.relayout(150).rootHeight == 100);
}

TEST_CASE("a root with an aspect ratio takes its height from the bind width and keeps it") {
  Column column(tile(obj({{"aspectRatio", V(1)}})));
  auto bound = column.frames([&] { column.layout.layout(*column.root, 134, NAN); });
  CHECK(column.root->yoga != nullptr);
  CHECK(YGNodeLayoutGetHeight(column.root->yoga) == 134);
  CHECK(column.relayout(150).rootHeight == 134);
}
