#include "pch.h"

#include "scripting/angelscript/bindings/math/ScriptImVec2Bindings.h"

#include "angelscript.h"
#include "scripting/angelscript/bindings/ScriptBindingUtils.h"
#include "scripting/angelscript/bindings/math/ScriptImVec2.h"

#include <cstddef>
#include <new>
#include <string_view>

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr std::string_view ImVec2Type = "ImVec2";

        void ConstructImVec2(asIScriptGeneric* generic)
        {
            if (generic != nullptr)
                new (generic->GetObject()) ScriptImVec2{};
        }

        void ConstructImVec2Values(asIScriptGeneric* generic)
        {
            if (generic != nullptr)
                new (generic->GetObject()) ScriptImVec2{generic->GetArgFloat(0), generic->GetArgFloat(1)};
        }
    }  // namespace

    bool RegisterScriptImVec2Bindings(asIScriptEngine& engine, std::string& error)
    {
        error.clear();
        const ScriptBindingUtils require{ImVec2Type.data()};

        return require(engine.RegisterObjectType(ImVec2Type.data(),
                                                 sizeof(ScriptImVec2),
                                                 asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ScriptImVec2>()),
                       "type",
                       error) &&
               require(
                   engine.RegisterObjectBehaviour(
                       ImVec2Type.data(), asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructImVec2), asCALL_GENERIC),
                   "default constructor",
                   error) &&
               require(engine.RegisterObjectBehaviour(ImVec2Type.data(),
                                                      asBEHAVE_CONSTRUCT,
                                                      "void f(float x, float y)",
                                                      asFUNCTION(ConstructImVec2Values),
                                                      asCALL_GENERIC),
                       "value constructor",
                       error) &&
               require(engine.RegisterObjectProperty(ImVec2Type.data(), "float x", offsetof(ScriptImVec2, X)),
                       "x property",
                       error) &&
               require(engine.RegisterObjectProperty(ImVec2Type.data(), "float y", offsetof(ScriptImVec2, Y)),
                       "y property",
                       error);
    }
}  // namespace PureMirror::Overlay
