#pragma once

#include "LogColor.h"
#include "LogLevel.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace PureMirror::Overlay
{
    struct LogMessage
    {
        static constexpr std::uint32_t DefaultOccurrenceLimit{10};

        std::uint64_t m_Sequence{};
        std::string m_MessageId;
        std::chrono::system_clock::time_point m_Timestamp;
        std::string m_Content;
        LogLevel m_Level{LogLevel::Info};
        LogColor m_Color;
        std::string m_Origin;
        std::uint32_t m_OccurrenceLimit{DefaultOccurrenceLimit};
    };
}  // namespace PureMirror::Overlay
