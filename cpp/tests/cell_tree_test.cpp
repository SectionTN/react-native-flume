#include "doctest.h"

#include <cstdio>
#include <memory>

#include "adapter/CellTree.h"

using namespace flume;

namespace {

struct FakeView {
  std::string name;
};

std::string number(float value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%g", value);
  return buffer;
}

// Names the root `root` and created views v1, v2, ... and logs every call it gets.
class FakeViews : public CellViews {
 public:
  std::vector<std::string> log;
  ViewStyleSpec lastStyle{};
  TextSpec lastText{};
  float insetLeft = 0, insetTop = 0;
  ImageSpec lastImage{};

  ViewHandle root() override { return &root_; }
  ViewHandle create(ViewKind kind) override {
    views_.push_back(std::make_unique<FakeView>(FakeView{"v" + std::to_string(views_.size() + 1)}));
    log.push_back("create " + views_.back()->name + " kind " + std::to_string(static_cast<int>(kind)));
    return views_.back().get();
  }
  void add(ViewHandle parent, ViewHandle child, int index) override {
    log.push_back("add " + name(parent) + " " + name(child) + " @" + std::to_string(index));
  }
  void remove(ViewHandle parent, ViewHandle child) override { log.push_back("remove " + name(parent) + " " + name(child)); }
  void move(ViewHandle parent, ViewHandle child, int index) override {
    log.push_back("move " + name(parent) + " " + name(child) + " @" + std::to_string(index));
  }
  void removeAll() override { log.push_back("removeAll"); }
  void frame(ViewHandle view, float x, float y, float width, float height) override {
    log.push_back("frame " + name(view) + " " + number(x) + " " + number(y) + " " + number(width) + " " + number(height));
  }
  void viewStyle(ViewHandle view, const ViewStyleSpec& s) override {
    lastStyle = s;
    log.push_back("style " + name(view));
  }
  void text(ViewHandle view, const TextSpec& t, float left, float top) override {
    lastText = t;
    insetLeft = left;
    insetTop = top;
    log.push_back("text " + name(view));
  }
  void image(ViewHandle view, const ImageSpec& i) override {
    lastImage = i;
    log.push_back("image " + name(view));
  }
  void pressable(ViewHandle view, const std::string& pressId, float pressedOpacity) override {
    log.push_back("pressable " + name(view) + " " + pressId + " " + number(pressedOpacity));
  }
  void accessibility(ViewHandle view, const std::optional<std::string>& label, const std::optional<std::string>& testID,
                     int role, int accessible) override {
    log.push_back("access " + name(view) + " " + label.value_or("-") + " " + testID.value_or("-") + " " +
                  std::to_string(role) + " " + std::to_string(accessible));
  }

