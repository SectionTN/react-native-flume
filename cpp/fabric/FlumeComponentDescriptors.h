#pragma once

#include <react/renderer/core/ConcreteComponentDescriptor.h>

#include "FlumeShadowNodes.h"

namespace facebook::react {

using FlumeViewComponentDescriptor = ConcreteComponentDescriptor<FlumeViewShadowNode>;
using FlumeSlotViewComponentDescriptor = ConcreteComponentDescriptor<FlumeSlotViewShadowNode>;

} // namespace facebook::react
