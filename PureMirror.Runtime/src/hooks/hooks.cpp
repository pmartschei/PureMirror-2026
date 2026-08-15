// clang-format off
#include "pch.h"
// clang-format on

#include "hooks.h"

#include "backend/dx10/hook_directx10.h"
#include "backend/dx11/hook_directx11.h"
#include "backend/dx12/hook_directx12.h"
#include "backend/dx9/hook_directx9.h"
#include "backend/opengl/hook_opengl.h"
#include "backend/vulkan/hook_vulkan.h"

#include <console/console.h>
#include <runtime/Runtime.h>
#include <external/minhook/MinHook.h>
#include <utils/utils.h>

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
        // DX9::Hook();
        // DX10::Hook();
        // DX11::Hook();
        DX12::Hook();
        VK::Hook();
        // GL::Hook();

#ifdef DISABLE_LOGGING_CONSOLE
        if (bNoConsole)
        {
            FreeConsole();
        }
#endif
    }

    void Free()
    {
        Runtime::Shutdown();
        MH_DisableHook(MH_ALL_HOOKS);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // GL::Unhook();
        VK::Unhook();
        DX12::Unhook();
        // DX11::Unhook();
        // DX10::Unhook();
        // DX9::Unhook();
    }
}  // namespace Hooks
