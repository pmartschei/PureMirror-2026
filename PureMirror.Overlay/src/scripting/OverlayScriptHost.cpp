#include "pch.h"

#include "OverlayScriptHost.h"

#include <imgui.h>

namespace PureMirror::Overlay
{
    OverlayScriptHost::OverlayScriptHost(Logger& logger) : m_Logger(logger) {}

    void OverlayScriptHost::LogInfo(const std::string_view pluginId, const std::string_view message)
    {
        m_Logger.Info(
            {.Type = LogOriginType::Plugin, .Identifier = std::string(pluginId), .DisplayName = std::string(pluginId)},
            message);
    }

    bool OverlayScriptHost::BeginWindow(const std::string_view pluginId, const std::string_view title)
    {
        static_cast<void>(pluginId);
        const std::string ownedTitle(title);
        return ImGui::Begin(ownedTitle.c_str());
    }

    void OverlayScriptHost::EndWindow(const std::string_view pluginId)
    {
        static_cast<void>(pluginId);
        ImGui::End();
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
}  // namespace PureMirror::Overlay
