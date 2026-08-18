#pragma once

#include "IScriptEngine.h"
#include "PluginScriptCompiler.h"
#include "src/plugins/PluginManifest.h"
#include "src/plugins/PluginPackage.h"

#include <filesystem>
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
        [[nodiscard]] ScriptCallResult Render();
        [[nodiscard]] ScriptCallResult Unload();
        [[nodiscard]] bool IsLoaded() const noexcept;
        [[nodiscard]] const PluginManifest& Manifest() const noexcept;

      private:
        IScriptEngine& m_ScriptEngine;
        PluginManifest m_Manifest;
        std::filesystem::path m_PackageRoot;
        bool m_IsCompiled{};
        bool m_IsLoaded{};
    };
}  // namespace PureMirror::Overlay
