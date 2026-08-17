#pragma once

#include "../../core/logger/LogColor.h"
#include "../../core/logger/Logger.h"

#include <array>
#include <cstdint>

namespace PureMirror::Overlay
{
    class LogMessageDesignerWindow
    {
      public:
        explicit LogMessageDesignerWindow(Logger& logger);

        void Render();
        void Open();

      private:
        void ResetForm();
        void Send();

        Logger& m_Logger;
        bool m_Open{};
        bool m_UseCustomColor{};
        bool m_HasSendResult{};
        bool m_LastSendWasLogged{};
        int m_LevelIndex{2};
        int m_OriginTypeIndex{};
        std::array<char, 128> m_MessageId{};
        std::array<char, 128> m_OriginIdentifier{};
        std::array<char, 128> m_OriginDisplayName{};
        std::array<char, 1'024> m_Content{};
        LogColor m_Color;
        std::uint32_t m_OccurrenceLimit{LogMessage::DefaultOccurrenceLimit};
    };
}  // namespace PureMirror::Overlay
