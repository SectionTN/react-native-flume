#include "doctest.h"

#include "bindings/JsiConverter.h"
#include "hermes_rig.h"

TEST_CASE("plain data round trips through Value") {
  HermesRig rig;
  jsi::Value in = rig.eval("({ a: 1, b: 'x', c: [true, null], d: { e: 2.5 } })");
  flume::Value value = flume::toValue(rig.rt(), in);
  REQUIRE(value.get("b") != nullptr);
  CHECK(value.get("b")->asString() == "x");
  CHECK(value.get("c")->asArray().size() == 2);
  jsi::Value out = flume::toJsi(rig.rt(), value);
  CHECK(rig.stringify(out) == "{\"a\":1,\"b\":\"x\",\"c\":[true,null],\"d\":{\"e\":2.5}}");
}

TEST_CASE("functions and host objects become null") {
  HermesRig rig;
  CHECK(flume::toValue(rig.rt(), rig.eval("(() => 1)")).kind() == flume::Value::Kind::Null);
}
