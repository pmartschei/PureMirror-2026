#pragma once

#include <string>

class asIScriptEngine;

namespace PureMirror::Overlay
{
    [[nodiscard]] bool RegisterScriptNumberBindings(asIScriptEngine& engine, std::string& error);
}  // namespace PureMirror::Overlay
