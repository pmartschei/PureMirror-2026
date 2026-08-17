#pragma once

#include "PluginChangePlan.h"
#include "PluginInstallation.h"
#include "PluginPackage.h"

#include <string>
#include <string_view>
#include <vector>

namespace PureMirror::Overlay
{
    class PluginPackagePlanner
    {
      public:
        [[nodiscard]] PluginChangePlan Plan(const std::vector<PluginInstallation>& currentInstallations,
                                            const std::vector<PluginPackage>& availablePackages,
                                            const std::vector<std::string>& desiredExplicitPluginIds) const;
        [[nodiscard]] PluginChangePlan PlanInstall(const std::vector<PluginInstallation>& currentInstallations,
                                                   const std::vector<PluginPackage>& availablePackages,
                                                   std::string_view pluginId) const;
        [[nodiscard]] PluginChangePlan PlanRemove(const std::vector<PluginInstallation>& currentInstallations,
                                                  const std::vector<PluginPackage>& availablePackages,
                                                  std::string_view pluginId) const;
    };
}  // namespace PureMirror::Overlay
