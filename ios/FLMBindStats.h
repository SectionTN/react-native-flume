#import <Foundation/Foundation.h>

#include <array>
#include <cstdint>
#include <vector>

// Opt-in per-bind stage times over the last 2,000 recycled binds, logged every 200; main thread only.
@interface FLMBindStats : NSObject

// Turning stats on starts an empty window.
@property (class, nonatomic) BOOL enabled;
@property (class, nonatomic, readonly) NSString *summary;

// stats: worklet, reconcile and layout ns, measure calls, measure hits, and 1 for a first bind.
+ (void)record:(int64_t)totalNs stats:(const std::array<int64_t, 6> &)stats;
+ (void)log;

@end

// Nearest-rank percentile of an ascending vector; 0 when empty.
int64_t FLMBindStatsPercentile(const std::vector<int64_t> &sorted, double p);
