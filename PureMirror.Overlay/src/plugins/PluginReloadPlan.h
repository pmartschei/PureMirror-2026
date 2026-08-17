#pragma once

#include <string>
#include <vector>

namespace PureMirror::Overlay
{
    struct PluginReloadPlan
    {
        std::vector<std::vector<std::string>> UnloadGroups;
        std::vector<std::vector<std::string>> LoadGroups;
        std::string Error;

        [[nodiscard]] bool IsSuccessful() const noexcept
        {
            return Error.empty();
        }
    };
}  // namespace PureMirror::Overlay
