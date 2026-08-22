#pragma once

#include <string>

class asIScriptEngine;

namespace PureMirror::Overlay
{
    class IScriptSuspensionRuntime;

    [[nodiscard]] bool RegisterScriptSuspensionBindings(asIScriptEngine& engine,
                                                        IScriptSuspensionRuntime& runtime,
                                                        std::string& error);
}  // namespace PureMirror::Overlay
