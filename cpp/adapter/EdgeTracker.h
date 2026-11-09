#pragma once

namespace flume {

struct EdgeDistances {
  float fromStart;  // in content order
  float fromEnd;
};

// From the visible area inside the insets; an inverted list's content start is its visual bottom.
EdgeDistances edgeDistances(float offset, float contentLength, float boundsLength, float insetStart, float insetEnd,
                            bool inverted);

struct EdgeHits {
  bool start;
  bool end;
  EdgeDistances distances;
};

// FlatList's rule per item count: fires once within the threshold and re-arms outside it.
class EdgeTracker {
 public:
  EdgeHits update(EdgeDistances distances, int itemCount, float visibleLength, double startThreshold,
                  double endThreshold);

 private:
  int sentStart_ = 0;  // the item count last sent; 0 also keeps an empty list from firing
  int sentEnd_ = 0;
};

}  // namespace flume
