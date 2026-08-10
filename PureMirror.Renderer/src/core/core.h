#pragma once

#include "graphics/RenderThread.h"

#include <include/Texture.h>

namespace Core
{
    bool HasContext();
    void InitializeLibs();
    void InitializeContext(HWND hwnd);
    void Render(RenderContext& renderContext);
    void Shutdown();
    Texture LoadTexture(const char* path);
}  // namespace Core
