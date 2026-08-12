#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "imgui_internal.h"

#include <initializer_list>

// Allows input on the empty area of multiple transparent overlay windows to
// pass through to the game. All possibly overlapping passthrough windows must
// be supplied together so the decision is independent of call order.
namespace PureMirror::ImGuiExtension
{
    inline bool ContainsRoot(const std::initializer_list<ImGuiWindow*> windows, const ImGuiWindow* root)
    {
        if (!root)
            return false;

        for (const auto* window : windows)
        {
            if (window && window->RootWindow == root)
                return true;
        }
        return false;
    }

    inline bool HasCapturingActiveItem(ImGuiContext& context, const std::initializer_list<ImGuiWindow*> windows)
    {
        if (context.ActiveId == 0 || context.ActiveIdAllowOverlap)
            return false;

        const auto* activeRoot = context.ActiveIdWindow ? context.ActiveIdWindow->RootWindow : nullptr;
        if (!activeRoot)
            return true;

        // Dragging the void of one of our overlay windows may pass through. Every
        // other active item, including items in non-overlay windows, keeps capture.
        return !ContainsRoot(windows, activeRoot) || context.ActiveId != activeRoot->MoveId;
    }

    inline void UpdateMouse(ImGuiContext& context, const std::initializer_list<ImGuiWindow*> windows)
    {
        if (context.HoveredId != 0 || context.OpenPopupStack.Size > 0 || HasCapturingActiveItem(context, windows))
            return;

        const auto* hoveredRoot = context.HoveredWindow ? context.HoveredWindow->RootWindow : nullptr;

        if (hoveredRoot && hoveredRoot->TitleBarRect().Contains(context.IO.MousePos))
        {
            return;
        }
        // A regular ImGui window above the transparent overlays owns this point.
        // Keep capture for its empty area as well as for its widgets.
        if (hoveredRoot && !ContainsRoot(windows, hoveredRoot))
            return;

        context.IO.WantCaptureMouse = false;
        ImGui::SetNextFrameWantCaptureMouse(false);
    }

    inline void UpdateKeyboard(ImGuiContext& context, const std::initializer_list<ImGuiWindow*> windows)
    {
        if (!context.NavWindow || !ContainsRoot(windows, context.NavWindow->RootWindow))
            return;

        const auto* activeRoot = context.ActiveIdWindow ? context.ActiveIdWindow->RootWindow : nullptr;
        if (context.ActiveId != 0 && activeRoot == context.NavWindow->RootWindow &&
            context.ActiveId != context.NavWindow->RootWindow->MoveId)
        {
            return;
        }

        context.NavWindow->RootWindow->Flags |= ImGuiWindowFlags_NoNav;
        context.IO.WantCaptureKeyboard = false;
        ImGui::SetNextFrameWantCaptureKeyboard(false);
    }

    inline void SetWindowsVoidInputPassthrough(const std::initializer_list<ImGuiWindow*> windows)
    {
        if (!GImGui || windows.size() == 0)
            return;

        auto& context = *GImGui;
        UpdateMouse(context, windows);
        UpdateKeyboard(context, windows);
    }

    inline void SetWindowVoidInputPassthrough(ImGuiWindow* window)
    {
        SetWindowsVoidInputPassthrough({window});
    }
}  // namespace PureMirror::ImGuiExtension
