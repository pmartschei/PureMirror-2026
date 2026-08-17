#pragma once

#include "ScriptDiagnostic.h"

#include <string>
#include <vector>

namespace PureMirror::Overlay
{
    struct ScriptModuleLoadResult
    {
        std::string ModuleId;
        std::vector<ScriptDiagnostic> Diagnostics;
        bool IsLoaded{};

        [[nodiscard]] bool IsSuccessful() const noexcept
        {
            return IsLoaded;
        }
    };
}  // namespace PureMirror::Overlay
