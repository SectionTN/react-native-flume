#pragma once

#include <jsi/jsi.h>

#include <vector>

namespace flume {

// globalThis.__flume on the React runtime. A worklet that captures it reaches install() on the flume runtime.
class FlumeHost : public facebook::jsi::HostObject {
 public:
  static void install(facebook::jsi::Runtime& reactRuntime);

  facebook::jsi::Value get(facebook::jsi::Runtime& rt, const facebook::jsi::PropNameID& name) override;
  std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime& rt) override;
};

}  // namespace flume
