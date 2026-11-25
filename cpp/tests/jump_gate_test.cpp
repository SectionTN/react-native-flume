#include "doctest.h"

#include <string>

#include "adapter/JumpGate.h"

using namespace flume;

TEST_CASE("a jump runs once the last slot renders") {
  JumpGate<std::string> gate;
  CHECK_FALSE(gate.request("a", {1, 2}, 0));
  CHECK_FALSE(gate.rendered(1));
  CHECK(gate.rendered(2) == "a");
  CHECK_FALSE(gate.pending());
}

TEST_CASE("a newer jump replaces the waiting one") {
  JumpGate<std::string> gate;
  gate.request("a", {1}, 0);
  CHECK(gate.request("b", {2}, 10) == "a");
  CHECK_FALSE(gate.rendered(1));
  CHECK(gate.rendered(2) == "b");
}

TEST_CASE("a drag cancels the waiting jump") {
  JumpGate<std::string> gate;
  gate.request("a", {1}, 0);
  CHECK(gate.cancel() == "a");
  CHECK_FALSE(gate.rendered(1));
}

TEST_CASE("a jump runs anyway after the wait") {
  JumpGate<std::string> gate(1000);
  gate.request("a", {1}, 0);
  CHECK_FALSE(gate.due(999));
  CHECK(gate.due(1000) == "a");
  CHECK_FALSE(gate.pending());
}

TEST_CASE("a rendered slot of no jump is ignored") {
  JumpGate<std::string> gate;
  CHECK_FALSE(gate.rendered(5));
  gate.request("a", {1}, 0);
  CHECK_FALSE(gate.rendered(5));
  CHECK(gate.pending() == "a");
}

TEST_CASE("a retarget keeps the first deadline") {
  JumpGate<std::string> gate(1000);
  gate.request("a", {1}, 0);
  CHECK(gate.retarget("b", {2}) == "a");
  CHECK_FALSE(gate.due(999));
  CHECK(gate.due(1000) == "b");
}

TEST_CASE("a retarget waits for its own slots") {
  JumpGate<std::string> gate;
  gate.request("a", {1}, 0);
  gate.retarget("b", {2});
  CHECK_FALSE(gate.rendered(1));
  CHECK(gate.rendered(2) == "b");
}

TEST_CASE("a retarget with no jump waiting does nothing") {
  JumpGate<std::string> gate;
  CHECK_FALSE(gate.retarget("b", {2}));
  CHECK_FALSE(gate.pending());
}

TEST_CASE("a jump with no slots waits for its deadline") {
  JumpGate<std::string> gate(1000);
  gate.request("a", {}, 0);
  CHECK_FALSE(gate.rendered(1));
  CHECK(gate.due(1000) == "a");
}
