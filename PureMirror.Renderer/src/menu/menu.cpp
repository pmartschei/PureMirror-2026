#include "pch.h"

#include "menu.h"

#include "imgui.h"
#include "../external/imgui/imgui_impl_win32.h"

namespace ig = ImGui;

namespace Menu {
    void InitializeContext(HWND hwnd) {
        if (ig::GetCurrentContext( ))
            return;

        ImGui::CreateContext( );
        ImGui_ImplWin32_Init(hwnd);

        ImGuiIO& io = ImGui::GetIO( );
        io.IniFilename = io.LogFilename = nullptr;
    }

    void Render( ) {
        if (!bShowMenu)
            return;

        ig::ShowDemoWindow( );
    }
} // namespace Menu
