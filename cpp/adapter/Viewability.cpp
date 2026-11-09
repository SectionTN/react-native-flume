#include "adapter/Viewability.h"

#include <algorithm>
#include <unordered_set>

namespace flume {

std::vector<int> ViewabilityTracker::compute(float viewportLength, const std::vector<ItemSpan>& items) const {
  std::vector<int> out;
  if (viewportLength <= 0 || (config.waitForInteraction && !interacted)) return out;
  // A threshold of 0 counts the same rows in either mode, so 0 reads as unset.
  const bool areaMode = config.viewAreaCoveragePercentThreshold > 0;
  const double threshold = areaMode ? config.viewAreaCoveragePercentThreshold : config.itemVisiblePercentThreshold;
  for (const ItemSpan& item : items) {
    if (item.start >= viewportLength || item.end <= 0) continue;
    const bool inside = item.start >= 0 && item.end <= viewportLength && item.end > item.start;
    if (!inside) {
      const double visible = std::max(0.f, std::min(item.end, viewportLength) - std::max(item.start, 0.f));
      const double whole = areaMode ? viewportLength : item.end - item.start;
      if (whole <= 0 || 100.0 * visible / whole < threshold) continue;
    }
    out.push_back(item.index);
  }
  std::sort(out.begin(), out.end());
  return out;
}

bool ViewabilityTracker::changed(const std::vector<int>& viewable) {
  const bool result = dirty_ || viewable != lastComputed_;
  dirty_ = false;
  lastComputed_ = viewable;
  return result;
}

std::optional<std::vector<ViewToken>> ViewabilityTracker::keyed(const std::vector<int>& viewable,
                                                               const std::function<std::string(int)>& keyAt) {
  if (!changed(viewable)) return std::nullopt;
  current_.clear();
  for (int index : viewable) current_.push_back(ViewToken{index, keyAt(index)});
  return current_;
}

std::optional<ViewabilityReport> ViewabilityTracker::report(const std::vector<ViewToken>& viewable) {
  std::unordered_set<std::string> previous;
  std::unordered_set<std::string> next;
  for (const ViewToken& token : lastReported_) previous.insert(token.key);
  for (const ViewToken& token : viewable) next.insert(token.key);
  std::vector<ViewableChange> changes;
  for (const ViewToken& token : lastReported_) {
    if (next.count(token.key) == 0) changes.push_back(ViewableChange{token, false});
  }
  for (const ViewToken& token : viewable) {
    if (previous.count(token.key) == 0) changes.push_back(ViewableChange{token, true});
  }
  lastReported_ = viewable;
  if (changes.empty()) return std::nullopt;
  return ViewabilityReport{viewable, std::move(changes)};
}

std::optional<ViewabilityReport> ViewabilityTracker::reportDelayed(const std::vector<ViewToken>& scheduled) {
  std::unordered_set<std::string> keys;
  for (const ViewToken& token : scheduled) keys.insert(token.key);
  std::vector<ViewToken> still;
  for (const ViewToken& token : current_) {
    if (keys.count(token.key) > 0) still.push_back(token);
  }
  return report(still);
}

}  // namespace flume
