#include "pch.h"

#include "dllmain.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    DisableThreadLibraryCalls(hModule);
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        dll = LoadLibraryEx(L".\\dxgi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (dll == NULL)
        {
            MessageBox(0, L"Cannot load original dxgi.dll library", L"Proxy", MB_ICONERROR);
            ExitProcess(0);
        }

        clientDLL = LoadLibrary(L".\\PureMirror.Runtime.dll");

        DWORD err = GetLastError();
        if (clientDLL != NULL)
        {
            // auto init = GetProcAddress(clientDLL, "Initialize");
            // init();
        }
        OriginalCreateDXGIFactory = reinterpret_cast<PFN_CreateDXGIFactory>(GetProcAddress(dll, "CreateDXGIFactory"));
        OriginalCreateDXGIFactory1 =
            reinterpret_cast<PFN_CreateDXGIFactory1>(GetProcAddress(dll, "CreateDXGIFactory1"));
        OriginalCreateDXGIFactory2 =
            reinterpret_cast<PFN_CreateDXGIFactory2>(GetProcAddress(dll, "CreateDXGIFactory2"));
        OriginalDXGIDeclareAdapterRemovalSupport = reinterpret_cast<PFN_DXGIDeclareAdapterRemovalSupport>(
            GetProcAddress(dll, "DXGIDeclareAdapterRemovalSupport"));
        OriginalDXGIGetDebugInterface1 =
            reinterpret_cast<PFN_DXGIGetDebugInterface1>(GetProcAddress(dll, "DXGIGetDebugInterface1"));
        break;
    }
    case DLL_PROCESS_DETACH:
    {
        FreeLibrary(dll);
    }
    break;
    }
    return TRUE;
}
