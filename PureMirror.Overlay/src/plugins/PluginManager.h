#pragma once

#include "PluginInfo.h"
#include "PluginInstallation.h"
#include "PluginPackage.h"
#include "src/core/logger/Logger.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace PureMirror::Overlay
{
    class IScriptEngine;
    class PluginScriptInstance;

    class PluginManager
    {
      public:
        PluginManager(IScriptEngine& scriptEngine, Logger& logger);
        ~PluginManager();

        PluginManager(const PluginManager&) = delete;
        PluginManager& operator=(const PluginManager&) = delete;

        [[nodiscard]] std::size_t ScanPlugins(const std::filesystem::path& pluginsRoot);
        [[nodiscard]] std::vector<PluginInfo> AvailablePlugins() const;
        [[nodiscard]] std::vector<PluginInfo> LoadedPlugins() const;
        [[nodiscard]] bool LoadPlugin(std::string_view pluginId);
        [[nodiscard]] bool UnloadPlugin(std::string_view pluginId);
        [[nodiscard]] bool ReloadPlugin(std::string_view pluginId);
        void Render();
        void UnloadAll();
        [[nodiscard]] std::size_t LoadedPluginCount() const noexcept;

      private:
        [[nodiscard]] bool ApplyInstallations(std::vector<PluginInstallation> installations,
                                              const std::vector<std::vector<std::string>>& loadGroups);
        [[nodiscard]] bool LoadGroups(const std::vector<PluginInstallation>& installations,
                                      const std::vector<std::vector<std::string>>& loadGroups);
        [[nodiscard]] bool LoadPackage(const PluginPackage& package);
        void UnloadGroups(const std::vector<std::vector<std::string>>& unloadGroups);
        [[nodiscard]] bool IsPluginLoaded(std::string_view pluginId) const;

        IScriptEngine& m_ScriptEngine;
        Logger& m_Logger;
        std::vector<PluginPackage> m_AvailablePackages;
        std::vector<PluginInstallation> m_Installations;
        std::vector<std::unique_ptr<PluginScriptInstance>> m_LoadedPlugins;
    };
}  // namespace PureMirror::Overlay
