#pragma once
// clang-format off
#include "pch.h"
// clang-format on

struct TextureAsset
{
    std::string Path;

    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t Channels = 0;
    std::vector<std::byte> Pixels;

    std::filesystem::file_time_type LastWriteTime{};
};
