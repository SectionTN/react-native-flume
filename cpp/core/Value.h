#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace flume {

// The item and tree value type: what a JS value becomes once it leaves a runtime.
class Value {
 public:
  enum class Kind : uint8_t { Null, Bool, Number, String, Array, Object };
  using Array = std::vector<Value>;
  using Object = std::vector<std::pair<std::string, Value>>;

  Value() = default;
  explicit Value(bool b) : v_(b) {}
  explicit Value(double n) : v_(n) {}
  explicit Value(std::string s) : v_(std::move(s)) {}
  explicit Value(Array a) : v_(std::move(a)) {}
  explicit Value(Object o) : v_(std::move(o)) {}

  // The variant order matches Kind.
  Kind kind() const { return static_cast<Kind>(v_.index()); }

  bool asBool() const {
    const bool* p = std::get_if<bool>(&v_);
    return p != nullptr && *p;
  }
  double asNumber() const {
    const double* p = std::get_if<double>(&v_);
    return p != nullptr ? *p : 0.0;
  }
  const std::string& asString() const {
    static const std::string empty;
    const std::string* p = std::get_if<std::string>(&v_);
    return p != nullptr ? *p : empty;
  }
  const Array& asArray() const {
    static const Array empty;
    const Array* p = std::get_if<Array>(&v_);
    return p != nullptr ? *p : empty;
  }
  const Object& asObject() const {
    static const Object empty;
    const Object* p = std::get_if<Object>(&v_);
    return p != nullptr ? *p : empty;
  }
  // Object only; nullptr when absent. Objects are small, so a scan beats a map.
  const Value* get(std::string_view key) const {
    const Object* o = std::get_if<Object>(&v_);
    if (o == nullptr) return nullptr;
    for (const auto& entry : *o) {
      if (entry.first == key) return &entry.second;
    }
    return nullptr;
  }

 private:
  std::variant<std::monostate, bool, double, std::string, Array, Object> v_;
};

}  // namespace flume
