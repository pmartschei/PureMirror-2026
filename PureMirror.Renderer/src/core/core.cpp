// clang-format off
#include "pch.h"
// clang-format on
#include "core.h"

#include "../external/imgui/imgui_impl_win32.h"
#include "imgui.h"

namespace Core
{
    static HWND g_hWindow = NULL;
    static WNDPROC oWndProc;
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

    void InitializeLibs() {}

    bool HasContext()
    {
        return !!ImGui::GetCurrentContext();
    }

    void InitializeContext(HWND hwnd)
    {
        if (ImGui::GetCurrentContext())
            return;

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hwnd);

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = io.LogFilename = nullptr;

        oWndProc =
            reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    }

    void Render()
    {
        ImGui::ShowDemoWindow();
    }

    void Shutdown()
    {
        if (oWndProc)
        {
            SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        }
    }
}  // namespace Core
