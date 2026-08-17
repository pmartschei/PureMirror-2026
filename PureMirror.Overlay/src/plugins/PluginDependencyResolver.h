#pragma once

#include "PluginDependencyResolution.h"
#include "PluginManifest.h"

#include <vector>

namespace PureMirror::Overlay
{
    class PluginDependencyResolver
    {
      public:
        [[nodiscard]] PluginDependencyResolution Resolve(const std::vector<PluginManifest>& manifests) const;
    };
}  // namespace PureMirror::Overlay
