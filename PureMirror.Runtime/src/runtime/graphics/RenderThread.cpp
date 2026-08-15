// clang-format off
#include "pch.h"
// clang-format on

#include "RenderThread.h"

#include <runtime/BackendDetector.h>
#include <utils/utils.h>

std::expected<void, RenderThreadError> RenderThread::Start(ImGuiContext& imguiContext,
                                                           std::function<void(RenderThread&)> renderCallback)
{
    if (m_IsRunning)
    {
        return std::unexpected(RenderThreadError::AlreadyRunning);
    }

    if (!renderCallback)
    {
        return std::unexpected(RenderThreadError::InvalidRenderCallback);
    }

    m_ImguiContext = &imguiContext;
    m_RenderCallback = std::move(renderCallback);

    m_Thread = std::jthread([this](std::stop_token stopToken) { Loop(stopToken); });
    m_IsRunning = true;

    return {};
}

ImDrawData* RenderThread::BeginRead()
{
    if (!m_IsRunning)
        return nullptr;

    return m_ImGuiDrawDataSnapshot.BeginRead();
}

void RenderThread::Stop()
{
    if (!m_IsRunning)
        return;

    m_Thread.request_stop();

    if (m_Thread.joinable())
        m_Thread.join();

    m_ImGuiDrawDataSnapshot.Clear();

    m_RenderCallback = {};
    m_ImguiContext = nullptr;

    m_IsRunning = false;
}

void RenderThread::Loop(std::stop_token stopToken)
{
    SetThreadDescription(GetCurrentThread(), L"PureMirror Render Thread");

    constexpr int FPS = 60;
    using namespace std::chrono;

    const auto frameDuration = round<steady_clock::duration>(duration<double>{1.0 / FPS});
    auto nextFrame = steady_clock::now();

    while (!stopToken.stop_requested())
    {
        m_ImGuiDrawDataSnapshot.BeginUpdate();

        if (stopToken.stop_requested())
        {
            m_ImGuiDrawDataSnapshot.CancelUpdate();
            break;
        }

        ImGui::SetCurrentContext(m_ImguiContext);

        m_RenderCallback(*this);

        if (stopToken.stop_requested())
        {
            m_ImGuiDrawDataSnapshot.CancelUpdate();
            break;
        }

        m_ImGuiDrawDataSnapshot.Update(ImGui::GetDrawData());

        auto usedImages = m_ImGuiDrawDataSnapshot.CollectUsedImages();

        auto renderer = BackendDetector::Instance().GetActiveRenderer();

        if (renderer)
        {
            renderer->MarkTexturesAsUsed(usedImages);
            renderer->CleanUpTextures();
        }

        auto now = system_clock::now();

        nextFrame += frameDuration;

        std::unique_lock lock(m_SleepMutex);

        m_SleepCv.wait_until(lock, stopToken, nextFrame, [] { return false; });
    }
}
