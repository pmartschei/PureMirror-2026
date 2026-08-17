#pragma once

#include <string>

namespace PureMirror::Overlay
{
    struct PluginManifestError
    {
        std::string Field;
        std::string Message;
    };
}  // namespace PureMirror::Overlay
