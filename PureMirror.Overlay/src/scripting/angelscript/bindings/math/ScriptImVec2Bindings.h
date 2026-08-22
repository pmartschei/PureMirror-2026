#pragma once

#include <string>

class asIScriptEngine;

namespace PureMirror::Overlay
{
    [[nodiscard]] bool RegisterScriptImVec2Bindings(asIScriptEngine& engine, std::string& error);
}  // namespace PureMirror::Overlay
