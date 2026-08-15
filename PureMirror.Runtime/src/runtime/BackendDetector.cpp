// clang-format off
#include "pch.h"
// clang-format on

#include "BackendDetector.h"

#include "console/console.h"
#include "Runtime.h"
#include "hooks/backend/dx12/hook_directx12.h"
#include "hooks/backend/vulkan/hook_vulkan.h"

void BackendDetector::Count(const std::shared_ptr<IRenderer>& renderer) noexcept
{
    if (!m_activeRenderer || m_activeRenderer != renderer)
    {
        RendererType type = renderer->GetType();
        m_counts[static_cast<size_t>(type)]++;
        if (m_counts[static_cast<size_t>(type)] > REQUIRED_DETECTION_VALUE)
        {
            LOG("Detected RendererType: %d\n", type);
            m_counts.fill(0);
            SetActiveRenderer(renderer);
        }
    }
}

bool BackendDetector::IsActiveRenderer(IRenderer& renderer) const noexcept
{
    return GetActiveRenderer() == &renderer;
}

void BackendDetector::ResetActiveRenderer() noexcept
{
    if (m_activeRenderer)
    {
        m_RenderThread.Stop();
        m_activeRenderer->SetRenderThread(nullptr);
        m_activeRenderer->Reset();
        m_activeRenderer.reset();
    }
}

void BackendDetector::SetActiveRenderer(std::shared_ptr<IRenderer> renderer) noexcept
{
    ResetActiveRenderer();
    m_activeRenderer = std::move(renderer);
    m_activeRenderer->SetRenderThread(&m_RenderThread);
}
