// clang-format off
#include "pch.h"
// clang-format on

#include "PureMirror.Core.h"

#include "ClientListener.h"
#include "WindowInput.h"

#include <imgui.h>

const RendererAPI* g_rendererAPI;

ClientListener g_ClientListener;

std::vector<std::string> m_Messages;

std::unique_ptr<PureMirror::WindowInput> g_WindowInput;

void Initialize(const RendererAPI* rendererAPI)
{
    g_rendererAPI = rendererAPI;
    g_WindowInput = std::make_unique<PureMirror::WindowInput>(g_rendererAPI->GetWindow());
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
    auto result = ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

    if (result != 0)
        return result;

    auto& io = ImGui::GetIO();

    switch (uMsg)
    {
    case WM_KILLFOCUS:
        return 0;
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
        return 0;
    case WM_LBUTTONUP:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONUP:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEHWHEEL:
    case WM_MOUSEWHEEL:
    {
        return io.WantCaptureMouse;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        return io.WantCaptureKeyboard;
    }
    default:
        return 0;
    }

    return 0;
}

void Render(const RenderContext* renderContext)
{
    for (auto& d : g_ClientListener.TakeMessages())
    {
        m_Messages.push_back(d);
    }
    ImGui::ShowDemoWindow();

    ImGui::SetNextWindowSize(ImVec2(250, 250));
    ImGui::Begin("wow such a nice name");
    static bool enabled = true;
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
            ImGui::Dummy(ImVec2(100, 100));
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
            ImGui::Dummy(ImVec2(200, 200));
            auto drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(pos, ImVec2(pos.x + 200, pos.y + 200), IM_COL32(255, 255, 0, 255));
        }
    }

    ImGui::End();

    ImGui::Begin("Messages");
    for (auto& message : m_Messages)
    {
        ImGui::BulletText(message.c_str());
    }

    if (ImGui::Button("Send") && g_WindowInput)
    {
        g_WindowInput->SendKey(VK_RETURN);

        g_WindowInput->SendKeyDown(VK_CONTROL);
        g_WindowInput->SendKey('A');
        g_WindowInput->SendKeyUp(VK_CONTROL);
        g_WindowInput->SendKey(VK_DELETE);

        g_WindowInput->SendTextUtf8("Mein Text");
        g_WindowInput->SendKey(VK_RETURN);

        // Restore the last chat entry
        g_WindowInput->SendKey(VK_RETURN);
        g_WindowInput->SendKey(VK_UP);
        g_WindowInput->SendKey(VK_UP);
        g_WindowInput->SendKey(VK_ESCAPE);
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
