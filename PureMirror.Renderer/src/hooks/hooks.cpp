// clang-format off
#include "pch.h"
// clang-format on

#include "hooks.h"

#include "../console/console.h"
#include "../core/core.h"
#include "../external/minhook/MinHook.h"
#include "../utils/utils.h"
#include "backend/dx10/hook_directx10.h"
#include "backend/dx11/hook_directx11.h"
#include "backend/dx12/hook_directx12.h"
#include "backend/dx9/hook_directx9.h"
#include "backend/opengl/hook_opengl.h"
#include "backend/vulkan/hook_vulkan.h"

static int vulkanCounter = 0;
static int dxd12Counter = 0;
static std::mutex g_mReinitHooksGuard;

static DWORD WINAPI ReinitializeGraphicalHooks(LPVOID lpParam)
{
    std::lock_guard<std::mutex> guard{g_mReinitHooksGuard};

    LOG("[!] Hooks will reinitialize!\n");

    HWND hNewWindow = U::GetProcessWindow();
    while (hNewWindow == reinterpret_cast<HWND>(lpParam))
    {
        hNewWindow = U::GetProcessWindow();
    }

    H::bShuttingDown = true;

    H::Free();
    H::Init();

    H::bShuttingDown = false;

    return 0;
}

namespace Hooks
{

    void Init()
    {
#ifdef DISABLE_LOGGING_CONSOLE
        bool bNoConsole = GetConsoleWindow() == NULL;
        if (bNoConsole)
        {
            AllocConsole();
        }
#endif

        RenderingBackend_t eRenderingBackend = U::GetRenderingBackend();
        // switch (eRenderingBackend) {
        //     case DIRECTX9:
        //         DX9::Hook(g_hWindow);
        //         break;
        //     case DIRECTX10:
        //         DX10::Hook(g_hWindow);
        //         break;
        //     case DIRECTX11:
        //         DX11::Hook(g_hWindow);
        //         break;
        //     case DIRECTX12:
        //         DX12::Hook(g_hWindow);
        //         break;
        //     case OPENGL:
        //         GL::Hook(g_hWindow);
        //         break;
        //     case VULKAN:
        //         VK::Hook(g_hWindow);
        //         break;
        // }
        DX12::Hook();
        VK::Hook();

#ifdef DISABLE_LOGGING_CONSOLE
        if (bNoConsole)
        {
            FreeConsole();
        }
#endif
    }

    void Free()
    {
        Core::Shutdown();
        MH_DisableHook(MH_ALL_HOOKS);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        DX12::Unhook();
        VK::Unhook();
    }
}  // namespace Hooks
