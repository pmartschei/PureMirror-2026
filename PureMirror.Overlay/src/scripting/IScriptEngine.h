#pragma once

#include "ScriptCallResult.h"
#include "ScriptModuleLoadResult.h"
#include "ScriptSource.h"

#include <string_view>
#include <vector>

namespace PureMirror::Overlay
{
    class IScriptEngine
    {
      public:
        virtual ~IScriptEngine() = default;

        [[nodiscard]] virtual ScriptModuleLoadResult LoadModule(std::string_view moduleId,
                                                                const std::vector<ScriptSource>& sources) = 0;
        [[nodiscard]] virtual ScriptCallResult CallFunction(std::string_view moduleId,
                                                            std::string_view functionDeclaration) = 0;
        virtual void UnloadModule(std::string_view moduleId) = 0;
    };
}  // namespace PureMirror::Overlay
