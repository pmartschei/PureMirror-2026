#pragma once

#include "LogColor.h"
#include "LogLevel.h"
#include "LogOrigin.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace PureMirror::Overlay
{
    struct LogMessage
    {
        static constexpr std::uint32_t DefaultOccurrenceLimit{10};

        std::uint64_t Sequence{};
        std::string MessageId;
        std::chrono::system_clock::time_point Timestamp;
        std::string Content;
        LogLevel Level{LogLevel::Info};
        LogColor Color;
        LogOrigin Origin;
        std::uint32_t OccurrenceLimit{DefaultOccurrenceLimit};
    };
}  // namespace PureMirror::Overlay
