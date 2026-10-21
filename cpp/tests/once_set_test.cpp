#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "core/OnceSet.h"
#include "doctest.h"

TEST_CASE("a message is first exactly once") {
  flume::OnceSet once;
  CHECK(once.first("a"));
  CHECK_FALSE(once.first("a"));
  CHECK(once.first("b"));
}

TEST_CASE("firsts are counted once across threads") {
  flume::OnceSet once;
  std::atomic<int> firsts{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; t++) {
    threads.emplace_back([&] {
      for (int i = 0; i < 2000; i++) {
        if (once.first("message " + std::to_string(i))) firsts++;
      }
    });
  }
  for (auto& thread : threads) thread.join();
  CHECK(firsts == 2000);
}
