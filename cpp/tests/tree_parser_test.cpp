#include "doctest.h"

#include "core/TreeParser.h"
#include "values.h"

TEST_CASE("parses the chat cell") {
  auto r = flume::parseTree(chatCell());
  REQUIRE(r.root);
  CHECK(r.error.empty());
  CHECK(r.root->type == flume::NodeType::View);
  CHECK(r.root->style.flexDirection == YGFlexDirectionRow);
  REQUIRE(r.root->children.size() == 2);
  CHECK(r.root->children[0]->type == flume::NodeType::Image);
  CHECK(r.root->children[0]->imageUri == "https://x/a.png");
  CHECK(r.root->children[0]->style.borderRadii[0] == 20.0f);
  auto& column = *r.root->children[1];
  CHECK(column.style.flex == 1.0f);
  REQUIRE(column.children.size() == 2);
  CHECK(column.children[0]->type == flume::NodeType::Text);
  CHECK(column.children[0]->text == "hello");
  CHECK(column.children[0]->style.fontSize == 16.0f);
  CHECK(column.children[1]->style.color == 0xFF666666u);
}

TEST_CASE("pressable parses as a view and object sources read uri") {
  auto r = flume::parseTree(obj({{"type", V("pressable")},
                                 {"id", V("like")},
                                 {"children", arr({obj({{"type", V("image")}, {"source", obj({{"uri", V("u")}})}})})}}));
  REQUIRE(r.root);
  CHECK(r.root->type == flume::NodeType::View);
  REQUIRE(r.root->children.size() == 1);
  CHECK(r.root->children[0]->imageUri == "u");
}

TEST_CASE("text options") {
  auto r = flume::parseTree(obj({{"type", V("text")}, {"text", V("a")}, {"allowFontScaling", V(false)}, {"key", V("k")}}));
  REQUIRE(r.root);
  CHECK_FALSE(r.root->allowFontScaling);
  CHECK(r.root->key == "k");
  CHECK(flume::parseTree(obj({{"type", V("text")}})).root->allowFontScaling);
}

TEST_CASE("errors") {
  CHECK(flume::parseTree(V(3)).error == "cell is not an element");
  CHECK(flume::parseTree(V(3)).root == nullptr);
  CHECK(flume::parseTree(obj({{"type", V("svg")}})).error == "unknown element type: svg");
  CHECK(flume::parseTree(obj({{"type", V("view")}, {"children", arr({V("x")})}})).error == "child 0 is not an element");
  CHECK(flume::parseTree(obj({{"type", V("view")}, {"style", obj({{"foo", V(1)}})}})).root != nullptr);
  CHECK(flume::parseTree(obj({{"type", V("view")}, {"children", arr({obj({{"type", V("nope")}})})}})).error ==
        "unknown element type: nope");
}

TEST_CASE("text and image elements drop their children") {
  auto text = flume::parseTree(
      obj({{"type", V("text")}, {"text", V("a")}, {"children", arr({obj({{"type", V("view")}})})}}));
  REQUIRE(text.root);
  CHECK(text.root->children.empty());
  auto image = flume::parseTree(
      obj({{"type", V("image")}, {"source", V("u")}, {"children", arr({obj({{"type", V("view")}})})}}));
  REQUIRE(image.root);
  CHECK(image.root->children.empty());
}
