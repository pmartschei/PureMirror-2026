#pragma once

#include "PluginPackage.h"

namespace PureMirror::Overlay
{
    struct PluginInstallation
    {
        PluginPackage Package;
        bool IsExplicit{};
    };
}  // namespace PureMirror::Overlay
