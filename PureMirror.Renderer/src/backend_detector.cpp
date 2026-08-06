// clang-format off
#include "pch.h"
// clang-format on

#include "backend_detector.h"

#include "console/console.h"
#include "hooks/backend/dx12/hook_directx12.h"
#include "hooks/backend/vulkan/hook_vulkan.h"

void BackendDetector::Count(RendererType type)
{
    auto activeRenderer = GetActiveRenderer();
    if (activeRenderer == RendererType::Unknown || activeRenderer != type)
    {
        m_counts[static_cast<size_t>(type)]++;
        if (m_counts[static_cast<size_t>(type)] > REQUIRED_DETECTION_VALUE)
        {
            LOG("Detected RendererType: %d\n", type);
            m_counts.fill(0);
            ResetRenderer();
            SetRenderer(type);
        }
    }
}

inline RendererType BackendDetector::GetActiveRenderer() const
{
    return m_currentRendererType;
}

void BackendDetector::ResetRenderer()
{
    switch (m_currentRendererType)
    {
    case RendererType::DirectX12:
        DX12::Unhook();
        break;
    case RendererType::Vulkan:
        VK::Unhook();
        break;
    default:
        // Unknown or not implemented
        break;
    }

    m_currentRendererType = RendererType::Unknown;
}

void BackendDetector::SetRenderer(RendererType type)
{
    m_currentRendererType = type;
}
