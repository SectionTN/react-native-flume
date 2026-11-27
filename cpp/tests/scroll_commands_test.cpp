#include "doctest.h"

#include "adapter/ScrollCommands.h"

using namespace flume;

namespace {

constexpr ScrollCommands::Rest kRest{};

}  // namespace

TEST_CASE("ScrollCommands ends the command in flight once") {
  ScrollCommands commands;
  commands.begin(3);
  CHECK(commands.pending() == 3);
  CHECK(commands.end() == 3);
  CHECK(commands.pending() == 0);
  CHECK(commands.end() == 0);
}

TEST_CASE("ScrollCommands reports nothing with no command") {
  ScrollCommands commands;
  CHECK(commands.end() == 0);
  CHECK(commands.settle(kRest) == 0);
}

TEST_CASE("ScrollCommands a newer command is the one reported") {
  ScrollCommands commands;
  commands.begin(3);
  commands.begin(4);
  CHECK(commands.end() == 4);
}

TEST_CASE("ScrollCommands reset forgets the command") {
  ScrollCommands commands;
  commands.begin(3);
  commands.land();
  commands.reset();
  CHECK(commands.end() == 0);
  CHECK(commands.settle(kRest) == 0);
}

TEST_CASE("ScrollCommands an end after reset and a new command reports the new one") {
  ScrollCommands commands;
  commands.begin(3);
  commands.reset();
  commands.begin(1);
  CHECK(commands.end() == 1);
}

TEST_CASE("ScrollCommands settles only at rest") {
  ScrollCommands commands;
  commands.begin(3);
  commands.land();
  CHECK(commands.settle({.moving = true}) == 0);
  CHECK(commands.settle({.dragging = true}) == 0);
  CHECK(commands.settle({.jumpWaiting = true}) == 0);
  CHECK(commands.settle({.realignPending = true}) == 0);
  CHECK(commands.settle(kRest) == 3);
  CHECK(commands.settle(kRest) == 0);
}

TEST_CASE("ScrollCommands does not settle before it lands") {
  ScrollCommands commands;
  commands.begin(3);
  CHECK(commands.settle(kRest) == 0);
  commands.land();
  CHECK(commands.settle(kRest) == 3);
}

TEST_CASE("ScrollCommands a newer command clears the landed mark") {
  ScrollCommands commands;
  commands.begin(3);
  commands.land();
  commands.begin(4);
  CHECK(commands.settle(kRest) == 0);
}

TEST_CASE("IntoView ends at once for a row fully in view") {
  CHECK_FALSE(intoViewPosition(5, 3, 8).has_value());
  CHECK_FALSE(intoViewPosition(3, 3, 8).has_value());
  CHECK_FALSE(intoViewPosition(8, 3, 8).has_value());
}

TEST_CASE("IntoView scrolls with viewPosition 0 before the visible rows and 1 after them") {
  CHECK(intoViewPosition(2, 3, 8) == 0.f);
  CHECK(intoViewPosition(9, 3, 8) == 1.f);
}

TEST_CASE("IntoView puts the row at the start when no row is fully in view") {
  CHECK(intoViewPosition(4, -1, -1) == 0.f);
}
