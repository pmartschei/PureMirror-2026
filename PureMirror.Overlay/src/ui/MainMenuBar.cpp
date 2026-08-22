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

        std::string PluginMenuLabel(const PluginInfo& plugin, const std::string_view action)
        {
            auto label = plugin.Name + " (" + plugin.Version + ')';
            if (!plugin.IsExplicit && action != "load")
                label += " [dependency]";
            return label + "###" + std::string(action) + '-' + plugin.Id;
        }
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
        m_PluginManager.RenderMenu();

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

        if (ImGui::BeginMenu("Load"))
        {
            const auto availablePlugins = m_PluginManager.AvailablePlugins();
            if (availablePlugins.empty())
                ImGui::MenuItem("No unloaded plugins found", nullptr, false, false);
            for (const auto& plugin : availablePlugins)
            {
                const auto label = PluginMenuLabel(plugin, "load");
                if (ImGui::MenuItem(label.c_str()))
                    static_cast<void>(m_PluginManager.LoadPlugin(plugin.Id));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Unload"))
        {
            const auto loadedPlugins = m_PluginManager.LoadedPlugins();
            if (loadedPlugins.empty())
                ImGui::MenuItem("No plugins loaded", nullptr, false, false);
            for (const auto& plugin : loadedPlugins)
            {
                const auto label = PluginMenuLabel(plugin, "unload");
                if (ImGui::MenuItem(label.c_str()))
                    static_cast<void>(m_PluginManager.UnloadPlugin(plugin.Id));
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Reload"))
        {
            const auto loadedPlugins = m_PluginManager.LoadedPlugins();
            if (loadedPlugins.empty())
                ImGui::MenuItem("No plugins loaded", nullptr, false, false);
            for (const auto& plugin : loadedPlugins)
            {
                const auto label = PluginMenuLabel(plugin, "reload");
                if (ImGui::MenuItem(label.c_str()))
                    static_cast<void>(m_PluginManager.ReloadPlugin(plugin.Id));
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        const auto loadedPluginCount = m_PluginManager.LoadedPluginCount();
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
