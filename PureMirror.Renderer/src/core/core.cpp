// clang-format off
#include "pch.h"
// clang-format on

#include "core.h"

#include "BackendDetector.h"
#include "console/console.h"
#include "external/imgui/imgui_impl_win32.h"
#include "graphics/TextureManager.h"
#include "graphics/dx12/DX12GpuUploader.h"
#include "utils/utils.h"

#include <imgui.h>
#include <include/CoreApi.h>
#include <include/Texture.h>

namespace Core
{
    using GetCoreAPI_t = CoreAPI* (*)();
    static HINSTANCE g_coreDLL = NULL;
    static std::vector<CoreAPI*> g_CoreAPIs = {};

    static HWND g_hWindow = NULL;
    static WNDPROC oWndProc;

    TextureManager g_TextureManager;

    const char* GetImguiVersion();

    RendererAPI g_rendererAPI = {.Version = RENDERER_API_VERSION,
                                 .Size = sizeof(RendererAPI),
                                 .GetImguiVersion = GetImguiVersion,
                                 .LoadTexture = LoadTexture};

    static LRESULT WINAPI WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        for (auto& coreApi : g_CoreAPIs)
        {
            coreApi->HandleInput(hWnd, uMsg, wParam, lParam);
        }

        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    const char* GetImguiVersion()
    {
        return ImGui::GetVersion();
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

                coreAPI->Initialize(&g_rendererAPI);
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

    void Render(RenderContext& renderContext)
    {
        for (auto& coreApi : g_CoreAPIs)
        {
            coreApi->Render(&renderContext);
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
        auto textureAsset = g_TextureManager.ReloadIfChanged(path);

        if (!textureAsset)
            return {};

        if (BackendDetector::Instance().GetActiveRenderer())
        {
            return BackendDetector::Instance().GetActiveRenderer()->UploadAndRetrieveTexture(textureAsset);
        }

        return {};
    }
}  // namespace Core
