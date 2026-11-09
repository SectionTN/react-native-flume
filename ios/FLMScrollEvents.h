#import <UIKit/UIKit.h>

#import <react/renderer/components/FlumeSpec/EventEmitters.h>

#include "adapter/ScrollEvents.h"

// The ScrollView payload in points, sent from the main thread; the scroll event is coalesced as React Native's own.
void FLMEmitScrollEvent(const facebook::react::FlumeViewEventEmitter &emitter, flume::ScrollEventKind kind,
                        UIScrollView *scrollView, BOOL horizontal, BOOL inverted, CGPoint velocity);
