#pragma once

#include <hermes/hermes.h>
#include <jsi/jsi.h>

#include <memory>
#include <string>

namespace jsi = facebook::jsi;

// One Hermes runtime per test case; eval returns the completion value of the source.
struct HermesRig {
  std::unique_ptr<facebook::hermes::HermesRuntime> runtime = facebook::hermes::makeHermesRuntime();

  jsi::Runtime& rt() { return *runtime; }

  jsi::Value eval(const std::string& source) {
    return runtime->evaluateJavaScript(std::make_shared<jsi::StringBuffer>(source), "test");
  }

  std::string stringify(const jsi::Value& value) {
    jsi::Object json = rt().global().getPropertyAsObject(rt(), "JSON");
    jsi::Function fn = json.getPropertyAsFunction(rt(), "stringify");
    return fn.callWithThis(rt(), json, value).asString(rt()).utf8(rt());
  }
};
