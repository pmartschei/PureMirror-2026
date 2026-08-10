#pragma once

namespace Hooks
{
    void Init();
    void Free();

    // TODO remove, as it is unused
    inline bool bShuttingDown;
}  // namespace Hooks

namespace H = Hooks;
