#pragma once

#include "LogFileStartPosition.h"

#include <chrono>

namespace PureMirror
{
    struct LogFileListenerOptions
    {
        std::chrono::milliseconds PollInterval{100};
        LogFileStartPosition InitialPosition{LogFileStartPosition::End};
    };
}  // namespace PureMirror
