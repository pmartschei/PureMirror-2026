// clang-format off
#include "pch.h"
// clang-format on

#include "core.h"

#include "BackendDetector.h"
#include "console/console.h"
#include "external/imgui/imgui_impl_win32.h"
#include "graphics/TextureManager.h"
#include "graphics/dx12/DX12GpuUploader.h"
#include "include/Texture.h"
#include "include/core_api.h"
#include "utils/utils.h"

#include <imgui.h>

namespace Core
{
    using GetCoreAPI_t = CoreAPI* (*)();
    static HINSTANCE g_coreDLL = NULL;
    static std::vector<CoreAPI*> g_CoreAPIs = {};

    static HWND g_hWindow = NULL;
    static WNDPROC oWndProc;

    TextureManager g_TextureManager;
    static LRESULT WINAPI WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        // if (Menu::bShowMenu) {
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

        // call core api

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

        g_hWindow = hwnd;
        oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
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

        if (BackendDetector::Instance().GetActiveRenderer())
        {
            return BackendDetector::Instance().GetActiveRenderer()->UploadAndRetrieveTexture(textureAsset);
        }

        return {};
    }

    void UnloadTexture(const char* path)
    {
        if (!path)
            return;

        if (BackendDetector::Instance().GetActiveRenderer())
            BackendDetector::Instance().GetActiveRenderer()->ReleaseTexture(path);

        g_TextureManager.Unload(path);
    }
}  // namespace Core
