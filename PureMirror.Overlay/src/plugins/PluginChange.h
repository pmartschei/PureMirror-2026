#pragma once

#include "PluginChangeType.h"
#include "PluginPackage.h"

#include <string>

namespace PureMirror::Overlay
{
    struct PluginChange
    {
        PluginChangeType Type{};
        std::string PluginId;
        std::string FromVersion;
        std::string ToVersion;
        PluginPackage Package;
    };
}  // namespace PureMirror::Overlay
