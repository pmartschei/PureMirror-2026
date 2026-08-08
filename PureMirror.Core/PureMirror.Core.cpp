// clang-format off
#include "pch.h"
// clang-format on

#include "PureMirror.Core.h"

#include <imgui.h>

void Render();

const char* GetImguiVersion();
void SetContext(ImGuiContext* context);

static CoreAPI g_CoreAPI =
    MAKE_CORE_API(.GetImguiVersion = GetImguiVersion, .SetContext = SetContext, .Render = Render);

extern "C"
{
    PUREMIRRORCORE_API CoreAPI* GetCoreAPI(void)
    {
        return &g_CoreAPI;
    }
}

const char* GetImguiVersion()
{
    return ImGui::GetVersion();
}

void SetContext(ImGuiContext* context)
{
    ImGui::SetCurrentContext(context);
}

void Render()
{
    ImGui::ShowDemoWindow();
    if (ImGui::Begin("Hallo"))
        ImGui::End();
}
