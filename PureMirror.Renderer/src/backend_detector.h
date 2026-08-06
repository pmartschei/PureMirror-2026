#pragma once
#include "pch.h"

enum class RendererType
{
    Unknown = 0,
    DirectX9,
    DirectX10,
    DirectX11,
    DirectX12,
    Vulkan,
    OpenGl,
    Count,
};

class BackendDetector
{
  public:
    static BackendDetector& Instance()
    {
        static BackendDetector instance;
        return instance;
    }

    void Count(RendererType type);
    inline RendererType GetActiveRenderer() const;

  private:
    BackendDetector() = default;
    ~BackendDetector() = default;
    BackendDetector(const BackendDetector&) = delete;
    BackendDetector& operator=(const BackendDetector&) = delete;

    void ResetRenderer();
    void SetRenderer(RendererType type);
    inline static constexpr uint64_t REQUIRED_DETECTION_VALUE = 100;
    std::array<uint64_t, static_cast<size_t>(RendererType::Count)> m_counts{};
    RendererType m_detectedRendererType = RendererType::Unknown;
    RendererType m_currentRendererType = RendererType::Unknown;
};
