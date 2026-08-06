#pragma once

namespace Core
{
    bool HasContext();
    void InitializeLibs();
    void InitializeContext(HWND hwnd);
    void Render();
    void Shutdown();
}  // namespace Core
