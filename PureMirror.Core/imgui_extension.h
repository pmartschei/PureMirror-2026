#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "imgui_internal.h"

// Usage:
// - call just before EndFrame()/Render(), e.g. SetWindowVoidInputPassthrough(ImGui::FindWindowByName("Dear ImGui
// Demo")). Purpose:
// - Allow mouse hover/click in the empty space (void) of window to be passed down underlying game/app.
// - This works by clearing the io.WantCaptureMouse flag, so low-level input handler can keep dispatching mouse to
// underlying game/app.
// - This doesn't handle multiple overlapped imgui windows, so it is generally expected you use this on a single window
// that is in the background,
//   or on multiple non-overlapping windows. Calling this on overlapping windows will erroneously clear
//   io.WantCaptureMouse when hovering one, regardless of another one that may sit behind. This is possible to fix with
//   some work.
// - Similarly, handle focusing the window.
// Discussions at https://github.com/ocornut/imgui/issues/8360
void SetWindowVoidInputPassthrough(ImGuiWindow* window);

static void UpdateWindowVoidInputPassthroughMouse(ImGuiWindow* window)
{
    ImGuiContext& g = *GImGui;
    if (g.HoveredId != 0)
        return;

    // If a popup is open it eats the click on void (FIXME: This could be an optional thing? user may still use
    // io.WantCaptureMouse vs ioWantCaptureMouseUnlessPopupClose)
    if (g.OpenPopupStack.Size > 0)
        return;

    // Any active item is assumed to take inputs unless g.ActiveIdAllowOverlap is set
    // (the variable is historically a bit misnamed, but it allows e.g. InputText() to be active while allowing hovering
    // other items)
    if (g.ActiveId != 0 && !g.ActiveIdAllowOverlap)
    {
        // Unless we're clicking/dragging in the window's void itself
        // When clicking on window's void we allow it to take the ActiveId, as it conveniently allows us to track that
        // we are dragging from void.
        if (g.ActiveId != window->MoveId)
            return;
    }
    else
    {
        if (g.HoveredWindow != NULL && g.HoveredWindow->RootWindow != window)
            return;
    }

    // Write to io.WantCaptureMouse directly, so it is available in e.g. low-level input handler _before_ the next
    // NewFrame(). We may need to call SetNextFrameWantCaptureMouse(): what matters is that WantCaptureXXX are cleared
    // at the time of low-level input handlers applying their filter.
    g.IO.WantCaptureMouse = false;
    ImGui::SetNextFrameWantCaptureMouse(false);
}

static void UpdateWindowVoidInputPassthroughKeyboard(ImGuiWindow* window)
{
    ImGuiContext& g = *GImGui;
    if (g.NavWindow == NULL || g.NavWindow->RootWindow != window->RootWindow)
        return;

    // If any item is active in the window (e.g. an InputText, or a Button) we keep keyboard to imgui
    if (g.ActiveId != 0 && g.ActiveId != window->MoveId && g.ActiveIdWindow->RootWindow == window->RootWindow)
        return;

    // Allow navigation to work, tho it is likely you'd want to use ImGuiWindowFlags_NoNav on the window.
    // if (g.NavCursorVisible && g.NavId != 0)
    //    return;
    // TODO uncomment when library got updated

    // When navigation cursor is cleared, we disable nav on this window for the frame, preventing
    // e.g. arrow keys from resuming navigation in NavUpdate() while underlying game/app is using them.
    // Next frame's Begin() will clear the flag again. This means navigation on this window may only be activated via:
    // - Ctrl+Tabbing into the window.
    // - Using arrow key while an item is currently active (typically InputText, but it will also work while holding
    // mouse button over any item even tho that's a low affordance behavior).
    // - And pressing Escape will deactivate navigation and relinquish keyboard underlying game/app.
    // This seems like the best design as we want e.g. clicking a button to end up stealing keyboard.
    window->Flags |= ImGuiWindowFlags_NoNav;

    g.IO.WantCaptureKeyboard = false;
    ImGui::SetNextFrameWantCaptureKeyboard(false);
}

void SetWindowVoidInputPassthrough(ImGuiWindow* window)
{
    UpdateWindowVoidInputPassthroughMouse(window);
    UpdateWindowVoidInputPassthroughKeyboard(window);
}
