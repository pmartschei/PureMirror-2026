// clang-format off
#include "pch.h"
// clang-format on
#include "DX12GpuUploader.h"

DX12GpuUploader::DX12GpuUploader(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, uint32_t descriptorCapacity)
    : m_DescriptorAllocator(device, srvHeap, descriptorCapacity)
{
    if (!device)
        throw std::invalid_argument("DX12GpuUploader: device is null");

    if (!srvHeap)
        throw std::invalid_argument("DX12GpuUploader: SRV heap is null");

    m_Device = device;

    m_DescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // ------------------------------------------------------------
    // COPY QUEUE
    // ------------------------------------------------------------

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    HRESULT hr = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue));

    if (FAILED(hr))
        throw std::runtime_error("Failed to create DX12 upload command queue");

    // ------------------------------------------------------------
    // COMMAND ALLOCATOR
    // ------------------------------------------------------------

    hr = m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_CommandAllocator));

    if (FAILED(hr))
        throw std::runtime_error("Failed to create DX12 upload command allocator");

    // ------------------------------------------------------------
    // COMMAND LIST
    // ------------------------------------------------------------

    hr = m_Device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_COPY, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList));

    if (FAILED(hr))
        throw std::runtime_error("Failed to create DX12 upload command list");

    // Command lists start open.
    hr = m_CommandList->Close();

    if (FAILED(hr))
        throw std::runtime_error("Failed to close initial DX12 upload command list");

    // ------------------------------------------------------------
    // FENCE
    // ------------------------------------------------------------

    hr = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));

    if (FAILED(hr))
        throw std::runtime_error("Failed to create DX12 upload fence");

    // ------------------------------------------------------------
    // FENCE EVENT
    // ------------------------------------------------------------

    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!m_FenceEvent)
        throw std::runtime_error("Failed to create DX12 fence event");

    // ------------------------------------------------------------
    // THREAD
    // ------------------------------------------------------------

    m_Thread = std::jthread([this](std::stop_token stopToken) { ThreadMain(stopToken); });
}

DX12GpuUploader::~DX12GpuUploader()
{
    m_Thread.request_stop();

    if (m_Thread.joinable())
        m_Thread.join();

    if (m_FenceEvent)
    {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }
}

std::shared_ptr<DX12Texture> DX12GpuUploader::UploadTexture(std::shared_ptr<TextureAsset> asset)
{
    if (!asset || asset->Pixels.empty())
        return nullptr;

    auto texture = std::make_shared<DX12Texture>();

    texture->Width = static_cast<uint32_t>(asset->Width);

    texture->Height = static_cast<uint32_t>(asset->Height);

    UploadRequest request;
    request.Type = RequestType::Upload;
    request.Asset = std::move(asset);
    request.Texture = texture;

    {
        std::lock_guard lock(m_Mutex);

        m_Queue.push(std::move(request));
    }

    m_Condition.notify_one();

    return texture;
}

void DX12GpuUploader::ReleaseTexture(std::shared_ptr<DX12Texture> texture)
{
    if (!texture)
        return;

    UploadRequest request;
    request.Type = RequestType::Release;
    request.Texture = std::move(texture);

    {
        std::lock_guard lock(m_Mutex);

        m_Queue.push(std::move(request));
    }

    m_Condition.notify_one();
}

