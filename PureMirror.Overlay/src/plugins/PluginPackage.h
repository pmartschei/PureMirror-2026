#pragma once

#include "PluginManifest.h"
#include "PluginPackageOrigin.h"

#include <string>

namespace PureMirror::Overlay
{
    struct PluginPackage
    {
        PluginManifest Manifest;
        PluginPackageOrigin Origin{PluginPackageOrigin::Local};
        std::string Location;
    };
}  // namespace PureMirror::Overlay
