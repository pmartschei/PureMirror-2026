#include "pch.h"

#include "OverlayScriptHost.h"

#include <imgui.h>

namespace PureMirror::Overlay
{
    namespace
    {
        LogOrigin PluginOrigin(const std::string_view pluginId)
        {
            return {.Type = LogOriginType::Plugin,
                    .Identifier = std::string(pluginId),
                    .DisplayName = std::string(pluginId)};
        }
    }  // namespace

    OverlayScriptHost::OverlayScriptHost(Logger& logger) : m_Logger(logger) {}

    void OverlayScriptHost::BeginScriptCall(const std::string_view pluginId)
    {
        static_cast<void>(pluginId);
        RecoverOpenScopes();
    }

    void OverlayScriptHost::EndScriptCall(const std::string_view pluginId)
    {
        static_cast<void>(pluginId);
        RecoverOpenScopes();
    }

    void OverlayScriptHost::LogInfo(const std::string_view pluginId, const std::string_view message)
    {
        m_Logger.Info(PluginOrigin(pluginId), message);
    }

    bool OverlayScriptHost::BeginWindow(const std::string_view pluginId, const std::string_view title)
    {
        static_cast<void>(pluginId);
        const std::string ownedTitle(title);
        const auto isVisible = ImGui::Begin(ownedTitle.c_str());
        m_UiScopes.Open(std::string(pluginId), "ImGui::Begin()", "ImGui::End()", [] { ImGui::End(); });
        return isVisible;
    }

    void OverlayScriptHost::EndWindow(const std::string_view pluginId)
    {
        const auto closed =
            m_UiScopes.Close("ImGui::End()", [this](const ScriptUiScope& scope) { LogRecoveredScope(scope); });
        if (!closed)
            LogUnexpectedClose(pluginId, "ImGui::End()");
    }

    void OverlayScriptHost::Text(const std::string_view pluginId, const std::string_view value)
    {
        static_cast<void>(pluginId);
        const std::string ownedValue(value);
        ImGui::TextUnformatted(ownedValue.c_str());
    }

    bool OverlayScriptHost::Button(const std::string_view pluginId, const std::string_view label)
    {
        static_cast<void>(pluginId);
        const std::string ownedLabel(label);
        return ImGui::Button(ownedLabel.c_str());
    }

    bool OverlayScriptHost::BeginMenu(const std::string_view pluginId, const std::string_view label)
    {
        const auto ownedLabel = std::string(label) + "###menu-" + std::string(pluginId) + '-' + std::string(label);
        if (!ImGui::BeginMenu(ownedLabel.c_str()))
            return false;
        m_UiScopes.Open(std::string(pluginId), "ImGui::BeginMenu()", "ImGui::EndMenu()", [] { ImGui::EndMenu(); });
        return true;
    }

    void OverlayScriptHost::EndMenu(const std::string_view pluginId)
    {
        const auto closed =
            m_UiScopes.Close("ImGui::EndMenu()", [this](const ScriptUiScope& scope) { LogRecoveredScope(scope); });
        if (!closed)
            LogUnexpectedClose(pluginId, "ImGui::EndMenu()");
    }

    bool OverlayScriptHost::MenuItem(const std::string_view pluginId, const std::string_view label)
    {
        const auto ownedLabel = std::string(label) + "###item-" + std::string(pluginId) + '-' + std::string(label);
        return ImGui::MenuItem(ownedLabel.c_str());
    }

    void OverlayScriptHost::MenuSeparator(const std::string_view pluginId)
    {
        static_cast<void>(pluginId);
        ImGui::Separator();
    }

    void OverlayScriptHost::RecoverOpenScopes()
    {
        m_UiScopes.CloseAll([this](const ScriptUiScope& scope) { LogRecoveredScope(scope); });
    }

    void OverlayScriptHost::LogRecoveredScope(const ScriptUiScope& scope)
    {
        m_Logger.Warning(PluginOrigin(scope.OwnerId),
                         "Plugin did not call " + scope.ClosingCommand + " after " + scope.OpeningCommand +
                             "; the host closed the UI scope automatically.",
                         "script.ui.scope.recovered." + scope.OwnerId + '.' + scope.ClosingCommand);
    }

    void OverlayScriptHost::LogUnexpectedClose(const std::string_view pluginId, const std::string_view closingCommand)
    {
        m_Logger.Warning(PluginOrigin(pluginId),
                         "Plugin called " + std::string(closingCommand) +
                             " without a matching open UI scope; the call was ignored.",
                         "script.ui.scope.unmatched." + std::string(pluginId) + '.' + std::string(closingCommand));
    }
}  // namespace PureMirror::Overlay
