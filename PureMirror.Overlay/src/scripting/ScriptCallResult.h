#pragma once

#include "ScriptCallStatus.h"
#include "ScriptDiagnostic.h"

#include <string>
#include <vector>

namespace PureMirror::Overlay
{
    struct ScriptCallResult
    {
        ScriptCallStatus Status{ScriptCallStatus::NotFound};
        std::string ModuleId;
        std::string FunctionDeclaration;
        std::vector<ScriptDiagnostic> Diagnostics;

        [[nodiscard]] bool IsSuccessful() const noexcept
        {
            return Status != ScriptCallStatus::Failed;
        }
    };
}  // namespace PureMirror::Overlay
