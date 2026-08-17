#pragma once

#include "../../core/commands/CommandRegistry.h"
#include "../../core/logger/LogMessage.h"
#include "../../core/logger/Logger.h"
#include "../logging/LogMessageDesignerWindow.h"

#include <array>
#include <cstdint>

namespace PureMirror::Overlay
{
    class ConsoleWindow
    {
      public:
        ConsoleWindow(Logger& logger, CommandRegistry& commands);

        void Render();
        [[nodiscard]] bool IsOpen() const;
        void SetOpen(bool open);

      private:
        [[nodiscard]] bool PassesFilter(const LogMessage& message) const;
        void ExecuteInput();

        Logger& m_Logger;
        CommandRegistry& m_Commands;
        LogMessageDesignerWindow m_MessageDesigner;
        bool m_Open{true};
        bool m_AutoScroll{true};
        bool m_ScrollToBottom{true};
        std::array<bool, 2> m_EnabledOriginTypes{true, true};
        std::array<bool, 5> m_EnabledLevels{true, true, true, true, true};
        std::array<char, 192> m_TextFilter{};
        std::array<char, 96> m_OriginFilter{};
        std::array<char, 512> m_CommandInput{};
        std::uint64_t m_LastSequence{};
    };
}  // namespace PureMirror::Overlay
