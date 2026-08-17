#pragma once

#include <cstdint>

namespace PureMirror::Overlay
{
    enum class LogLevel : std::uint8_t
    {
        Trace,
        Debug,
        Info,
        Warning,
        Error
    };
}  // namespace PureMirror::Overlay
