#include "pch.h"

#include "ScriptUiBindings.h"

#include "IScriptUiRuntime.h"
#include "ScriptBindingUtils.h"
#include "ScriptImVec2.h"
#include "angelscript.h"

#include <cstddef>
#include <format>
#include <new>

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr std::string_view UiNamespace = "UI";
        constexpr std::string_view ImVec2Type = "ImVec2";

        IScriptUiRuntime* UiRuntime(asIScriptGeneric* generic)
        {
            return generic != nullptr ? static_cast<IScriptUiRuntime*>(generic->GetAuxiliary()) : nullptr;
        }

        const std::string& StringArgument(asIScriptGeneric& generic, const asUINT index)
        {
            return *static_cast<const std::string*>(generic.GetArgObject(index));
        }

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

        void BeginGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = UiRuntime(generic); runtime != nullptr)
                generic->SetReturnByte(
                    runtime->HostBegin(StringArgument(*generic, 0), nullptr, generic->GetArgDWord(1)));
        }

        void BeginOpenGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = UiRuntime(generic); runtime != nullptr)
                generic->SetReturnByte(runtime->HostBegin(StringArgument(*generic, 0),
                                                          static_cast<bool*>(generic->GetArgAddress(1)),
                                                          generic->GetArgDWord(2)));
        }

        void EndGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = UiRuntime(generic); runtime != nullptr)
                runtime->HostEnd();
        }

        void TextGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = UiRuntime(generic); runtime != nullptr)
                runtime->HostText(StringArgument(*generic, 0));
        }

        void ButtonGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = UiRuntime(generic); runtime != nullptr)
                generic->SetReturnByte(runtime->HostButton(
                    StringArgument(*generic, 0), *static_cast<const ScriptImVec2*>(generic->GetArgObject(1))));
        }

        void BeginMenuGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = UiRuntime(generic); runtime != nullptr)
                generic->SetReturnByte(
                    runtime->HostBeginMenu(StringArgument(*generic, 0), generic->GetArgByte(1) != 0));
        }

        void EndMenuGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = UiRuntime(generic); runtime != nullptr)
                runtime->HostEndMenu();
        }

        void MenuItemGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = UiRuntime(generic); runtime != nullptr)
                generic->SetReturnByte(runtime->HostMenuItem(StringArgument(*generic, 0),
                                                             StringArgument(*generic, 1),
                                                             generic->GetArgByte(2) != 0,
                                                             generic->GetArgByte(3) != 0));
        }

        void SeparatorGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = UiRuntime(generic); runtime != nullptr)
                runtime->HostSeparator();
        }
    }  // namespace

    bool RegisterScriptUiBindings(asIScriptEngine& engine, IScriptUiRuntime& runtime, std::string& error)
    {
        error.clear();
        const ScriptBindingUtils require{"UI"};
        const auto operation = [](const std::string_view name) { return std::format("{}::{}", UiNamespace, name); };

        auto successful =
            require(engine.SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, true), "allow inout references", error) &&
            require(engine.RegisterObjectType(ImVec2Type.data(),
                                              sizeof(ScriptImVec2),
                                              asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ScriptImVec2>()),
                    "ImVec2 type",
                    error) &&
            require(engine.RegisterObjectBehaviour(
                        ImVec2Type.data(), asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructImVec2), asCALL_GENERIC),
                    "ImVec2 default constructor",
                    error) &&
            require(engine.RegisterObjectBehaviour(ImVec2Type.data(),
                                                   asBEHAVE_CONSTRUCT,
                                                   "void f(float x, float y)",
                                                   asFUNCTION(ConstructImVec2Values),
                                                   asCALL_GENERIC),
                    "ImVec2 value constructor",
                    error) &&
            require(engine.RegisterObjectProperty(ImVec2Type.data(), "float x", offsetof(ScriptImVec2, X)),
                    "ImVec2::x",
                    error) &&
            require(engine.RegisterObjectProperty(ImVec2Type.data(), "float y", offsetof(ScriptImVec2, Y)),
                    "ImVec2::y",
                    error) &&
            require(engine.SetDefaultNamespace(UiNamespace.data()), std::format("namespace {}", UiNamespace), error) &&
            require(engine.RegisterGlobalFunction("bool Begin(const string &in name, uint flags = 0)",
                                                  asFUNCTION(BeginGeneric),
                                                  asCALL_GENERIC,
                                                  &runtime),
                    operation("Begin"),
                    error) &&
            require(engine.RegisterGlobalFunction("bool Begin(const string &in name, bool &inout open, uint flags = 0)",
                                                  asFUNCTION(BeginOpenGeneric),
                                                  asCALL_GENERIC,
                                                  &runtime),
                    operation("Begin(open)"),
                    error) &&
            require(engine.RegisterGlobalFunction("void End()", asFUNCTION(EndGeneric), asCALL_GENERIC, &runtime),
                    operation("End"),
                    error) &&
            require(engine.RegisterGlobalFunction(
                        "void Text(const string &in text)", asFUNCTION(TextGeneric), asCALL_GENERIC, &runtime),
                    operation("Text"),
                    error) &&
            require(
                engine.RegisterGlobalFunction("bool Button(const string &in label, const ImVec2 &in size = ImVec2())",
                                              asFUNCTION(ButtonGeneric),
                                              asCALL_GENERIC,
                                              &runtime),
                operation("Button"),
                error) &&
            require(engine.RegisterGlobalFunction("bool BeginMenu(const string &in label, bool enabled = true)",
                                                  asFUNCTION(BeginMenuGeneric),
                                                  asCALL_GENERIC,
                                                  &runtime),
                    operation("BeginMenu"),
                    error) &&
            require(
                engine.RegisterGlobalFunction("void EndMenu()", asFUNCTION(EndMenuGeneric), asCALL_GENERIC, &runtime),
                operation("EndMenu"),
                error) &&
            require(engine.RegisterGlobalFunction("bool MenuItem(const string &in label, const string &in shortcut = "
                                                  "\"\", bool selected = false, bool enabled = true)",
                                                  asFUNCTION(MenuItemGeneric),
                                                  asCALL_GENERIC,
                                                  &runtime),
                    operation("MenuItem"),
                    error) &&
            require(engine.RegisterGlobalFunction(
                        "void Separator()", asFUNCTION(SeparatorGeneric), asCALL_GENERIC, &runtime),
                    operation("Separator"),
                    error);

        const auto reset = require(engine.SetDefaultNamespace(""), "default namespace for UI functions", error);
        successful = successful && reset;
        return successful;
    }
}  // namespace PureMirror::Overlay
