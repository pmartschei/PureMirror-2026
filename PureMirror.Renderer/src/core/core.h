#pragma once

#include "render_thread.h"

namespace Core
{
    bool HasContext();
    void InitializeLibs();
    void InitializeContext(HWND hwnd);
    void Render();
    void Shutdown();

    extern RenderThread GlobalRenderThread;
}  // namespace Core
