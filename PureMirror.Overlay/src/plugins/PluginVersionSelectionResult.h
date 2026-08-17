#pragma once

#include "PluginPackage.h"
#include "PluginVersionSelectionError.h"

#include <vector>

namespace PureMirror::Overlay
{
    struct PluginVersionSelectionResult
    {
        std::vector<PluginPackage> Packages;
        std::vector<PluginVersionSelectionError> Errors;

        [[nodiscard]] bool IsSuccessful() const noexcept
        {
            return Errors.empty();
        }
    };
}  // namespace PureMirror::Overlay
