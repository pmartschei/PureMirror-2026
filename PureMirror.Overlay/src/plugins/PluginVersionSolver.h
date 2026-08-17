#pragma once

#include "PluginPackage.h"
#include "PluginVersionSelectionResult.h"

#include <string>
#include <vector>

namespace PureMirror::Overlay
{
    class PluginVersionSolver
    {
      public:
        [[nodiscard]] PluginVersionSelectionResult Resolve(
            const std::vector<PluginPackage>& availablePackages,
            const std::vector<std::string>& rootPluginIds,
            const std::vector<PluginPackage>& preferredPackages = {}) const;
    };
}  // namespace PureMirror::Overlay
