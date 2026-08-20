#pragma once

#include <string>

class asIScriptEngine;

namespace PureMirror::Overlay
{
    class IScriptTaskRuntime;

    [[nodiscard]] bool RegisterScriptTaskBindings(asIScriptEngine& engine,
                                                  IScriptTaskRuntime& runtime,
                                                  std::string& error);
}  // namespace PureMirror::Overlay
