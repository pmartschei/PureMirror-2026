#pragma once

namespace Utils
{
    HWND GetProcessWindow();
    void UnloadDLL();

    HMODULE GetCurrentImageBase();

    int GetCorrectDXGIFormat(int eCurrentFormat);

    void osSleep(double timeInSeconds);
}  // namespace Utils

namespace U = Utils;
