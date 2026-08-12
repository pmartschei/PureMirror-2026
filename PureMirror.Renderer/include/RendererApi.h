#pragma once
#include "Texture.h"

#include <cstdint>
#include <wtypes.h>

#define RENDERER_API_VERSION 2

inline constexpr ULONG_PTR DIRECT_GAME_INPUT_MARKER = 0x50554D49;  // "PUMI"

struct RendererAPI
{
    // Keep this as first and second
    uint32_t Version;
    uint32_t Size;

    const char* (*GetImguiVersion)();
    HWND (*GetWindow)();

    // Call this
    Texture (*LoadTexture)(const char* path);
};

#define RENDERER_CONTEXT_API_VERSION 1

struct RenderContext
{
    // Keep this as first and second
    uint32_t Version;
    uint32_t Size;

    // Currently unused, might be used in the future
};