bool DX12GpuUploader::IsReady(const std::shared_ptr<DX12Texture>& texture) const
{
    return texture && texture->Ready.load(std::memory_order_acquire);
}
void DX12GpuUploader::ThreadMain(std::stop_token stopToken)
{
    while (true)
    {
        UploadRequest request;

        {
            std::unique_lock lock(m_Mutex);

            m_Condition.wait(lock, stopToken, [this] { return !m_Queue.empty(); });

            if (m_Queue.empty())
            {
                if (stopToken.stop_requested())
                    break;

                continue;
            }

            request = std::move(m_Queue.front());
            m_Queue.pop();
        }

        try
        {
            if (request.Type == RequestType::Release)
                ReleaseTextureInternal(request.Texture);
            else
                UploadTextureInternal(request);
        }
        catch (...)
        {
            request.Texture->Ready.store(false, std::memory_order_release);

            // Hier später Logging einbauen.
        }
    }
}
void DX12GpuUploader::UploadTextureInternal(const UploadRequest& request)
{
    const TextureAsset& asset = *request.Asset;
    DX12Texture& texture = *request.Texture;

    const UINT64 pixelSize = static_cast<UINT64>(asset.Width) * static_cast<UINT64>(asset.Height) * 4;

    // ------------------------------------------------------------
    // TEXTURE RESOURCE
    // ------------------------------------------------------------

    D3D12_RESOURCE_DESC textureDesc{};

    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    textureDesc.Alignment = 0;

    textureDesc.Width = static_cast<UINT64>(asset.Width);

    textureDesc.Height = static_cast<UINT>(asset.Height);

    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;

    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;

    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = m_Device->CreateCommittedResource(&defaultHeap,
                                                   D3D12_HEAP_FLAG_NONE,
                                                   &textureDesc,
                                                   D3D12_RESOURCE_STATE_COPY_DEST,
                                                   nullptr,
                                                   IID_PPV_ARGS(&texture.Resource));

    if (FAILED(hr))
        throw std::runtime_error("Failed to create DX12 texture resource");

    // ------------------------------------------------------------
    // CALCULATE UPLOAD BUFFER SIZE
    // ------------------------------------------------------------

    UINT64 uploadSize = 0;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT numRows = 0;
    UINT64 rowSize = 0;

    m_Device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSize, &uploadSize);

    // ------------------------------------------------------------
    // UPLOAD BUFFER
    // ------------------------------------------------------------

    D3D12_RESOURCE_DESC uploadDesc{};

    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;

    uploadDesc.Alignment = 0;
    uploadDesc.Width = uploadSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;

    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.SampleDesc.Quality = 0;

    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    hr = m_Device->CreateCommittedResource(&uploadHeap,
                                           D3D12_HEAP_FLAG_NONE,
                                           &uploadDesc,
                                           D3D12_RESOURCE_STATE_GENERIC_READ,
                                           nullptr,
                                           IID_PPV_ARGS(&texture.UploadBuffer));

    if (FAILED(hr))
        throw std::runtime_error("Failed to create DX12 upload buffer");

    // ------------------------------------------------------------
    // COPY CPU DATA INTO UPLOAD BUFFER
    // ------------------------------------------------------------

    uint8_t* mapped = nullptr;

    D3D12_RANGE readRange{};
    readRange.Begin = 0;
    readRange.End = 0;

    hr = texture.UploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mapped));

    if (FAILED(hr))
        throw std::runtime_error("Failed to map DX12 upload buffer");

    const std::byte* source = asset.Pixels.data();

    const uint8_t* pixels = reinterpret_cast<const uint8_t*>(source);

    const UINT sourceRowPitch = static_cast<UINT>(asset.Width * 4);

    const UINT destinationRowPitch = footprint.Footprint.RowPitch;

    for (UINT row = 0; row < numRows; ++row)
    {
        std::memcpy(mapped + footprint.Offset + static_cast<size_t>(row) * destinationRowPitch,
                    pixels + static_cast<size_t>(row) * sourceRowPitch,
                    sourceRowPitch);
    }

    texture.UploadBuffer->Unmap(0, nullptr);

    // ------------------------------------------------------------
    // RESET COMMAND LIST
    // ------------------------------------------------------------

    hr = m_CommandAllocator->Reset();

    if (FAILED(hr))
        throw std::runtime_error("Failed to reset DX12 upload allocator");

    hr = m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);

    if (FAILED(hr))
        throw std::runtime_error("Failed to reset DX12 upload command list");

    // ------------------------------------------------------------
    // COPY BUFFER -> TEXTURE
    // ------------------------------------------------------------

    D3D12_TEXTURE_COPY_LOCATION destination{};

    destination.pResource = texture.Resource.Get();

    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    destination.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION sourceLocation{};

    sourceLocation.pResource = texture.UploadBuffer.Get();

    sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

    sourceLocation.PlacedFootprint = footprint;

    m_CommandList->CopyTextureRegion(&destination, 0, 0, 0, &sourceLocation, nullptr);

    // ------------------------------------------------------------
    // CLOSE
    // ------------------------------------------------------------

    hr = m_CommandList->Close();

    if (FAILED(hr))
        throw std::runtime_error("Failed to close DX12 upload command list");

    // ------------------------------------------------------------
    // EXECUTE
    // ------------------------------------------------------------

    ID3D12CommandList* commandLists[] = {m_CommandList.Get()};

    m_CommandQueue->ExecuteCommandLists(1, commandLists);

    // ------------------------------------------------------------
    // FENCE
    // ------------------------------------------------------------

    const uint64_t fenceValue = m_NextFenceValue.fetch_add(1, std::memory_order_relaxed);

    hr = m_CommandQueue->Signal(m_Fence.Get(), fenceValue);

    if (FAILED(hr))
        throw std::runtime_error("Failed to signal DX12 upload fence");

    texture.FenceValue = fenceValue;

    // ------------------------------------------------------------
    // WAIT
    //
    // This makes the texture completely ready before we expose
    // it to the renderer.
    // ------------------------------------------------------------

    WaitForFence(fenceValue);

    // ------------------------------------------------------------
    // CREATE SRV
    //
    // This is CPU-side and safe after the resource exists.
    // ------------------------------------------------------------
    texture.Descriptor = m_DescriptorAllocator.Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    m_Device->CreateShaderResourceView(texture.Resource.Get(), &srvDesc, texture.Descriptor.Cpu);

    // ImGui DX12 backend uses the GPU descriptor handle.
    texture.ImGuiID = reinterpret_cast<ImTextureID>(texture.Descriptor.Gpu.ptr);

    texture.Ready.store(true, std::memory_order_release);

    // We no longer need the CPU upload buffer.
    texture.UploadBuffer.Reset();
}

void DX12GpuUploader::ReleaseTextureInternal(const std::shared_ptr<DX12Texture>& texture)
{
    if (!texture)
        return;

    if (texture->FenceValue > 0)
        WaitForFence(texture->FenceValue);

    if (texture->Descriptor.IsValid())
    {
        m_DescriptorAllocator.Free(texture->Descriptor);
        texture->Descriptor = {};
    }

    texture->Resource.Reset();
    texture->UploadBuffer.Reset();
    texture->ImGuiID = {};
    texture->FenceValue = 0;
    texture->Ready.store(false, std::memory_order_release);
}

void DX12GpuUploader::WaitForFence(uint64_t value)
{
    if (m_Fence->GetCompletedValue() >= value)
        return;

    HRESULT hr = m_Fence->SetEventOnCompletion(value, m_FenceEvent);

    if (FAILED(hr))
        throw std::runtime_error("Failed to set DX12 fence event");

    WaitForSingleObject(m_FenceEvent, INFINITE);
}
