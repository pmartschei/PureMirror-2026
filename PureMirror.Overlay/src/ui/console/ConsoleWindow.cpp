#include "pch.h"

#include "ConsoleWindow.h"

#include <imgui.h>

namespace PureMirror::Overlay
{
    namespace
    {
        bool ContainsIgnoringCase(const std::string_view value, const std::string_view search)
        {
            if (search.empty())
                return true;
            return std::search(value.begin(),
                               value.end(),
                               search.begin(),
                               search.end(),
                               [](const unsigned char left, const unsigned char right)
                               { return std::tolower(left) == std::tolower(right); }) != value.end();
        }

        std::string FormatTimestamp(const std::chrono::system_clock::time_point timestamp)
        {
            const auto time = std::chrono::system_clock::to_time_t(timestamp);
            std::tm local{};
            localtime_s(&local, &time);
            const auto milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count() % 1000;
            std::array<char, 24> buffer{};
            std::snprintf(buffer.data(),
                          buffer.size(),
                          "%02d:%02d:%02d.%03lld",
                          local.tm_hour,
                          local.tm_min,
                          local.tm_sec,
                          static_cast<long long>(milliseconds));
            return buffer.data();
        }

        ImVec4 ToImGuiColor(const LogColor& color)
        {
            return {color.m_Red, color.m_Green, color.m_Blue, color.m_Alpha};
        }
    }  // namespace

    ConsoleWindow::ConsoleWindow(Logger& logger, CommandRegistry& commands) : m_Logger(logger), m_Commands(commands) {}

    void ConsoleWindow::Render()
    {
        if (!m_Open)
            return;

        ImGui::SetNextWindowSize(ImVec2(850.0f, 430.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("PureMirror Console", &m_Open))
        {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Clear"))
            m_Logger.Clear();
        ImGui::SameLine();
        if (ImGui::Button("Copy"))
        {
            std::string clipboard;
            for (const auto& message : m_Logger.Snapshot())
            {
                if (!PassesFilter(message))
                    continue;
                clipboard += '[' + FormatTimestamp(message.m_Timestamp) + "] [" + Logger::LevelName(message.m_Level) +
                             "] [" + message.m_Origin + "] " + message.m_Content + '\n';
            }
            ImGui::SetClipboardText(clipboard.c_str());
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_AutoScroll);

        ImGui::SetNextItemWidth(230.0f);
        ImGui::InputTextWithHint(
            "##console-text-filter", "Filter text or message ID", m_TextFilter.data(), m_TextFilter.size());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint(
            "##console-origin-filter", "Filter origin", m_OriginFilter.data(), m_OriginFilter.size());
        ImGui::SameLine();

        constexpr std::array levels{
            LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warning, LogLevel::Error};
        for (std::size_t index = 0; index < levels.size(); ++index)
        {
            if (index > 0)
                ImGui::SameLine();
            ImGui::Checkbox(Logger::LevelName(levels[index]), &m_EnabledLevels[index]);
        }

        const auto messages = m_Logger.Snapshot();
        if (!messages.empty() && messages.back().m_Sequence != m_LastSequence)
        {
            m_LastSequence = messages.back().m_Sequence;
            if (m_AutoScroll)
                m_ScrollToBottom = true;
        }

        const auto footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        ImGui::BeginChild("ConsoleMessages", ImVec2(0.0f, -footerHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginTable("ConsoleTable",
                              4,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 95.0f);
            ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 58.0f);
            ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_WidthFixed, 145.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto& message : messages)
            {
                if (!PassesFilter(message))
                    continue;

                const auto color = ToImGuiColor(message.m_Color);
                ImGui::PushID(static_cast<int>(message.m_Sequence));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", FormatTimestamp(message.m_Timestamp).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(color, "%s", Logger::LevelName(message.m_Level));
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(message.m_Origin.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextColored(color, "%s", message.m_Content.c_str());
                if (ImGui::IsItemHovered() && !message.m_MessageId.empty())
                    ImGui::SetTooltip(
                        "Message ID: %s\nOccurrence limit: %u", message.m_MessageId.c_str(), message.m_OccurrenceLimit);
                ImGui::PopID();
            }

            if (m_ScrollToBottom)
            {
                ImGui::SetScrollHereY(1.0f);
                m_ScrollToBottom = false;
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##console-command",
                                     "Enter a command (for example /help or /clear)",
                                     m_CommandInput.data(),
                                     m_CommandInput.size(),
                                     ImGuiInputTextFlags_EnterReturnsTrue))
        {
            ExecuteInput();
            ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::End();
    }

    bool ConsoleWindow::IsOpen() const
    {
        return m_Open;
    }

    void ConsoleWindow::SetOpen(const bool open)
    {
        m_Open = open;
    }

    bool ConsoleWindow::PassesFilter(const LogMessage& message) const
    {
        const auto levelIndex = static_cast<std::size_t>(message.m_Level);
        if (levelIndex >= m_EnabledLevels.size() || !m_EnabledLevels[levelIndex])
            return false;

        const std::string_view textFilter(m_TextFilter.data());
        const std::string_view originFilter(m_OriginFilter.data());
        return ContainsIgnoringCase(message.m_Origin, originFilter) &&
               (ContainsIgnoringCase(message.m_Content, textFilter) ||
                ContainsIgnoringCase(message.m_MessageId, textFilter));
    }

    void ConsoleWindow::ExecuteInput()
    {
        const std::string input(m_CommandInput.data());
        m_CommandInput.fill('\0');
        if (input.empty())
            return;

        m_Logger.Debug("PureMirror.Console", "> " + input, "console.command.input");
        const auto result = m_Commands.Execute(input);
        if (!result.m_Message.empty())
        {
            if (result.m_Status == CommandStatus::Executed)
                m_Logger.Info("PureMirror.Console", result.m_Message, "console.command.result");
            else
                m_Logger.Error("PureMirror.Console", result.m_Message, "console.command.error");
        }
        m_ScrollToBottom = true;
    }
}  // namespace PureMirror::Overlay
