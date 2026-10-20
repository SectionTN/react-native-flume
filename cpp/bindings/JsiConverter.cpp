#include "bindings/JsiConverter.h"

namespace flume {

namespace jsi = facebook::jsi;

Value toValue(jsi::Runtime& rt, const jsi::Value& value) {
  if (value.isBool()) return Value(value.getBool());
  if (value.isNumber()) return Value(value.getNumber());
  if (value.isString()) return Value(value.getString(rt).utf8(rt));
  if (!value.isObject()) return Value();
  jsi::Object object = value.getObject(rt);
  if (object.isFunction(rt) || object.isHostObject(rt)) return Value();
  if (object.isArray(rt)) {
    jsi::Array array = object.getArray(rt);
    size_t size = array.size(rt);
    Value::Array items;
    items.reserve(size);
    for (size_t i = 0; i < size; i++) items.push_back(toValue(rt, array.getValueAtIndex(rt, i)));
    return Value(std::move(items));
  }
  jsi::Array names = object.getPropertyNames(rt);
  size_t size = names.size(rt);
  Value::Object entries;
  entries.reserve(size);
  for (size_t i = 0; i < size; i++) {
    jsi::String name = names.getValueAtIndex(rt, i).getString(rt);
    entries.emplace_back(name.utf8(rt), toValue(rt, object.getProperty(rt, name)));
  }
  return Value(std::move(entries));
}

jsi::Value toJsi(jsi::Runtime& rt, const Value& value) {
  switch (value.kind()) {
    case Value::Kind::Null: return jsi::Value::null();
    case Value::Kind::Bool: return jsi::Value(value.asBool());
    case Value::Kind::Number: return jsi::Value(value.asNumber());
    case Value::Kind::String: return jsi::String::createFromUtf8(rt, value.asString());
    case Value::Kind::Array: {
      const auto& items = value.asArray();
      jsi::Array array(rt, items.size());
      for (size_t i = 0; i < items.size(); i++) array.setValueAtIndex(rt, i, toJsi(rt, items[i]));
      return array;
    }
    case Value::Kind::Object: {
      jsi::Object object(rt);
      for (const auto& [key, entry] : value.asObject()) object.setProperty(rt, key.c_str(), toJsi(rt, entry));
      return object;
    }
  }
  return jsi::Value::undefined();
}

}  // namespace flume
