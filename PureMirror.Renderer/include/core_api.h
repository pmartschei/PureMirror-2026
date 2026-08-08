#pragma once
#include <cstdint>
#include <imgui.h>

#define CORE_API_VERSION 1

struct CoreAPI
{
    // Keep this as first and second
    uint32_t Version;
    uint32_t Size;

    const char* (*GetImguiVersion)();
    void (*SetContext)(ImGuiContext*);
    void (*Render)();
};

#define MAKE_CORE_API(...) (CoreAPI{.Version = CORE_API_VERSION, .Size = sizeof(CoreAPI), __VA_ARGS__})
