#pragma once

#include "PluginDependencyIssueType.h"

#include <string>

namespace PureMirror::Overlay
{
    struct PluginDependencyIssue
    {
        PluginDependencyIssueType Type{};
        std::string PluginId;
        std::string DependencyId;
        std::string RequiredVersion;
        std::string InstalledVersion;
    };
}  // namespace PureMirror::Overlay
