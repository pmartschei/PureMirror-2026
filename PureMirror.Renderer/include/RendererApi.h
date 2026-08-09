#pragma once
#include "Texture.h"

#include <cstdint>

#define RENDERER_API_VERSION 1

struct RendererApi
{
    // Keep this as first and second
    uint32_t Version;

    Texture (*LoadTexture)(const char* path);
};
