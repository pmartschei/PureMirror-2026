// clang-format off
#include "pch.h"
// clang-format on

#include "DX12Renderer.h"

#include <console/console.h>
#include <hooks/backend/dx12/hook_directx12.h>

DX12Renderer::DX12Renderer() {}

DX12Renderer::~DX12Renderer()
{
    if (!m_GpuUploader)
        return;

    for (auto& [path, texture] : m_Textures)
        m_GpuUploader->ReleaseTexture(texture);

    m_Textures.clear();
}

void DX12Renderer::Initialize(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, UINT capacity, UINT offset)
{
    m_Device = device;

    assert(m_Device);

    ComPtr<IDXGIFactory4> factory;

    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        throw std::runtime_error("DX12Renderer: Could not CreateDXGIFactory1");

    const LUID adapterLuid = m_Device->GetAdapterLuid();

    ComPtr<IDXGIAdapter1> adapter;

    if (FAILED(factory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(&adapter))))
        throw std::runtime_error("DX12Renderer: Could not EnumAdapterByLuid");

    if (FAILED(adapter.As(&m_Adapter)))
        throw std::runtime_error("DX12Renderer: Could not get Adapter3");

    if (!m_GpuUploader)
        m_GpuUploader = std::make_unique<DX12GpuUploader>(device, srvHeap, capacity, offset);
}

void DX12Renderer::Shutdown()
{
    for (auto& [path, texture] : m_Textures)
        m_GpuUploader->ReleaseTexture(texture);

    m_Textures.clear();

    m_GpuUploader.reset();
}

Texture DX12Renderer::UploadAndRetrieveTexture(const std::shared_ptr<TextureAsset>& asset)
{
    if (!asset)
        return {};

    if (auto it = m_Textures.find(asset->Path); it != m_Textures.end())
    {
        const auto& dx12Texture = it->second;

        return {dx12Texture->ImGuiID,
                ImVec2(static_cast<float>(dx12Texture->Width), static_cast<float>(dx12Texture->Height))};
    }

    auto dx12Texture = m_GpuUploader->UploadTexture(asset);

    m_Textures.emplace(asset->Path, dx12Texture);

    Texture result{};

    result.Size = ImVec2(static_cast<float>(dx12Texture->Width), static_cast<float>(dx12Texture->Height));

    if (!dx12Texture || !m_GpuUploader->IsReady(dx12Texture))
        return result;

    result.TextureID = dx12Texture->ImGuiID;

    return result;
}

void DX12Renderer::CleanUpTextures()
{
    CleanUpFailedTextures();

    auto memoryInfo = GetMemoryUsage();

    if (memoryInfo.Budget == 0)
        return;

    const double usage = static_cast<double>(memoryInfo.CurrentUsage) / static_cast<double>(memoryInfo.Budget);

    if (usage < CLEANUP_THRESHOLD)
    {
        return;
    }

    CleanUpUnusedTextures(memoryInfo);
}

void DX12Renderer::MarkTexturesAsUsed(const std::unordered_set<ImTextureID>& usedTextures)
{
    const UINT64 currentFrame = ImGui::GetFrameCount();

    for (auto& [path, texture] : m_Textures)
    {
        if (!texture || !m_GpuUploader->IsReady(texture))
            continue;

        if (usedTextures.contains(texture->ImGuiID))
            texture->LastUsedFrame = currentFrame;
    }
}

RendererType DX12Renderer::GetType() noexcept
{
    return RendererType::DirectX12;
}

void DX12Renderer::Reset()
{
    Shutdown();
    SetRenderThread(nullptr);
    DX12::Unhook();
}

void DX12Renderer::SetRenderThread(RenderThread* renderThread)
{
    m_RenderThread = renderThread;
}

void DX12Renderer::CleanUpFailedTextures()
{
    for (auto it = m_Textures.begin(); it != m_Textures.end();)
    {
        auto& texture = it->second;

        if (!texture)
        {
            it = m_Textures.erase(it);
            continue;
        }

        const TextureState state = texture->State.load(std::memory_order_acquire);

        if (state == TextureState::Failed)
        {
            m_GpuUploader->ReleaseTexture(texture);
            it = m_Textures.erase(it);
            continue;
        }

        ++it;
    }
}

DXGI_QUERY_VIDEO_MEMORY_INFO DX12Renderer::GetMemoryUsage()
{
    DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo{};

    if (!m_Adapter)
        return memoryInfo;

    if (FAILED(m_Adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo)))
    {
        return {};
    }

    return memoryInfo;
}

void DX12Renderer::CleanUpUnusedTextures(DXGI_QUERY_VIDEO_MEMORY_INFO& memoryInfo)
{
    const UINT64 currentFrame = ImGui::GetFrameCount();

    std::vector<std::shared_ptr<DX12Texture>> candidates;

    // INFO: If the renderer somehow does render faster than 60 because it was changed, then we need to dynamically
    // update this number here
    constexpr uint64_t KeepAliveFrames = 60 * 5;

    for (const auto& [path, texture] : m_Textures)
    {
        if (!texture)
            continue;

        if (texture->State.load(std::memory_order_acquire) != TextureState::Ready)
        {
            continue;
        }

        if (currentFrame - texture->LastUsedFrame < KeepAliveFrames)
            continue;

        candidates.push_back(texture);
    }

    std::ranges::sort(candidates, std::greater{}, &DX12Texture::SizeInBytes);

    for (const auto& texture : candidates)
    {
        auto it = std::ranges::find_if(m_Textures, [&](const auto& pair) { return pair.second == texture; });

        if (it == m_Textures.end())
            continue;

        m_GpuUploader->ReleaseTexture(texture);
        m_Textures.erase(it);

        if (memoryInfo.CurrentUsage >= texture->SizeInBytes)
            memoryInfo.CurrentUsage -= texture->SizeInBytes;

        const double newUsage = static_cast<double>(memoryInfo.CurrentUsage) / static_cast<double>(memoryInfo.Budget);

        if (newUsage < CLEANUP_THRESHOLD_DESIRED)
            break;
    }
}
