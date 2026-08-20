#include "pch.h"

#include "PluginScriptInstance.h"

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr ScriptCallback LoadCallback{"void on_load()",
                                              ScriptCallbackTag::Suspendable | ScriptCallbackTag::Coroutine};
        constexpr ScriptCallback RenderCallback{"void on_render()", ScriptCallbackTag::Ui};
        constexpr ScriptCallback UnloadCallback{"void on_unload()", ScriptCallbackTag::None};
    }  // namespace

    PluginScriptInstance::PluginScriptInstance(IScriptEngine& scriptEngine,
                                               PluginManifest manifest,
                                               std::filesystem::path packageRoot)
        : m_ScriptEngine(scriptEngine), m_Manifest(std::move(manifest)), m_PackageRoot(std::move(packageRoot))
    {
    }

    PluginScriptInstance::~PluginScriptInstance()
    {
        if (m_IsCompiled)
            static_cast<void>(Unload());
    }

    ScriptModuleLoadResult PluginScriptInstance::Load(const std::vector<PluginPackage>& dependencies)
    {
        auto result = Compile(dependencies);
        if (!result.IsSuccessful())
            return result;

        auto bindResult = BindImports();
        result.Diagnostics.insert(
            result.Diagnostics.end(), bindResult.Diagnostics.begin(), bindResult.Diagnostics.end());
        if (!bindResult.IsSuccessful())
        {
            result.IsLoaded = false;
            static_cast<void>(Unload());
            return result;
        }

        const auto activation = Activate();
        result.Diagnostics.insert(
            result.Diagnostics.end(), activation.Diagnostics.begin(), activation.Diagnostics.end());
        if (!activation.IsSuccessful())
            result.IsLoaded = false;
        return result;
    }

    ScriptModuleLoadResult PluginScriptInstance::Compile(const std::vector<PluginPackage>& dependencies)
    {
        if (m_IsCompiled)
            static_cast<void>(Unload());

        auto result = PluginScriptCompiler(m_ScriptEngine).Compile(m_Manifest, m_PackageRoot, dependencies);
        m_IsCompiled = result.IsSuccessful();
        return result;
    }

    ScriptModuleLoadResult PluginScriptInstance::BindImports()
    {
        if (!m_IsCompiled)
            return {.ModuleId = m_Manifest.Id,
                    .Diagnostics = {
                        {.Severity = ScriptDiagnosticSeverity::Error, .Message = "Plugin script is not compiled."}}};
        return m_ScriptEngine.BindModuleImports(m_Manifest.Id);
    }

    ScriptCallResult PluginScriptInstance::Activate()
    {
        if (!m_IsCompiled)
            return {.Status = ScriptCallStatus::Failed,
                    .ModuleId = m_Manifest.Id,
                    .FunctionDeclaration = "void on_load()",
                    .Diagnostics = {
                        {.Severity = ScriptDiagnosticSeverity::Error, .Message = "Plugin script is not compiled."}}};

        auto result = m_ScriptEngine.CallFunction(m_Manifest.Id, LoadCallback);
        if (!result.IsSuccessful())
        {
            m_ScriptEngine.UnloadModule(m_Manifest.Id);
            m_IsCompiled = false;
            return result;
        }
        m_IsLoaded = true;
        m_IsActivating = result.Status == ScriptCallStatus::Suspended;
        return result;
    }

    ScriptCallResult PluginScriptInstance::Render()
    {
        if (!m_IsLoaded)
            return {.Status = ScriptCallStatus::Failed,
                    .ModuleId = m_Manifest.Id,
                    .FunctionDeclaration = "void on_render()",
                    .Diagnostics = {
                        {.Severity = ScriptDiagnosticSeverity::Error, .Message = "Plugin script is not loaded."}}};
        if (m_IsActivating)
        {
            auto result = m_ScriptEngine.CallFunction(m_Manifest.Id, LoadCallback);
            if (result.Status == ScriptCallStatus::Suspended || !result.IsSuccessful())
                return result;
            m_IsActivating = false;
            return result;
        }
        return m_ScriptEngine.CallFunction(m_Manifest.Id, RenderCallback);
    }

    ScriptCallResult PluginScriptInstance::Unload()
    {
        if (!m_IsCompiled)
            return {.ModuleId = m_Manifest.Id, .FunctionDeclaration = "void on_unload()"};
        auto result = m_IsLoaded
                          ? m_ScriptEngine.CallFunction(m_Manifest.Id, UnloadCallback)
                          : ScriptCallResult{.ModuleId = m_Manifest.Id, .FunctionDeclaration = "void on_unload()"};
        m_ScriptEngine.UnloadModule(m_Manifest.Id);
        m_IsCompiled = false;
        m_IsLoaded = false;
        m_IsActivating = false;
        return result;
    }

    bool PluginScriptInstance::IsLoaded() const noexcept
    {
        return m_IsLoaded;
    }

    const PluginManifest& PluginScriptInstance::Manifest() const noexcept
    {
        return m_Manifest;
    }
}  // namespace PureMirror::Overlay
