#pragma once

#include "PluginInstallation.h"
#include "PluginReloadPlan.h"

#include <string_view>
#include <vector>

namespace PureMirror::Overlay
{
    class PluginReloadPlanner
    {
      public:
        [[nodiscard]] PluginReloadPlan Plan(const std::vector<PluginInstallation>& installations,
                                            std::string_view pluginId) const;
    };
}  // namespace PureMirror::Overlay
