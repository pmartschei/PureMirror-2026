#pragma once

namespace Hooks
{
    void Init();
    void Free();

    inline bool bShuttingDown;
}  // namespace Hooks

namespace H = Hooks;
