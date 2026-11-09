#include "adapter/EdgeTracker.h"

#include <algorithm>

namespace flume {

EdgeDistances edgeDistances(float offset, float contentLength, float boundsLength, float insetStart, float insetEnd,
                            bool inverted) {
  const float fromTop = std::max(0.f, offset + insetStart);
  const float fromBottom = std::max(0.f, contentLength - (offset + boundsLength - insetEnd));
  return inverted ? EdgeDistances{fromBottom, fromTop} : EdgeDistances{fromTop, fromBottom};
}

EdgeHits EdgeTracker::update(EdgeDistances distances, int itemCount, float visibleLength, double startThreshold,
                             double endThreshold) {
  const bool withinStart = distances.fromStart <= startThreshold * visibleLength;
  const bool withinEnd = distances.fromEnd <= endThreshold * visibleLength;
  const bool start = withinStart && itemCount != sentStart_;
  const bool end = withinEnd && itemCount != sentEnd_;
  sentStart_ = !withinStart ? 0 : (start ? itemCount : sentStart_);
  sentEnd_ = !withinEnd ? 0 : (end ? itemCount : sentEnd_);
  return EdgeHits{start, end, distances};
}

}  // namespace flume
