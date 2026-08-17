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
        if (m_IsLoaded)
        {
            static_cast<void>(m_ScriptEngine.CallFunction(m_Manifest.Id, "void on_unload()"));
            m_ScriptEngine.UnloadModule(m_Manifest.Id);
        }
    }

    ScriptModuleLoadResult PluginScriptInstance::Load()
    {
        if (m_IsLoaded)
            static_cast<void>(Unload());

        auto result = PluginScriptCompiler(m_ScriptEngine).Compile(m_Manifest, m_PackageRoot);
        if (!result.IsSuccessful())
            return result;

        const auto callback = m_ScriptEngine.CallFunction(m_Manifest.Id, "void on_load()");
        if (!callback.IsSuccessful())
        {
            result.Diagnostics.insert(
                result.Diagnostics.end(), callback.Diagnostics.begin(), callback.Diagnostics.end());
            result.IsLoaded = false;
            m_ScriptEngine.UnloadModule(m_Manifest.Id);
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
        if (!m_IsLoaded)
            return {.ModuleId = m_Manifest.Id, .FunctionDeclaration = "void on_unload()"};
        auto result = m_ScriptEngine.CallFunction(m_Manifest.Id, "void on_unload()");
        m_ScriptEngine.UnloadModule(m_Manifest.Id);
        m_IsLoaded = false;
        return result;
    }

    bool PluginScriptInstance::IsLoaded() const noexcept
    {
        return m_IsLoaded;
    }
}  // namespace PureMirror::Overlay
