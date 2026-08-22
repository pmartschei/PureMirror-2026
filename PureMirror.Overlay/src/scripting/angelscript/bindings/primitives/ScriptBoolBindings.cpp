#include "pch.h"

#include "ScriptBoolBindings.h"

#include "ScriptBool.h"
#include "angelscript.h"
#include "scripting/angelscript/bindings/ScriptBindingUtils.h"

#include <format>

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr std::string_view BoolName = "Bool";

        ScriptBool* CreateBool()
        {
            return new ScriptBool{};
        }

        ScriptBool* CreateBool(const bool value)
        {
            return new ScriptBool{value};
        }
    }  // namespace

    bool RegisterScriptBoolBindings(asIScriptEngine& engine, std::string& error)
    {
        error.clear();
        const ScriptBindingUtils require{"Bool"};
        const auto operation = [](const std::string_view name) { return std::format("{} {}", BoolName, name); };
        const std::string previousNamespace = engine.GetDefaultNamespace();

        auto successful =
            require(engine.SetDefaultNamespace(""), "global namespace", error) &&
            require(engine.RegisterObjectType(BoolName.data(), 0, asOBJ_REF), operation("type"), error) &&
            require(engine.RegisterObjectBehaviour(
                        BoolName.data(), asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptBool, AddRef), asCALL_THISCALL),
                    operation("addref"),
                    error) &&
            require(engine.RegisterObjectBehaviour(
                        BoolName.data(), asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptBool, Release), asCALL_THISCALL),
                    operation("release"),
                    error) &&
            require(engine.RegisterObjectBehaviour(BoolName.data(),
                                                   asBEHAVE_FACTORY,
                                                   "Bool@ f()",
                                                   asFUNCTIONPR(CreateBool, (), ScriptBool*),
                                                   asCALL_CDECL),
                    operation("default factory"),
                    error) &&
            require(engine.RegisterObjectBehaviour(BoolName.data(),
                                                   asBEHAVE_FACTORY,
                                                   "Bool@ f(bool value)",
                                                   asFUNCTIONPR(CreateBool, (bool), ScriptBool*),
                                                   asCALL_CDECL),
                    operation("value factory"),
                    error) &&
            require(engine.RegisterObjectMethod(BoolName.data(),
                                                "Bool& opAssign(bool value)",
                                                asMETHODPR(ScriptBool, Assign, (bool), ScriptBool&),
                                                asCALL_THISCALL),
                    operation("bool assignment"),
                    error) &&
            require(engine.RegisterObjectMethod(BoolName.data(),
                                                "Bool& opAssign(const Bool&in value)",
                                                asMETHODPR(ScriptBool, Assign, (const ScriptBool&), ScriptBool&),
                                                asCALL_THISCALL),
                    operation("Bool assignment"),
                    error) &&
            require(engine.RegisterObjectMethod(
                        BoolName.data(), "bool opImplConv() const", asMETHOD(ScriptBool, ToBool), asCALL_THISCALL),
                    operation("implicit bool conversion"),
                    error) &&
            require(engine.RegisterObjectMethod(BoolName.data(),
                                                "bool get_Value() const property",
                                                asMETHOD(ScriptBool, GetValue),
                                                asCALL_THISCALL),
                    operation("Value getter"),
                    error) &&
            require(engine.RegisterObjectMethod(BoolName.data(),
                                                "void set_Value(bool value) property",
                                                asMETHOD(ScriptBool, SetValue),
                                                asCALL_THISCALL),
                    operation("Value setter"),
                    error);

        const auto reset =
            require(engine.SetDefaultNamespace(previousNamespace.c_str()), "restore default namespace", error);
        successful = successful && reset;
        return successful;
    }
}  // namespace PureMirror::Overlay
