#pragma once

#include "graphics/RenderThread.h"

#include <include/Texture.h>

namespace Core
{
    bool HasContext();
    void InitializeLibs();
    void InitializeContext(HWND hwnd);
    void Render();
    void Shutdown();
    Texture LoadTexture(const char* path);
    void UnloadTexture(const char* path);
}  // namespace Core
