#include "pch.h"

#include "PluginScriptInstance.h"

namespace PureMirror::Overlay
{
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

        auto result = m_ScriptEngine.CallFunction(m_Manifest.Id, "void on_load()");
        if (!result.IsSuccessful())
        {
            m_ScriptEngine.UnloadModule(m_Manifest.Id);
            m_IsCompiled = false;
            return result;
        }
        m_IsLoaded = true;
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
        return m_ScriptEngine.CallFunction(m_Manifest.Id, "void on_render()");
    }

    ScriptCallResult PluginScriptInstance::Unload()
    {
        if (!m_IsCompiled)
            return {.ModuleId = m_Manifest.Id, .FunctionDeclaration = "void on_unload()"};
        auto result = m_IsLoaded
                          ? m_ScriptEngine.CallFunction(m_Manifest.Id, "void on_unload()")
                          : ScriptCallResult{.ModuleId = m_Manifest.Id, .FunctionDeclaration = "void on_unload()"};
        m_ScriptEngine.UnloadModule(m_Manifest.Id);
        m_IsCompiled = false;
        m_IsLoaded = false;
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
