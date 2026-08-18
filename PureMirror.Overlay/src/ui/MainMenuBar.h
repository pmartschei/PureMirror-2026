#pragma once

#include <filesystem>
#include <functional>

namespace PureMirror::Overlay
{
    class Logger;
    class PluginManager;

    class MainMenuBar
    {
      public:
        MainMenuBar(PluginManager& pluginManager,
                    Logger& logger,
                    std::filesystem::path pureMirrorRoot,
                    std::function<void()> exitHandler);

        void Render();
        [[nodiscard]] const std::filesystem::path& PluginsRoot() const noexcept;

      private:
        void RenderPureMirrorMenu();
        void RenderPluginsMenu();
        void OpenFolder(const std::filesystem::path& folder);

        PluginManager& m_PluginManager;
        Logger& m_Logger;
        std::filesystem::path m_PureMirrorRoot;
        std::filesystem::path m_PluginsRoot;
        std::function<void()> m_ExitHandler;
    };
}  // namespace PureMirror::Overlay
