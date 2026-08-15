#pragma once
#include "pch.h"

#include "graphics/IRenderer.h"
#include "graphics/RendererType.h"

class BackendDetector
{
  public:
    static BackendDetector& Instance()
    {
        static BackendDetector instance;
        return instance;
    }

    void Count(const std::shared_ptr<IRenderer>& renderer) noexcept;
    [[nodiscard]] bool IsActiveRenderer(IRenderer& renderer) const noexcept;
    [[nodiscard]] inline IRenderer* GetActiveRenderer() const noexcept
    {
        return m_activeRenderer.get();
    };

  private:
    BackendDetector() = default;
    ~BackendDetector() = default;

    BackendDetector(const BackendDetector&) = delete;
    BackendDetector& operator=(const BackendDetector&) = delete;

    BackendDetector(BackendDetector&&) noexcept = default;
    BackendDetector& operator=(BackendDetector&&) noexcept = default;

    void ResetActiveRenderer() noexcept;
    void SetActiveRenderer(std::shared_ptr<IRenderer> renderer) noexcept;

    RenderThread m_RenderThread;
    std::shared_ptr<IRenderer> m_activeRenderer;
    inline static constexpr uint64_t REQUIRED_DETECTION_VALUE = 100;
    std::array<uint64_t, static_cast<size_t>(RendererType::Count)> m_counts{};
    RendererType m_currentRendererType = RendererType::Unknown;
};
