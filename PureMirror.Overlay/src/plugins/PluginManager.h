#pragma once

#include "src/core/logger/Logger.h"

#include <cstddef>
#include <filesystem>
#include <memory>
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

        [[nodiscard]] std::size_t LoadStartupPlugins(const std::filesystem::path& pluginsRoot);
        void Render();
        void UnloadAll();
        [[nodiscard]] std::size_t LoadedPluginCount() const noexcept;

      private:
        IScriptEngine& m_ScriptEngine;
        Logger& m_Logger;
        std::vector<std::unique_ptr<PluginScriptInstance>> m_LoadedPlugins;
    };
}  // namespace PureMirror::Overlay
