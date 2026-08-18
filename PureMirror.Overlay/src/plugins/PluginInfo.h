#pragma once

#include <string>

namespace PureMirror::Overlay
{
    struct PluginInfo
    {
        std::string Id;
        std::string Name;
        std::string Version;
        bool IsExplicit{};
    };
}  // namespace PureMirror::Overlay
