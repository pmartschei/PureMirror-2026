#pragma once

#include <string>

namespace PureMirror::Overlay
{
    struct PluginDependency
    {
        std::string Id;
        std::string VersionRange;
    };
}  // namespace PureMirror::Overlay
