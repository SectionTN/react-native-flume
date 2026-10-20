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

TEST_CASE("a cyclic object converts to a finite value") {
  HermesRig rig;
  bool truncated = false;
  flume::Value value =
      flume::toValue(rig.rt(), rig.eval("(() => { const o = { n: 1 }; o.self = o; return o; })()"), &truncated);
  CHECK(truncated);
  const flume::Value* cursor = &value;
  int depth = 0;
  while (cursor->kind() == flume::Value::Kind::Object) {
    cursor = cursor->get("self");
    depth++;
    REQUIRE(cursor != nullptr);
  }
  CHECK(depth <= 65);
}
