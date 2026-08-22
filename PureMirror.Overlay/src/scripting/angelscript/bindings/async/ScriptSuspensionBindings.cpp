#include "pch.h"

#include "scripting/angelscript/bindings/async/ScriptSuspensionBindings.h"

#include "angelscript.h"
#include "scripting/angelscript/IScriptSuspensionRuntime.h"
#include "scripting/angelscript/bindings/ScriptBindingUtils.h"

#include <format>

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr std::string_view UtilsNamespace = "Utils";

        IScriptSuspensionRuntime* SuspensionRuntime(asIScriptGeneric* generic)
        {
            return generic != nullptr ? static_cast<IScriptSuspensionRuntime*>(generic->GetAuxiliary()) : nullptr;
        }

        void YieldGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = SuspensionRuntime(generic); runtime != nullptr)
                runtime->HostYield();
        }

        void SleepGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = SuspensionRuntime(generic); runtime != nullptr)
                runtime->HostSleep(generic->GetArgQWord(0));
        }
    }  // namespace

    bool RegisterScriptSuspensionBindings(asIScriptEngine& engine,
                                          IScriptSuspensionRuntime& runtime,
                                          std::string& error)
    {
        error.clear();
        const ScriptBindingUtils require{"suspension"};
        const auto operation = [](const std::string_view name) { return std::format("{}::{}", UtilsNamespace, name); };

        const auto successful =
            require(engine.SetDefaultNamespace(UtilsNamespace.data()),
                    std::format("namespace {}", UtilsNamespace),
                    error) &&
            require(engine.RegisterGlobalFunction("void Yield()", asFUNCTION(YieldGeneric), asCALL_GENERIC, &runtime),
                    operation("Yield"),
                    error) &&
            require(engine.RegisterGlobalFunction(
                        "void Sleep(uint64 timeInMs)", asFUNCTION(SleepGeneric), asCALL_GENERIC, &runtime),
                    operation("Sleep"),
                    error);
        const auto reset = require(engine.SetDefaultNamespace(""), "default namespace for suspension functions", error);
        return successful && reset;
    }
}  // namespace PureMirror::Overlay
