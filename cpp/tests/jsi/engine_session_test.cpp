#include <cmath>
#include <memory>

#include <jsi/instrumentation.h>

#include "bindings/Engine.h"
#include "bindings/FlumeHost.h"
#include "doctest.h"
#include "fake_measurer.h"
#include "fake_worklets.h"
#include "hermes_rig.h"

namespace {

// A React runtime with __flume installed, as the bindings installer leaves it.
struct ReactSide {
  HermesRig rig;
  std::shared_ptr<QueueInvoker> invoker = std::make_shared<QueueInvoker>();
  ReactSide() { flume::FlumeHost::install(rig.rt(), invoker); }
};

struct NullListener : flume::StoreListener {
  void onListChanged(const flume::ListOps&, int) override {}
};

}  // namespace

TEST_CASE("a reload drops the reporter of the runtime it destroys") {
  auto before = std::make_unique<ReactSide>();
  before->rig.eval("__flume.setReporter(function () {})");
  before.reset();
  ReactSide after;
  after.rig.eval("globalThis.reported = ''; __flume.setReporter(function (level, message) { reported = message; })");
  flume::Engine::instance().warn("reported after a reload");
  after.invoker->drain(after.rig.rt());
  CHECK(after.rig.eval("reported").asString(after.rig.rt()).utf8(after.rig.rt()) == "reported after a reload");
}

TEST_CASE("a reload releases the flume runtime of the runtime it destroys") {
  auto before = std::make_unique<ReactSide>();
  auto flume = installFlumeRuntime(before->rig.rt());
  REQUIRE_FALSE(flume.expired());
  before.reset();
  CHECK(flume.expired());
  ReactSide after;
  CHECK_FALSE(installFlumeRuntime(after.rig.rt()).expired());
}

TEST_CASE("a reload sweeps the stores its runtime never released") {
  auto before = std::make_unique<ReactSide>();
  auto flume = installFlumeRuntime(before->rig.rt());
  int id = static_cast<int>(
      before->rig
          .eval("__flume.setScheduler({});"
                "var id = __flume.createStore();"
                "__flume.setData(id, ['a', 'b'], ['row'], new Int32Array([0, 0]), [0, 1], [{n: 1}, {n: 2}], true, false);"
                "__flume.setCell(id, function (item, index) {"
                "  'show source';"
                "  return {type: 'view', style: {height: 10}};"
                "});"
                "id")
          .asNumber());
  NullListener listener;
  auto& engine = flume::Engine::instance();
  REQUIRE(engine.attach(id, &listener, std::make_unique<FakeMeasurer>(), 1) == 2);
  flume::OpBatch out;
  REQUIRE(engine.bind(id, 1, 0, 300, NAN, out));
  REQUIRE_FALSE(out.ints.empty());
  before.reset();
  CHECK(engine.count(id) == 0);
  CHECK(flume.expired());
}

TEST_CASE("a warning reported before a reload is reported again after it") {
  auto before = std::make_unique<ReactSide>();
  before->rig.eval("__flume.setReporter(function () {})");
  flume::Engine::instance().warn("reported once per runtime");
  before->invoker->drain(before->rig.rt());
  before.reset();
  ReactSide after;
  after.rig.eval("globalThis.reported = ''; __flume.setReporter(function (level, message) { reported = message; })");
  flume::Engine::instance().warn("reported once per runtime");
  after.invoker->drain(after.rig.rt());
  CHECK(after.rig.eval("reported").asString(after.rig.rt()).utf8(after.rig.rt()) == "reported once per runtime");
}

namespace {

// A store with two rows, attached and bound once, as a mounted list leaves it.
int mountedStore(ReactSide& side, flume::StoreListener& listener) {
  int id = static_cast<int>(
      side.rig
          .eval("__flume.setScheduler({});"
                "var id = __flume.createStore();"
                "__flume.setData(id, ['a', 'b'], ['row'], new Int32Array([0, 0]), [0, 1], [{n: 1}, {n: 2}], true, false);"
                "__flume.setCell(id, function (item, index) {"
                "  'show source';"
                "  return {type: 'view', style: {height: 10}};"
                "});"
                "id")
          .asNumber());
  auto& engine = flume::Engine::instance();
  REQUIRE(engine.attach(id, &listener, std::make_unique<FakeMeasurer>(), 1) == 2);
  flume::OpBatch out;
  REQUIRE(engine.bind(id, 1, 0, 300, NAN, out));
  return id;
}

}  // namespace

TEST_CASE("deleting or overwriting the session global keeps the session") {
  ReactSide side;
  auto flume = installFlumeRuntime(side.rig.rt());
  NullListener listener;
  int id = mountedStore(side, listener);
  side.rig.eval("delete globalThis.__flumeSession; globalThis.__flumeSession = 0;");
  side.rig.rt().instrumentation().collectGarbage("test");
  CHECK(flume::Engine::instance().count(id) == 2);
  CHECK_FALSE(flume.expired());
}

TEST_CASE("the session global is read-only and hidden from enumeration") {
  ReactSide side;
  CHECK(side.rig
            .eval("var d = Object.getOwnPropertyDescriptor(globalThis, '__flumeSession');"
                  "!d.writable && !d.enumerable && !d.configurable && Object.keys(globalThis).indexOf('__flumeSession') < 0")
            .getBool());
}

TEST_CASE("a second install on the same runtime keeps its one session") {
  ReactSide side;
  auto flume = installFlumeRuntime(side.rig.rt());
  NullListener listener;
  int id = mountedStore(side, listener);
  CHECK_NOTHROW(flume::FlumeHost::install(side.rig.rt(), side.invoker));
  side.rig.rt().instrumentation().collectGarbage("test");
  CHECK(flume::Engine::instance().count(id) == 2);
  CHECK_FALSE(flume.expired());
}
