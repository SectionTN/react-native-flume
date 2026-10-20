#include "core/TreeParser.h"

namespace flume {
namespace {

std::unique_ptr<Node> parseNode(const Value& value, std::string& error) {
  const Value* type = value.get("type");
  if (type == nullptr || type->kind() != Value::Kind::String) {
    error = "cell is not an element";
    return nullptr;
  }
  auto node = std::make_unique<Node>();
  const std::string& name = type->asString();
  if (name == "view" || name == "pressable") {
    node->type = NodeType::View;
  } else if (name == "text") {
    node->type = NodeType::Text;
  } else if (name == "image") {
    node->type = NodeType::Image;
  } else {
    error = "unknown element type: " + name;
    return nullptr;
  }
  if (const Value* style = value.get("style"); style != nullptr && style->kind() == Value::Kind::Object) {
    node->style = Style::fromValue(*style);
  }
  if (const Value* text = value.get("text"); text != nullptr && text->kind() == Value::Kind::String) {
    node->text = text->asString();
  }
  if (const Value* scaling = value.get("allowFontScaling");
      scaling != nullptr && scaling->kind() == Value::Kind::Bool) {
    node->allowFontScaling = scaling->asBool();
  }
  if (const Value* key = value.get("key"); key != nullptr && key->kind() == Value::Kind::String) {
    node->key = key->asString();
  }
  if (const Value* source = value.get("source"); source != nullptr) {
    if (source->kind() == Value::Kind::String) {
      node->imageUri = source->asString();
    } else if (const Value* uri = source->get("uri"); uri != nullptr && uri->kind() == Value::Kind::String) {
      node->imageUri = uri->asString();
    }
  }
  // Text and image are leaves: Yoga refuses children under a node with a measure function.
  if (const Value* children = value.get("children");
      node->type == NodeType::View && children != nullptr && children->kind() == Value::Kind::Array) {
    const auto& items = children->asArray();
    node->children.reserve(items.size());
    for (size_t i = 0; i < items.size(); i++) {
      if (items[i].kind() != Value::Kind::Object) {
        error = "child " + std::to_string(i) + " is not an element";
        return nullptr;
      }
      auto child = parseNode(items[i], error);
      if (child == nullptr) return nullptr;
      node->children.push_back(std::move(child));
    }
  }
  return node;
}

}  // namespace

ParseResult parseTree(const Value& value) {
  ParseResult result;
  if (value.kind() != Value::Kind::Object) {
    result.error = "cell is not an element";
    return result;
  }
  result.root = parseNode(value, result.error);
  return result;
}

}  // namespace flume
