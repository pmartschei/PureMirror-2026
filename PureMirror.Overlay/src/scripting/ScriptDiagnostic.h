#pragma once

#include "ScriptDiagnosticSeverity.h"

#include <cstddef>
#include <string>

namespace PureMirror::Overlay
{
    struct ScriptDiagnostic
    {
        ScriptDiagnosticSeverity Severity{};
        std::string Section;
        std::size_t Row{};
        std::size_t Column{};
        std::string Message;
    };
}  // namespace PureMirror::Overlay
