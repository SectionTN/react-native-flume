#import "FLMBindStats.h"

#import <os/log.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr size_t kWindow = 2000;
constexpr size_t kLogEvery = 200;

class FLMRing {
 public:
  void add(int64_t value)
  {
    values_[next_] = value;
    next_ = (next_ + 1) % kWindow;
    size_ = std::min(size_ + 1, kWindow);
  }

  std::vector<int64_t> sorted() const
  {
    std::vector<int64_t> out(values_.begin(), values_.begin() + size_);
    std::sort(out.begin(), out.end());
    return out;
  }

  int64_t sum() const
  {
    int64_t total = 0;
    for (size_t i = 0; i < size_; i++) {
      total += values_[i];
    }
    return total;
  }

  size_t size() const
  {
    return size_;
  }

  void clear()
  {
    next_ = 0;
    size_ = 0;
  }

 private:
  std::array<int64_t, kWindow> values_{};
  size_t next_ = 0;
  size_t size_ = 0;
};

struct FLMStatsState {
  FLMRing total, worklet, reconcile, layout, apply, measureCalls, measureHits, first, measure;
  size_t recycled = 0;
  bool enabled = false;
};

FLMStatsState &FLMState()
{
  static FLMStatsState state;
  return state;
}

NSString *FLMMs(int64_t ns)
{
  return [NSString stringWithFormat:@"%.2f", ns / 1'000'000.0];
}

NSString *FLMPair(const FLMRing &ring)
{
  std::vector<int64_t> sorted = ring.sorted();
  return [NSString stringWithFormat:@"%@/%@", FLMMs(FLMBindStatsPercentile(sorted, 0.5)),
                                    FLMMs(FLMBindStatsPercentile(sorted, 0.95))];
}

}  // namespace

int64_t FLMBindStatsPercentile(const std::vector<int64_t> &sorted, double p)
{
  if (sorted.empty()) {
    return 0;
  }
  auto rank = static_cast<size_t>(std::ceil(p * sorted.size()));
  return sorted[std::clamp<size_t>(rank, 1, sorted.size()) - 1];
}

@implementation FLMBindStats

+ (BOOL)enabled
{
  return FLMState().enabled;
}

+ (void)setEnabled:(BOOL)enabled
{
  FLMStatsState &s = FLMState();
  if (enabled) {
    for (FLMRing *ring : {&s.total, &s.worklet, &s.reconcile, &s.layout, &s.apply, &s.measureCalls, &s.measureHits,
                          &s.first, &s.measure}) {
      ring->clear();
    }
    s.recycled = 0;
  }
  s.enabled = enabled;
}

+ (void)record:(int64_t)totalNs stats:(const std::array<int64_t, 6> &)stats
{
  FLMStatsState &s = FLMState();
  if (!s.enabled) {
    return;
  }
  if (stats[5] == 1) {
    s.first.add(totalNs);
    return;
  }
  s.total.add(totalNs);
  s.worklet.add(stats[0]);
  s.reconcile.add(stats[1]);
  s.layout.add(stats[2]);
  s.apply.add(totalNs - stats[0] - stats[1] - stats[2]);
  s.measureCalls.add(stats[3]);
  s.measureHits.add(stats[4]);
  s.recycled++;
  if (s.recycled % kLogEvery == 0) {
    [self log];
  }
}

+ (void)recordMeasure:(int64_t)totalNs
{
  FLMStatsState &s = FLMState();
  if (s.enabled) {
    s.measure.add(totalNs);
  }
}

+ (void)log
{
  os_log(OS_LOG_DEFAULT, "%{public}@", self.summary);
  std::vector<int64_t> measure = FLMState().measure.sorted();
  os_log(OS_LOG_DEFAULT, "FlumeStats measure n=%zu total p50=%{public}@ p95=%{public}@", measure.size(),
         FLMMs(FLMBindStatsPercentile(measure, 0.5)), FLMMs(FLMBindStatsPercentile(measure, 0.95)));
}

+ (NSString *)summary
{
  FLMStatsState &s = FLMState();
  std::vector<int64_t> total = s.total.sorted();
  std::vector<int64_t> first = s.first.sorted();
  int64_t calls = s.measureCalls.sum();
  int64_t hitPercent = calls == 0 ? 0 : s.measureHits.sum() * 100 / calls;
  double perBind = s.total.size() == 0 ? 0.0 : static_cast<double>(calls) / s.total.size();
  return [NSString
      stringWithFormat:@"FlumeStats recycled n=%zu total p50=%@ p95=%@ max=%@ | worklet %@ | reconcile %@ | layout %@"
                       @" | apply %@ | measure %.1f/bind hits %lld%% | first n=%zu p50=%@",
                       s.total.size(), FLMMs(FLMBindStatsPercentile(total, 0.5)),
                       FLMMs(FLMBindStatsPercentile(total, 0.95)), FLMMs(FLMBindStatsPercentile(total, 1.0)),
                       FLMPair(s.worklet), FLMPair(s.reconcile), FLMPair(s.layout), FLMPair(s.apply), perBind,
                       hitPercent, s.first.size(), FLMMs(FLMBindStatsPercentile(first, 0.5))];
}

@end
