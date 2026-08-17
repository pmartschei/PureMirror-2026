#pragma once

#include <string>
#include <vector>

namespace PureMirror::Overlay
{
    struct PluginVersionSelectionError
    {
        std::string PluginId;
        std::vector<std::string> RequiredRanges;
        std::string Message;
    };
}  // namespace PureMirror::Overlay
