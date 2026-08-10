#pragma once
#include "RendererApi.h"

#include <cstdint>
#include <imgui.h>
#include <wtypes.h>

#define CORE_API_VERSION 1

struct CoreAPI
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

#define MAKE_CORE_API(...) (CoreAPI{.Version = CORE_API_VERSION, .Size = sizeof(CoreAPI), __VA_ARGS__})
