#pragma once

#include "graphics/IRenderer.h"
#include "render_thread.h"

namespace Core
{
    bool HasContext();
    void InitializeLibs();
    void InitializeContext(HWND hwnd);
    void Render();
    void Shutdown();
    Texture LoadTexture(const char* path);
    void UnloadTexture(const char* path);

    extern RenderThread GlobalRenderThread;
    extern IRenderer* GlobalRenderer;
}  // namespace Core
