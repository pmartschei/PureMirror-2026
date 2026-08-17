#pragma once

#include "PluginManifest.h"
#include "PluginManifestError.h"

#include <vector>

namespace PureMirror::Overlay
{
    struct PluginManifestParseResult
    {
        PluginManifest Manifest;
        std::vector<PluginManifestError> Errors;

        [[nodiscard]] bool IsSuccessful() const noexcept
        {
            return Errors.empty();
        }
    };
}  // namespace PureMirror::Overlay
