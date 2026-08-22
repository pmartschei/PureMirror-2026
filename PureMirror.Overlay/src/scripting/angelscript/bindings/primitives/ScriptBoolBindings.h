#pragma once

#include <string>

class asIScriptEngine;

namespace PureMirror::Overlay
{
    [[nodiscard]] bool RegisterScriptBoolBindings(asIScriptEngine& engine, std::string& error);
}  // namespace PureMirror::Overlay
