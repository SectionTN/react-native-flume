#include "doctest.h"

#include "bindings/ItemCache.h"
#include "hermes_rig.h"
#include "values.h"

TEST_CASE("materializes once per key and version") {
  HermesRig rig;
  flume::ItemCache cache;
  const jsi::Value& a = cache.materialize(rig.rt(), "a", 1, obj({{"n", V(1)}}));
  const jsi::Value& again = cache.materialize(rig.rt(), "a", 1, obj({{"n", V(1)}}));
  CHECK(cache.conversions == 1);
  CHECK(jsi::Object::strictEquals(rig.rt(), a.asObject(rig.rt()), again.asObject(rig.rt())));
}

TEST_CASE("a new version converts again") {
  HermesRig rig;
  flume::ItemCache cache;
  cache.materialize(rig.rt(), "a", 1, obj({{"n", V(1)}}));
  const jsi::Value& v2 = cache.materialize(rig.rt(), "a", 2, obj({{"n", V(2)}}));
  CHECK(cache.conversions == 2);
  CHECK(v2.asObject(rig.rt()).getProperty(rig.rt(), "n").asNumber() == 2);
}

TEST_CASE("retain drops the keys that left the data") {
  HermesRig rig;
  flume::ItemCache cache;
  for (const char* key : {"a", "b", "c"}) cache.materialize(rig.rt(), key, 1, obj({}));
  cache.retain({"b"});
  CHECK(cache.size() == 1);
  cache.materialize(rig.rt(), "b", 1, obj({}));
  CHECK(cache.conversions == 3);
}
