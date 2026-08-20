#pragma once

#include <cstdint>

namespace PureMirror::Overlay
{
    class IScriptSuspensionRuntime
    {
      public:
        virtual ~IScriptSuspensionRuntime() = default;

        virtual void HostYield() = 0;
        virtual void HostSleep(std::uint64_t timeInMs) = 0;
    };
}  // namespace PureMirror::Overlay
