#include "core/TreeParser.h"

#include "core/Color.h"

namespace flume {
namespace {

const std::string* stringAt(const Value& value, const char* key) {
  const Value* v = value.get(key);
  return v != nullptr && v->kind() == Value::Kind::String ? &v->asString() : nullptr;
}

std::optional<ResizeMode> resizeModeOf(const std::string& name) {
  if (name == "cover") return ResizeMode::Cover;
  if (name == "contain") return ResizeMode::Contain;
  if (name == "stretch") return ResizeMode::Stretch;
  if (name == "center") return ResizeMode::Center;
  return std::nullopt;
}

void parseSource(const Value& source, Node& node) {
  if (source.kind() == Value::Kind::String) {
    node.imageUri = source.asString();
    return;
  }
  if (const std::string* uri = stringAt(source, "uri")) node.imageUri = *uri;
  if (const Value* headers = source.get("headers"); headers != nullptr) {
    for (const auto& [name, value] : headers->asObject()) {
      if (value.kind() == Value::Kind::String) node.imageHeaders.emplace_back(name, value.asString());
    }
  }
}

std::unique_ptr<Node> parseNode(const Value& value, std::string& error, std::vector<std::string>* unknownKeys) {
  const std::string* type = stringAt(value, "type");
  if (type == nullptr) {
    error = "cell is not an element";
    return nullptr;
  }
  auto node = std::make_unique<Node>();
  if (*type == "view") {
    node->type = NodeType::View;
  } else if (*type == "pressable") {
    node->type = NodeType::Pressable;
  } else if (*type == "text") {
    node->type = NodeType::Text;
  } else if (*type == "image") {
    node->type = NodeType::Image;
  } else {
    error = "unknown element type: " + *type;
    return nullptr;
  }
  if (const Value* style = value.get("style"); style != nullptr && style->kind() == Value::Kind::Object) {
    node->style = Style::fromValue(*style, unknownKeys);
  }
  if (const std::string* text = stringAt(value, "text")) node->text = *text;
  if (const Value* scaling = value.get("allowFontScaling");
      scaling != nullptr && scaling->kind() == Value::Kind::Bool) {
    node->allowFontScaling = scaling->asBool();
  }
  if (const std::string* key = stringAt(value, "key")) node->key = *key;
  if (const std::string* id = stringAt(value, "id")) node->pressId = *id;
  if (const Value* opacity = value.get("pressedOpacity"); opacity != nullptr && opacity->kind() == Value::Kind::Number) {
    node->pressedOpacity = static_cast<float>(opacity->asNumber());
  }
  if (const std::string* testID = stringAt(value, "testID")) node->testID = *testID;
  if (const std::string* label = stringAt(value, "accessibilityLabel")) node->accessibilityLabel = *label;
  if (const Value* accessible = value.get("accessible");
      accessible != nullptr && accessible->kind() == Value::Kind::Bool) {
    node->accessible = accessible->asBool();
  }
  if (const Value* source = value.get("source"); source != nullptr) parseSource(*source, *node);
  // The element props win over the same keys given in the style.
  node->resizeMode = node->style.resizeMode.value_or(ResizeMode::Cover);
  if (const std::string* mode = stringAt(value, "resizeMode")) {
    if (auto parsed = resizeModeOf(*mode)) node->resizeMode = *parsed;
  }
  node->tintColor = node->style.tintColor;
  if (const std::string* tint = stringAt(value, "tintColor")) {
    if (auto color = parseColor(*tint)) node->tintColor = *color;
  }
  // Text and image are leaves: Yoga refuses children under a node with a measure function.
  bool container = node->type == NodeType::View || node->type == NodeType::Pressable;
  if (const Value* children = value.get("children");
      container && children != nullptr && children->kind() == Value::Kind::Array) {
    const auto& items = children->asArray();
    node->children.reserve(items.size());
    for (size_t i = 0; i < items.size(); i++) {
      if (items[i].kind() != Value::Kind::Object) {
        error = "child " + std::to_string(i) + " is not an element";
        return nullptr;
      }
      auto child = parseNode(items[i], error, unknownKeys);
      if (child == nullptr) return nullptr;
      node->children.push_back(std::move(child));
    }
  }
  return node;
}

}  // namespace

ParseResult parseTree(const Value& value, std::vector<std::string>* unknownKeys) {
  ParseResult result;
  if (value.kind() != Value::Kind::Object) {
    result.error = "cell is not an element";
    return result;
  }
  result.root = parseNode(value, result.error, unknownKeys);
  return result;
}

}  // namespace flume
