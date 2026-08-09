#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "TextureAsset.h"

class IGpuUploader
{
  public:
    virtual ~IGpuUploader() = default;

    virtual void UploadTexture(const std::shared_ptr<TextureAsset>& asset) = 0;
};
