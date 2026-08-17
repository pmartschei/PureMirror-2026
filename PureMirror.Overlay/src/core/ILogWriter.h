#pragma once

#include "LogMessage.h"

namespace PureMirror::Overlay
{
    class ILogWriter
    {
      public:
        virtual ~ILogWriter() = default;

        virtual void Write(const LogMessage& message) = 0;
    };
}  // namespace PureMirror::Overlay