 private:
  static std::string name(ViewHandle view) { return static_cast<FakeView*>(view)->name; }
  FakeView root_{"root"};
  std::vector<std::unique_ptr<FakeView>> views_;
};

bool logged(const FakeViews& views, const std::string& prefix) {
  for (const auto& line : views.log) {
    if (line.rfind(prefix, 0) == 0) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("a first bind clears the cell and builds the tree under the root") {
  OpBatch batch;
  batch.first = true;
  batch.create(1, ViewKind::View, 0, 0);
  batch.create(2, ViewKind::Text, 1, 0);
  FakeViews views;
  CellTree tree(views);
  tree.apply(batch);
  CHECK(views.log ==
        std::vector<std::string>{"removeAll", "create v1 kind 0", "add root v1 @0", "create v2 kind 1", "add v1 v2 @0"});
}

TEST_CASE("frames reach their views and the root frame sizes the cell") {
  OpBatch batch;
  batch.first = true;
  batch.create(1, ViewKind::View, 0, 0);
  batch.frame(0, 0, 0, 320, 64);
  batch.frame(1, 8, 8, 48, 48);
  FakeViews views;
  CellTree tree(views);
  tree.apply(batch);
  CHECK(views.log.back() == "frame v1 8 8 48 48");
  CHECK_FALSE(logged(views, "frame root"));
  CHECK(tree.rootWidth() == 320);
  CHECK(tree.rootHeight() == 64);
}

TEST_CASE("a removed view takes no later ops") {
  OpBatch batch;
  batch.first = true;
  batch.create(1, ViewKind::View, 0, 0);
  batch.remove(1);
  batch.viewStyle(1, Style{});
  FakeViews views;
  CellTree tree(views);
  tree.apply(batch);
  CHECK(views.log.back() == "remove root v1");
  CHECK_FALSE(logged(views, "style"));
}

TEST_CASE("a view style decodes every field") {
  Style s;
  s.backgroundColor = 0xFF112233u;
  s.borderColor = 0xFF445566u;
  s.shadowColor = 0xFF778899u;
  s.opacity = 0.5f;
  s.borderRadii = {1.f, 2.f, 3.f, 4.f};
  s.borderWidth = 2;
  s.elevation = 3;
  s.shadowOpacity = 0.25f;
  s.shadowRadius = 5;
  s.shadowOffset = std::pair<float, float>{1, -1};
  OpBatch batch;
  batch.create(1, ViewKind::View, 0, 0);
  batch.viewStyle(1, s);
  FakeViews views;
  CellTree tree(views);
  tree.apply(batch);
  CHECK(views.lastStyle.backgroundColor == 0xFF112233u);
  CHECK(views.lastStyle.borderColor == 0xFF445566u);
  CHECK(views.lastStyle.shadowColor == 0xFF778899u);
  CHECK(views.lastStyle.opacity == 0.5f);
  CHECK(views.lastStyle.radii == std::array<float, 4>{1, 2, 3, 4});
  CHECK(views.lastStyle.borderWidth == 2);
  CHECK(views.lastStyle.elevation == 3);
  CHECK(views.lastStyle.shadowOpacity == 0.25f);
  CHECK(views.lastStyle.shadowRadius == 5);
  CHECK(views.lastStyle.shadowOffsetX == 1);
  CHECK(views.lastStyle.shadowOffsetY == -1);
}

TEST_CASE("text and image ops decode their specs") {
  Style t;
  t.fontSize = 17;
  t.fontWeight = 600;
  t.numberOfLines = 2;
  t.fontFamily = "Menlo";
  t.italic = true;
  t.lineHeight = 22;
  t.letterSpacing = 0.5f;
  t.textAlign = TextAlign::Center;
  t.decoration = 1;
  t.color = 0xFF010203u;
  Style i;
  i.width = Dimension{Dimension::Unit::Point, 40};
  i.height = Dimension{Dimension::Unit::Point, 30};
  i.borderRadii = {6.f, 6.f, 0.f, 0.f};
  OpBatch batch;
  batch.create(1, ViewKind::Text, 0, 0);
  batch.text(1, "Hi", t, 200, false, 3, 4);
  batch.create(2, ViewKind::Image, 0, 1);
  batch.image(2, "a.png", i, ResizeMode::Contain, 0xFF0000FFu, {{"A", "1"}, {"B", "2"}});
  FakeViews views;
  CellTree tree(views);
  tree.apply(batch);
  CHECK(views.lastText.text == "Hi");
  CHECK(views.lastText.color == 0xFF010203u);
  CHECK(views.lastText.fontSize == 17);
  CHECK(views.lastText.fontWeight == 600);
  CHECK(views.lastText.numberOfLines == 2);
  CHECK(views.lastText.fontFamily == "Menlo");
  CHECK(views.lastText.italic);
  CHECK(views.lastText.lineHeight == 22);
  CHECK(views.lastText.letterSpacing == 0.5f);
  CHECK(views.lastText.textAlign == static_cast<int>(TextAlign::Center));
  CHECK(views.lastText.decoration == 1);
  CHECK(views.lastText.maxWidth == 200);
  CHECK_FALSE(views.lastText.allowFontScaling);
  CHECK(views.insetLeft == 3);
  CHECK(views.insetTop == 4);
  CHECK(views.lastImage.uri == "a.png");
  CHECK(views.lastImage.resizeMode == static_cast<int>(ResizeMode::Contain));
  CHECK(views.lastImage.tint == std::optional<uint32_t>{0xFF0000FFu});
  CHECK(views.lastImage.headers == Headers{{"A", "1"}, {"B", "2"}});
  CHECK(views.lastImage.width == 40);
  CHECK(views.lastImage.height == 30);
  CHECK(views.lastImage.radii == std::array<float, 4>{6, 6, 0, 0});
}

TEST_CASE("a text without a family decodes an empty family") {
  OpBatch batch;
  batch.create(1, ViewKind::Text, 0, 0);
  batch.text(1, "Hi", Style{}, 100, true, 0, 0);
  FakeViews views;
  CellTree tree(views);
  tree.apply(batch);
  CHECK(views.lastText.fontFamily.empty());
  CHECK(views.lastText.allowFontScaling);
}

TEST_CASE("pressables and accessibility reach their views") {
  OpBatch batch;
  batch.create(1, ViewKind::Pressable, 0, 0);
  batch.pressable(1, "like", 0.5f);
  batch.accessibility(1, "Like", "", 1, std::nullopt);
  FakeViews views;
  CellTree tree(views);
  tree.apply(batch);
  CHECK(views.log[views.log.size() - 2] == "pressable v1 like 0.5");
  CHECK(views.log.back() == "access v1 Like - 1 -1");
}

TEST_CASE("move reorders a child within its parent") {
  OpBatch batch;
  batch.first = true;
  batch.create(1, ViewKind::View, 0, 0);
  batch.create(2, ViewKind::View, 1, 0);
  batch.create(3, ViewKind::View, 1, 1);
  batch.move(3, 0);
  FakeViews views;
  CellTree tree(views);
  tree.apply(batch);
  CHECK(views.log.back() == "move v1 v3 @0");
}

TEST_CASE("an unknown op code stops the batch") {
  OpBatch batch;
  batch.ints = {42, 1};
  batch.create(1, ViewKind::View, 0, 0);
  FakeViews views;
  CellTree tree(views);
  tree.apply(batch);
  CHECK(views.log.empty());
}
