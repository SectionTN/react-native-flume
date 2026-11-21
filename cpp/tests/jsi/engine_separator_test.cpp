#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "batch.h"
#include "bindings/Engine.h"
#include "bindings/FlumeHost.h"
#include "doctest.h"
#include "fake_measurer.h"
#include "fake_worklets.h"
#include "hermes_rig.h"

namespace {

// A React runtime with __flume and a flume runtime installed, so rows bind.
struct ReactSide {
  HermesRig rig;
  std::shared_ptr<QueueInvoker> invoker = std::make_shared<QueueInvoker>();
  ReactSide() {
    flume::FlumeHost::install(rig.rt(), invoker);
    installFlumeRuntime(rig.rt());
  }
};

struct NullListener : flume::StoreListener {
  void onListChanged(const flume::ListOps&, int) override {}
};

// Rows 10 pt tall, typed by typeIds, and a separator 1 pt tall whose text names its leading and trailing items.
int separatedStore(ReactSide& side, flume::StoreListener& listener, int count, const std::string& typeIds,
                   const std::string& reactTypes = "[]") {
  std::string keys, items;
  for (int i = 0; i < count; i++) {
    keys += (i ? ", '" : "'") + std::to_string(i) + "'";
    items += (i ? ", {n: " : "{n: ") + std::to_string(i) + "}";
  }
  std::string indices;
  for (int i = 0; i < count; i++) indices += (i ? ", " : "") + std::to_string(i);
  int id = static_cast<int>(
      side.rig
          .eval("__flume.setScheduler({});"
                "var id = __flume.createStore();"
                "__flume.setCellKinds(id, false, " +
                reactTypes +
                ");"
                "__flume.setData(id, [" +
                keys + "], ['row', 'react'], new Int32Array([" + typeIds + "]), [" + indices + "], [" + items +
                "], true, false);"
                "__flume.setCell(id, function (item, index) {"
                "  'show source';"
                "  return {type: 'view', style: {height: 10}};"
                "});"
                "__flume.setSeparatorCell(id, function (info) {"
                "  'show source';"
                "  var trailing = info.trailingItem === undefined ? 'none' : String(info.trailingItem.n);"
                "  return {type: 'text', text: info.leadingItem.n + '-' + trailing, style: {height: 1}};"
                "});"
                "id")
          .asNumber());
  REQUIRE(flume::Engine::instance().attach(id, &listener, std::make_unique<FakeMeasurer>(), 1) == count);
  return id;
}

flume::OpBatch bound(int id, int index) {
  flume::OpBatch out;
  REQUIRE(flume::Engine::instance().bind(id, index, index, 300, NAN, out));
  return out;
}

bool says(const flume::OpBatch& batch, const std::string& text) {
  return std::find(batch.strings.begin(), batch.strings.end(), text) != batch.strings.end();
}

}  // namespace

TEST_CASE("a separator worklet draws under every row but the last, with the next item") {
  ReactSide side;
  NullListener listener;
  int id = separatedStore(side, listener, 3, "0, 0, 0");
  auto first = bound(id, 0);
  CHECK(says(first, "0-1"));
  CHECK(first.rootHeight == 11);
  auto last = bound(id, 2);
  CHECK_FALSE(says(last, "2-none"));
  CHECK(last.rootHeight == 10);
  // The last row keeps the composed root, so it never switches shape.
  CHECK(frameOf(last, 1).has_value());
}

TEST_CASE("a separator before a React row has no trailing item") {
  ReactSide side;
  NullListener listener;
  int id = separatedStore(side, listener, 3, "0, 0, 1", "['react']");
  CHECK(says(bound(id, 1), "1-none"));
}

TEST_CASE("an inverted row shape puts the separator before the row") {
  ReactSide side;
  NullListener listener;
  int id = separatedStore(side, listener, 3, "0, 0, 0");
  flume::Engine::instance().setRowShape(id, {false, true, 1});
  auto out = bound(id, 0);
  // The separator is created first: view 1 at the top, the row view 2 under it.
  auto divider = frameOf(out, 1);
  auto row = frameOf(out, 2);
  REQUIRE(divider);
  REQUIRE(row);
  CHECK((*divider)[1] == 0);
  CHECK((*row)[1] == 1);
}

TEST_CASE("three lanes leave the last grid row without a separator") {
  ReactSide side;
  NullListener listener;
  int id = separatedStore(side, listener, 10, "0, 0, 0, 0, 0, 0, 0, 0, 0, 0");
  flume::Engine::instance().setRowShape(id, {false, false, 3});
  CHECK(says(bound(id, 8), "8-9"));
  CHECK(bound(id, 9).rootHeight == 10);
}

TEST_CASE("clearing the separator worklet binds plain rows again") {
  ReactSide side;
  NullListener listener;
  int id = separatedStore(side, listener, 3, "0, 0, 0");
  side.rig.eval("__flume.setSeparatorCell(id, null)");
  auto out = bound(id, 0);
  CHECK(out.rootHeight == 10);
  CHECK_FALSE(frameOf(out, 1).has_value());
}
