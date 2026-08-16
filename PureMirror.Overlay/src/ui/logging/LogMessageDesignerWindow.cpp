#include "pch.h"

#include "LogMessageDesignerWindow.h"

#include <imgui.h>
#include <span>

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr std::array Levels{
            LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warning, LogLevel::Error};

        void CopyToBuffer(const std::string_view value, std::span<char> buffer)
        {
            const auto length = (std::min)(value.size(), buffer.size() - 1);
            std::ranges::copy(value.substr(0, length), buffer.begin());
            buffer[length] = '\0';
        }
    }  // namespace

    LogMessageDesignerWindow::LogMessageDesignerWindow(Logger& logger) : m_Logger(logger)
    {
        ResetForm();
    }

    void LogMessageDesignerWindow::Render()
    {
        if (!m_Open)
            return;

        ImGui::SetNextWindowSize(ImVec2(520.0f, 460.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Log Message Designer", &m_Open))
        {
            ImGui::End();
            return;
        }

        ImGui::InputTextWithHint("Message ID", "Empty creates a unique ID", m_MessageId.data(), m_MessageId.size());
        ImGui::InputTextWithHint("Origin", "PureMirror.Debug", m_Origin.data(), m_Origin.size());

        ImGui::SetNextItemWidth(180.0f);
        ImGui::Combo("Level", &m_LevelIndex, "Trace\0Debug\0Info\0Warning\0Error\0");

        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputScalar("Occurrence limit", ImGuiDataType_U32, &m_OccurrenceLimit);
        ImGui::TextDisabled("Messages with the same ID are logged at most this many times.");

        ImGui::Checkbox("Use custom color", &m_UseCustomColor);
        if (!m_UseCustomColor)
            ImGui::BeginDisabled();
        ImGui::ColorEdit4("Color", &m_Color.m_Red, ImGuiColorEditFlags_AlphaBar);
        if (!m_UseCustomColor)
            ImGui::EndDisabled();

        ImGui::TextUnformatted("Content");
        ImGui::InputTextMultiline("##log-message-content", m_Content.data(), m_Content.size(), ImVec2(-1.0f, 130.0f));

        const auto levelIndex = std::clamp(m_LevelIndex, 0, static_cast<int>(Levels.size() - 1));
        const auto level = Levels[static_cast<std::size_t>(levelIndex)];
        const auto previewColor = m_UseCustomColor ? m_Color : Logger::DefaultColor(level);
        ImGui::TextUnformatted("Preview");
        ImGui::TextColored(ImVec4(previewColor.m_Red, previewColor.m_Green, previewColor.m_Blue, previewColor.m_Alpha),
                           "[%s] [%s] %s",
                           Logger::LevelName(level),
                           m_Origin.data(),
                           m_Content.data());

        if (ImGui::Button("Send"))
            Send();
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
            ResetForm();

        if (m_HasSendResult)
        {
            ImGui::SameLine();
            if (m_LastSendWasLogged)
                ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "Logged");
            else
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Not logged (occurrence limit reached)");
        }

        ImGui::End();
    }

    void LogMessageDesignerWindow::Open()
    {
        m_Open = true;
    }

    void LogMessageDesignerWindow::ResetForm()
    {
        m_MessageId.fill('\0');
        m_Origin.fill('\0');
        m_Content.fill('\0');
        CopyToBuffer("debug.message", m_MessageId);
        CopyToBuffer("PureMirror.Debug", m_Origin);
        CopyToBuffer("Designed log message", m_Content);
        m_LevelIndex = static_cast<int>(LogLevel::Info);
        m_UseCustomColor = false;
        m_Color = Logger::DefaultColor(LogLevel::Info);
        m_OccurrenceLimit = LogMessage::DefaultOccurrenceLimit;
        m_HasSendResult = false;
        m_LastSendWasLogged = false;
    }

    void LogMessageDesignerWindow::Send()
    {
        const auto messagesBefore = m_Logger.Snapshot();
        const auto lastSequenceBefore = messagesBefore.empty() ? 0 : messagesBefore.back().m_Sequence;
        const auto levelIndex = std::clamp(m_LevelIndex, 0, static_cast<int>(Levels.size() - 1));
        const auto level = Levels[static_cast<std::size_t>(levelIndex)];
        const auto* color = m_UseCustomColor ? &m_Color : nullptr;
        m_Logger.Log(level, m_Origin.data(), m_Content.data(), m_MessageId.data(), color, m_OccurrenceLimit);

        const auto messagesAfter = m_Logger.Snapshot();
        m_LastSendWasLogged = !messagesAfter.empty() && messagesAfter.back().m_Sequence != lastSequenceBefore;
        m_HasSendResult = true;
    }
}  // namespace PureMirror::Overlay
