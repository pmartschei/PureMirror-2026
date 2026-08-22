#pragma once

#include "IScriptEngine.h"
#include "PluginScriptCompiler.h"
#include "src/plugins/PluginManifest.h"
#include "src/plugins/PluginPackage.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace PureMirror::Overlay
{
    class PluginScriptInstance
    {
      public:
        PluginScriptInstance(IScriptEngine& scriptEngine, PluginManifest manifest, std::filesystem::path packageRoot);
        ~PluginScriptInstance();

        PluginScriptInstance(const PluginScriptInstance&) = delete;
        PluginScriptInstance& operator=(const PluginScriptInstance&) = delete;

        [[nodiscard]] ScriptModuleLoadResult Load(const std::vector<PluginPackage>& dependencies = {});
        [[nodiscard]] ScriptModuleLoadResult Compile(const std::vector<PluginPackage>& dependencies = {});
        [[nodiscard]] ScriptModuleLoadResult BindImports();
        [[nodiscard]] ScriptCallResult Activate();
        [[nodiscard]] ScriptCallResult BeginFrame();
        [[nodiscard]] ScriptCallResult EndFrame();
        [[nodiscard]] ScriptCallResult Update(float deltaTime);
        [[nodiscard]] ScriptCallResult Disable();
        [[nodiscard]] ScriptCallResult Enable();
        [[nodiscard]] ScriptCallResult RenderMenu();
        [[nodiscard]] ScriptCallResult RenderSettings();
        [[nodiscard]] ScriptCallResult RenderInterface();
        [[nodiscard]] ScriptCallResult Render();
        [[nodiscard]] ScriptCallResult Unload();
        [[nodiscard]] bool IsLoaded() const noexcept;
        [[nodiscard]] const PluginManifest& Manifest() const noexcept;

      private:
        [[nodiscard]] ScriptCallResult Invoke(const ScriptCallback& callback);
        [[nodiscard]] ScriptCallResult NotLoadedResult(std::string_view functionDeclaration) const;
        IScriptEngine& m_ScriptEngine;
        PluginManifest m_Manifest;
        std::filesystem::path m_PackageRoot;
        bool m_IsCompiled{};
        bool m_IsLoaded{};
        bool m_IsActivating{};
    };
}  // namespace PureMirror::Overlay
