// clang-format off
#include "pch.h"
// clang-format on

#include "core.h"

#include "../external/imgui/imgui_impl_win32.h"
#include "graphics/TextureManager.h"
#include "graphics/dx12/DX12GpuUploader.h"
#include "imgui.h"
#include "include/core_api.h"
#include "render_thread.h"

#include <include/Texture.h>

namespace Core
{
    using GetCoreAPI_t = CoreAPI* (*)();
    static HINSTANCE g_coreDLL = NULL;
    static HWND g_hWindow = NULL;
    static WNDPROC oWndProc;
    static std::vector<CoreAPI*> g_CoreAPIs = {};
    RenderThread GlobalRenderThread;
    TextureManager g_TextureManager;
    std::unique_ptr<DX12GpuUploader> g_GpuUploader;
    std::vector<std::shared_ptr<DX12Texture>> m_Textures;
    IRenderer* GlobalRenderer;
    static LRESULT WINAPI WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_KEYDOWN)
        {
            // if (wParam == VK_HOME)
            //{
            //     HANDLE hHandle = CreateThread(NULL, 0, ReinitializeGraphicalHooks, NULL, 0, NULL);
            //     if (hHandle != NULL)
            //         CloseHandle(hHandle);
            //     return 0;
            // }
            // else if (wParam == VK_END)
            //{
            //     H::bShuttingDown = true;
            //     U::UnloadDLL();
            //     return 0;
            // }
            // else if (wParam == VK_NUMPAD5)
            //{
            //     H::Free();
            //     if (U::GetRenderingBackend() == VULKAN)
            //     {
            //         U::SetRenderingBackend(DIRECTX12);
            //     }
            //     else
            //     {
            //         U::SetRenderingBackend(VULKAN);
            //     }
            //     H::Init();
            // }
        }
        else if (uMsg == WM_DESTROY)
        {
            // HANDLE hHandle = CreateThread(NULL, 0, ReinitializeGraphicalHooks, hWnd, 0, NULL);
            // if (hHandle != NULL)
            //     CloseHandle(hHandle);
        }

        LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        // if (Menu::bShowMenu) {
        //     ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

        // (Doesn't work for some games like 'Sid Meier's Civilization VI')
        // Window may not maximize from taskbar because 'H::bShowDemoWindow' is set to true by default. ('hooks.hpp')
        //
        // return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam) == 0;
        //}

        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    void InitializeLibs()
    {
        g_coreDLL = LoadLibraryA("PureMirror.Core.dll");
        if (g_coreDLL)
        {
            auto GetCoreAPI = reinterpret_cast<GetCoreAPI_t>(GetProcAddress(g_coreDLL, "GetCoreAPI"));

            if (GetCoreAPI)
            {
                auto coreAPI = GetCoreAPI();
                // TODO Version check imgui check whatever
                g_CoreAPIs.push_back(coreAPI);
            }
        }
    }

    bool HasContext()
    {
        return !!ImGui::GetCurrentContext();
    }

    void InitializeContext(HWND hwnd)
    {
        if (ImGui::GetCurrentContext())
            return;

        auto imguiContext = ImGui::CreateContext();
        ImGui_ImplWin32_Init(hwnd);
        InitializeLibs();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = io.LogFilename = nullptr;

        oWndProc =
            reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    }

    void Render()
    {
        for (auto& coreApi : g_CoreAPIs)
        {
            coreApi->Render();
        }
    }

    void Shutdown()
    {
        if (oWndProc)
        {
            SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        }
    }

    Texture LoadTexture(const char* path)
    {
        auto textureAsset = g_TextureManager.Load(path);

        if (!textureAsset)
            return {};

        return GlobalRenderer->UploadAndRetrieveTexture(textureAsset);
    }
}  // namespace Core
