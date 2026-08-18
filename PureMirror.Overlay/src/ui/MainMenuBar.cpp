#include "pch.h"

#include "MainMenuBar.h"

#include "src/core/logger/Logger.h"
#include "src/plugins/PluginManager.h"

#include <imgui.h>
#include <shellapi.h>

namespace PureMirror::Overlay
{
    namespace
    {
        const LogOrigin MainMenuOrigin{
            .Type = LogOriginType::Host, .Identifier = "puremirror.main-menu", .DisplayName = "PureMirror"};
    }  // namespace

    MainMenuBar::MainMenuBar(PluginManager& pluginManager,
                             Logger& logger,
                             std::filesystem::path pureMirrorRoot,
                             std::function<void()> exitHandler)
        : m_PluginManager(pluginManager), m_Logger(logger), m_PureMirrorRoot(std::move(pureMirrorRoot)),
          m_PluginsRoot(m_PureMirrorRoot / "plugins"), m_ExitHandler(std::move(exitHandler))
    {
    }

    void MainMenuBar::Render()
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        RenderPureMirrorMenu();
        RenderPluginsMenu();

        ImGui::EndMainMenuBar();
    }

    const std::filesystem::path& MainMenuBar::PluginsRoot() const noexcept
    {
        return m_PluginsRoot;
    }

    void MainMenuBar::RenderPureMirrorMenu()
    {
        if (!ImGui::BeginMenu("PureMirror"))
            return;

        if (ImGui::MenuItem("Open PureMirror folder"))
            OpenFolder(m_PureMirrorRoot);
        if (ImGui::MenuItem("Open plugins folder"))
            OpenFolder(m_PluginsRoot);

        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
        {
            m_PluginManager.UnloadAll();
            m_ExitHandler();
        }
        ImGui::EndMenu();
    }

    void MainMenuBar::RenderPluginsMenu()
    {
        if (!ImGui::BeginMenu("Plugins"))
            return;

        const auto loadedPluginCount = m_PluginManager.LoadedPluginCount();

        if (loadedPluginCount != 0)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem("Load all plugins"))
            static_cast<void>(m_PluginManager.LoadStartupPlugins(m_PluginsRoot));
        if (loadedPluginCount != 0)
            ImGui::EndDisabled();

        if (loadedPluginCount == 0)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem("Reload all plugins"))
            static_cast<void>(m_PluginManager.LoadStartupPlugins(m_PluginsRoot));
        if (ImGui::MenuItem("Unload all plugins"))
            m_PluginManager.UnloadAll();
        if (loadedPluginCount == 0)
            ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::TextDisabled("%zu plugin%s loaded", loadedPluginCount, loadedPluginCount == 1 ? "" : "s");
        ImGui::EndMenu();
    }

    void MainMenuBar::OpenFolder(const std::filesystem::path& folder)
    {
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        if (error)
        {
            m_Logger.Error(MainMenuOrigin,
                           "Could not create folder '" + folder.string() + "': " + error.message(),
                           "main-menu.folder.create");
            return;
        }

        const auto result = reinterpret_cast<std::intptr_t>(
            ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        if (result <= 32)
        {
            m_Logger.Error(MainMenuOrigin, "Could not open folder '" + folder.string() + "'.", "main-menu.folder.open");
        }
    }
}  // namespace PureMirror::Overlay
