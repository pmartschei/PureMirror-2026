#include "pch.h"

#include "PluginScriptInstance.h"

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr auto CoroutineCallbackTags = ScriptCallbackTag::Coroutine;
        constexpr auto UiCallbackTags = ScriptCallbackTag::Coroutine | ScriptCallbackTag::Ui;
        constexpr auto MenuCallbackTags = ScriptCallbackTag::Coroutine | ScriptCallbackTag::MenuUi;

        const ScriptCallback LoadCallback{"void OnLoad()",
                                          ScriptCallbackTag::Suspendable | ScriptCallbackTag::Coroutine};
        const ScriptCallback UnloadCallback{"void OnUnload()", ScriptCallbackTag::None};
        const ScriptCallback BeginFrameCallback{"void OnBeginFrame()", CoroutineCallbackTags};
        const ScriptCallback EndFrameCallback{"void OnEndFrame()", CoroutineCallbackTags};
        const ScriptCallback DisableCallback{"void OnDisable()", CoroutineCallbackTags};
        const ScriptCallback EnableCallback{"void OnEnable()", CoroutineCallbackTags};
        const ScriptCallback RenderMenuCallback{"void OnRenderMenu()", MenuCallbackTags};
        const ScriptCallback RenderSettingsCallback{"void OnRenderSettings()", UiCallbackTags};
        const ScriptCallback RenderInterfaceCallback{"void OnRenderInterface()", UiCallbackTags};
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
                    .FunctionDeclaration = std::string(LoadCallback.FunctionDeclaration),
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

    ScriptCallResult PluginScriptInstance::BeginFrame()
    {
        return Invoke(BeginFrameCallback);
    }

    ScriptCallResult PluginScriptInstance::EndFrame()
    {
        return Invoke(EndFrameCallback);
    }

    ScriptCallResult PluginScriptInstance::Update(const float deltaTime)
    {
        return Invoke({"void OnUpdate(float)", CoroutineCallbackTags, {{.Index = 0, .Value = deltaTime}}});
    }

    ScriptCallResult PluginScriptInstance::Disable()
    {
        return Invoke(DisableCallback);
    }

    ScriptCallResult PluginScriptInstance::Enable()
    {
        return Invoke(EnableCallback);
    }

    ScriptCallResult PluginScriptInstance::RenderMenu()
    {
        return Invoke(RenderMenuCallback);
    }

    ScriptCallResult PluginScriptInstance::RenderSettings()
    {
        return Invoke(RenderSettingsCallback);
    }

    ScriptCallResult PluginScriptInstance::RenderInterface()
    {
        return Invoke(RenderInterfaceCallback);
    }

    ScriptCallResult PluginScriptInstance::Render()
    {
        return RenderInterface();
    }

    ScriptCallResult PluginScriptInstance::Unload()
    {
        if (!m_IsCompiled)
            return {.ModuleId = m_Manifest.Id, .FunctionDeclaration = std::string(UnloadCallback.FunctionDeclaration)};
        auto result = m_IsLoaded
                          ? m_ScriptEngine.CallFunction(m_Manifest.Id, UnloadCallback)
                          : ScriptCallResult{.ModuleId = m_Manifest.Id,
                                             .FunctionDeclaration = std::string(UnloadCallback.FunctionDeclaration)};
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

    ScriptCallResult PluginScriptInstance::Invoke(const ScriptCallback& callback)
    {
        if (!m_IsLoaded)
            return NotLoadedResult(callback.FunctionDeclaration);
        if (m_IsActivating)
        {
            auto result = m_ScriptEngine.CallFunction(m_Manifest.Id, LoadCallback);
            if (result.Status == ScriptCallStatus::Suspended || !result.IsSuccessful())
                return result;
            m_IsActivating = false;
            return result;
        }
        return m_ScriptEngine.CallFunction(m_Manifest.Id, callback);
    }

    ScriptCallResult PluginScriptInstance::NotLoadedResult(const std::string_view functionDeclaration) const
    {
        return {
            .Status = ScriptCallStatus::Failed,
            .ModuleId = m_Manifest.Id,
            .FunctionDeclaration = std::string(functionDeclaration),
            .Diagnostics = {{.Severity = ScriptDiagnosticSeverity::Error, .Message = "Plugin script is not loaded."}}};
    }
}  // namespace PureMirror::Overlay
