// clang-format off
#include "pch.h"
// clang-format on

#include "console.h"

#include <hooks/hooks.h>

void Console::Alloc()
{
#ifndef DISABLE_LOGGING_CONSOLE
    AllocConsole();

    SetConsoleTitleA("PureMirror - Debug Console");

    freopen_s(reinterpret_cast<FILE**>(stdin), "conin$", "r", stdin);
    freopen_s(reinterpret_cast<FILE**>(stdout), "conout$", "w", stdout);

    ::ShowWindow(GetConsoleWindow(), SW_SHOW);
#endif
}

void Console::Free()
{
#ifndef DISABLE_LOGGING_CONSOLE
    fclose(stdin);
    fclose(stdout);

    if (H::bShuttingDown)
    {
        ::ShowWindow(GetConsoleWindow(), SW_HIDE);
    }
    else
    {
        FreeConsole();
    }
#endif
}
