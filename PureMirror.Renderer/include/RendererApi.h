#pragma once
#include "Texture.h"

#include <cstdint>

#define RENDERER_API_VERSION 1

struct RendererAPI
{
    // Keep this as first and second
    uint32_t Version;
    uint32_t Size;

    const char* (*GetImguiVersion)();

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
