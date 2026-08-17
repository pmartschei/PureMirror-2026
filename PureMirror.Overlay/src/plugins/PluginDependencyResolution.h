#pragma once

#include "PluginDependencyIssue.h"

#include <string>
#include <vector>

namespace PureMirror::Overlay
{
    struct PluginDependencyResolution
    {
        std::vector<std::vector<std::string>> LoadGroups;
        std::vector<PluginDependencyIssue> Issues;

        [[nodiscard]] bool IsSuccessful() const noexcept
        {
            return Issues.empty();
        }
    };
}  // namespace PureMirror::Overlay
