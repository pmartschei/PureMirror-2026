#pragma once
#include "RendererApi.h"

#include <cstdint>
#include <imgui.h>
#include <wtypes.h>

#define OVERLAY_API_VERSION 1

struct OverlayAPI
{
    // Keep this as first and second
    uint32_t Version;
    uint32_t Size;

    void (*Initialize)(const RendererAPI* rendererAPI);

    const char* (*GetImguiVersion)();

    void (*SetContext)(ImGuiContext*);
    LRESULT (*HandleInput)(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void (*Render)(const RenderContext*);
};

#define MAKE_OVERLAY_API(...) (OverlayAPI{.Version = OVERLAY_API_VERSION, .Size = sizeof(OverlayAPI), __VA_ARGS__})
