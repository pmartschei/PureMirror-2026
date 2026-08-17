#pragma once

#include "PluginChange.h"
#include "PluginInstallation.h"
#include "PluginVersionSelectionError.h"

#include <string>
#include <vector>

namespace PureMirror::Overlay
{
    struct PluginChangePlan
    {
        std::vector<PluginInstallation> Installations;
        std::vector<PluginChange> Changes;
        std::vector<std::vector<std::string>> LoadGroups;
        std::vector<PluginVersionSelectionError> Errors;

        [[nodiscard]] bool IsSuccessful() const noexcept
        {
            return Errors.empty();
        }
    };
}  // namespace PureMirror::Overlay
