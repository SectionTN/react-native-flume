#pragma once

#include <react/renderer/components/FlumeSpec/EventEmitters.h>
#include <react/renderer/components/FlumeSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/graphics/Point.h>
#include <jsi/jsi.h>
#ifdef RN_SERIALIZABLE_STATE
#include <folly/dynamic.h>
#endif

namespace facebook::react {

// The list's scroll in dp from an origin native picks. Only its difference from the slot origins matters.
class FlumeViewState final {
 public:
  FlumeViewState() = default;

  Point contentOffset{};

#ifdef RN_SERIALIZABLE_STATE
  FlumeViewState(const FlumeViewState & /*previous*/, folly::dynamic data)
      : contentOffset{static_cast<Float>(data["x"].getDouble()), static_cast<Float>(data["y"].getDouble())} {}

  folly::dynamic getDynamic() const {
    return folly::dynamic::object("x", contentOffset.x)("y", contentOffset.y);
  }
#endif
};

// Where native shows the slot, in the list's scrolled coordinates. A slot never placed measures at its Yoga origin.
class FlumeSlotViewState final {
 public:
  FlumeSlotViewState() = default;

  Point origin{};
  bool placed{false};

#ifdef RN_SERIALIZABLE_STATE
  FlumeSlotViewState(const FlumeSlotViewState & /*previous*/, folly::dynamic data)
      : origin{static_cast<Float>(data["x"].getDouble()), static_cast<Float>(data["y"].getDouble())}, placed{true} {}

  folly::dynamic getDynamic() const {
    return folly::dynamic::object("x", origin.x)("y", origin.y);
  }
#endif
};

JSI_EXPORT extern const char FlumeViewComponentName[];
JSI_EXPORT extern const char FlumeSlotViewComponentName[];

// Offsets its slots by the scroll, as ScrollView offsets its content, so measure() matches the screen.
class JSI_EXPORT FlumeViewShadowNode final
    : public ConcreteViewShadowNode<FlumeViewComponentName, FlumeViewProps, FlumeViewEventEmitter, FlumeViewState> {
 public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

  Point getContentOriginOffset(bool includeTransform) const override;
};

// Moves a slot's content from its Yoga origin to the cell native shows it in.
class JSI_EXPORT FlumeSlotViewShadowNode final
    : public ConcreteViewShadowNode<
          FlumeSlotViewComponentName,
          FlumeSlotViewProps,
          FlumeSlotViewEventEmitter,
          FlumeSlotViewState> {
 public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

  Point getContentOriginOffset(bool includeTransform) const override;
};

} // namespace facebook::react
