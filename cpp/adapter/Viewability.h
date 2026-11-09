#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace flume {

struct ViewabilityConfig {
  double itemVisiblePercentThreshold = 0;
  double viewAreaCoveragePercentThreshold = 0;  // above 0 selects the area rule
  double minimumViewTimeMs = 0;
  bool waitForInteraction = false;
};

struct ItemSpan {
  int index;
  float start, end;  // along the scroll axis, from the viewport's start
};

struct ViewToken {
  int index;
  std::string key;
  bool operator==(const ViewToken&) const = default;
};

struct ViewableChange {
  ViewToken token;
  bool isViewable;
  bool operator==(const ViewableChange&) const = default;
};

struct ViewabilityReport {
  std::vector<ViewToken> viewable;
  std::vector<ViewableChange> changed;
};

// FlatList's rule: an overlapping item counts when it is entirely visible or its visible share reaches the threshold.
class ViewabilityTracker {
 public:
  ViewabilityConfig config;
  bool interacted = false;

  std::vector<int> compute(float viewportLength, const std::vector<ItemSpan>& items) const;  // ascending
  // Against the last computed set, which this records; after a data change the next set counts as changed.
  bool changed(const std::vector<int>& viewable);
  // The computed set with its keys when it changed; keys are read once per change.
  std::optional<std::vector<ViewToken>> keyed(const std::vector<int>& viewable,
                                              const std::function<std::string(int)>& keyAt);
  // Against the last reported set by key, so rows that only moved report nothing: exits first, then entries.
  std::optional<ViewabilityReport> report(const std::vector<ViewToken>& viewable);
  // After minimumViewTime only the scheduled rows still in the computed set count, at their indices now.
  std::optional<ViewabilityReport> reportDelayed(const std::vector<ViewToken>& scheduled);
  void invalidate() { dirty_ = true; }

 private:
  std::vector<int> lastComputed_;
  bool dirty_ = false;
  std::vector<ViewToken> current_;
  std::vector<ViewToken> lastReported_;
};

}  // namespace flume
