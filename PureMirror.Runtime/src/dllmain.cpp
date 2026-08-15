// clang-format off
#include "pch.h"
// clang-format on

#include "console/console.h"
#include "runtime/BackendDetector.h"
#include "external/minhook/MinHook.h"
#include "hooks/hooks.h"
#include "utils/utils.h"

DWORD WINAPI OnProcessAttach(LPVOID lpParam);
DWORD WINAPI OnProcessDetach(LPVOID lpParam);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);

        HANDLE hHandle = CreateThread(NULL, 0, OnProcessAttach, hinstDLL, 0, NULL);
        if (hHandle != NULL)
        {
            CloseHandle(hHandle);
        }
    }
    else if (fdwReason == DLL_PROCESS_DETACH && !lpReserved)
    {
        OnProcessDetach(NULL);
    }

    return TRUE;
}

DWORD WINAPI OnProcessAttach(LPVOID lpParam)
{
    Console::Alloc();

    MH_Initialize();
    H::Init();

    return 0;
}

DWORD WINAPI OnProcessDetach(LPVOID lpParam)
{
    H::Free();
    MH_Uninitialize();

    Console::Free();

    return 0;
}
