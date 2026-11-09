#import "FLMScrollEvents.h"

using namespace facebook;

namespace {

std::string FLMScrollEventName(flume::ScrollEventKind kind)
{
  switch (kind) {
    case flume::ScrollEventKind::Scroll:
      return "scroll";
    case flume::ScrollEventKind::BeginDrag:
      return "scrollBeginDrag";
    case flume::ScrollEventKind::EndDrag:
      return "scrollEndDrag";
    case flume::ScrollEventKind::MomentumBegin:
      return "momentumScrollBegin";
    case flume::ScrollEventKind::MomentumEnd:
      return "momentumScrollEnd";
  }
  return "scroll";
}

jsi::Object FLMPair(jsi::Runtime &runtime, const char *first, double a, const char *second, double b)
{
  auto object = jsi::Object(runtime);
  object.setProperty(runtime, first, a);
  object.setProperty(runtime, second, b);
  return object;
}

}  // namespace

void FLMEmitScrollEvent(const react::FlumeViewEventEmitter &emitter, flume::ScrollEventKind kind,
                        UIScrollView *scrollView, BOOL horizontal, BOOL inverted, CGPoint velocity)
{
  const CGSize content = scrollView.contentSize;
  const CGSize bounds = scrollView.bounds.size;
  const UIEdgeInsets inset = scrollView.contentInset;
  CGPoint offset = scrollView.contentOffset;
  if (horizontal) {
    offset.x = flume::contentStartOffset(offset.x, content.width, bounds.width, inverted);
    velocity.x = inverted ? -velocity.x : velocity.x;
  } else {
    offset.y = flume::contentStartOffset(offset.y, content.height, bounds.height, inverted);
    velocity.y = inverted ? -velocity.y : velocity.y;
  }
  auto payload = [=](jsi::Runtime &runtime) {
    auto object = jsi::Object(runtime);
    object.setProperty(runtime, "contentOffset", FLMPair(runtime, "x", offset.x, "y", offset.y));
    object.setProperty(runtime, "contentSize", FLMPair(runtime, "width", content.width, "height", content.height));
    object.setProperty(runtime, "layoutMeasurement", FLMPair(runtime, "width", bounds.width, "height", bounds.height));
    auto contentInset = jsi::Object(runtime);
    contentInset.setProperty(runtime, "top", inset.top);
    contentInset.setProperty(runtime, "left", inset.left);
    contentInset.setProperty(runtime, "bottom", inset.bottom);
    contentInset.setProperty(runtime, "right", inset.right);
    object.setProperty(runtime, "contentInset", contentInset);
    object.setProperty(runtime, "velocity", FLMPair(runtime, "x", velocity.x, "y", velocity.y));
    return object;
  };
  if (kind == flume::ScrollEventKind::Scroll) {
    emitter.dispatchUniqueEvent("scroll", payload);
  } else {
    emitter.dispatchEvent(FLMScrollEventName(kind), payload);
  }
}
