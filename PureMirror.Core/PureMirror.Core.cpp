// clang-format off
#include "pch.h"
// clang-format on

#include "PureMirror.Core.h"

#include <imgui.h>

const RendererAPI* g_rendererAPI;

void Initialize(const RendererAPI* rendererAPI)
{
    g_rendererAPI = rendererAPI;
}

const char* GetImguiVersion()
{
    return ImGui::GetVersion();
}

void SetContext(ImGuiContext* context)
{
    ImGui::SetCurrentContext(context);
}

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT HandleInput(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
}

void Render(const RenderContext* renderContext)
{
    ImGui::ShowDemoWindow();

    ImGui::SetNextWindowSize(ImVec2(250, 250));
    ImGui::Begin("wow such a nice name");
    static bool enabled = false;
    if (ImGui::Button("Open"))
    {
        enabled = !enabled;
    }
    if (enabled)
    {
        auto texture = g_rendererAPI->LoadTexture("test.png");
        auto texture2 = g_rendererAPI->LoadTexture("test2.png");
        if (texture.TextureID)
        {
            ImGui::Image(texture.TextureID, ImVec2(100, 100));
        }
        else
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            auto drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(pos, ImVec2(pos.x + 100, pos.y + 100), IM_COL32(255, 0, 0, 255));
        }
        if (texture2.TextureID)
        {
            ImGui::Image(texture2.TextureID, ImVec2(200, 200));
        }
        else
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            auto drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(pos, ImVec2(pos.x + 200, pos.y + 200), IM_COL32(255, 255, 0, 255));
        }
    }

    ImGui::End();
}

static CoreAPI g_CoreAPI = MAKE_CORE_API(.Initialize = Initialize,
                                         .GetImguiVersion = GetImguiVersion,
                                         .SetContext = SetContext,
                                         .HandleInput = HandleInput,
                                         .Render = Render);

extern "C"
{
    PUREMIRRORCORE_API CoreAPI* GetCoreAPI(void)
    {
        return &g_CoreAPI;
    }
}
