#pragma once

#include <ReactCommon/JavaTurboModule.h>
#include <ReactCommon/TurboModule.h>
#include <jsi/jsi.h>

#include "FlumeComponentDescriptors.h"

// Stands in for the generated header that autolinking includes, and adds the hand-written descriptors.
namespace facebook::react {

JSI_EXPORT
std::shared_ptr<TurboModule> FlumeSpec_ModuleProvider(
    const std::string &moduleName,
    const JavaTurboModule::InitParams &params);

} // namespace facebook::react
