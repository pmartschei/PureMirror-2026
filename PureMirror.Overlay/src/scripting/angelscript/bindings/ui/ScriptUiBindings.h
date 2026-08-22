#pragma once

#include <string>

class asIScriptEngine;

namespace PureMirror::Overlay
{
    class IScriptUiRuntime;

    [[nodiscard]] bool RegisterScriptUiBindings(asIScriptEngine& engine, IScriptUiRuntime& runtime, std::string& error);
}  // namespace PureMirror::Overlay
