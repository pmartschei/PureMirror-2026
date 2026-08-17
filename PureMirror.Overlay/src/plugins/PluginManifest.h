#pragma once

#include "PluginDependency.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PureMirror::Overlay
{
    struct PluginManifest
    {
        std::uint32_t SchemaVersion{};
        std::string Id;
        std::string Name;
        std::string Version;
        std::string ApiVersion;
        std::string Entry;
        std::vector<std::string> Exports;
        std::vector<PluginDependency> Dependencies;
        std::vector<PluginDependency> OptionalDependencies;
        std::vector<std::string> Capabilities;
    };
}  // namespace PureMirror::Overlay
