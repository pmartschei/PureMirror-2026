// clang-format off
#include "pch.h"
// clang-format on

#include "RenderThread.h"

#include <core/BackendDetector.h>
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

    m_ShouldTerminate = false;
    m_Thread = std::jthread(&RenderThread::Loop, this);
    m_IsRunning = true;
}

ImDrawData* RenderThread::BeginRead()
{
    return m_ImGuiDrawDataSnapshot.BeginRead();
}

void RenderThread::EndRead()
{
    m_ImGuiDrawDataSnapshot.EndRead();
}

void RenderThread::Stop()
{
    if (!m_IsRunning)
    {
        return;
    }
    m_ShouldTerminate = true;

    m_Thread.request_stop();
}

void RenderThread::Loop()
{
    SetThreadDescription(GetCurrentThread(), L"PureMirror Render Thread");
    const int m_FPS = 60;
    using namespace std::chrono;
    using dsec = duration<double>;
    auto invFpsLimit = round<system_clock::duration>(dsec{1. / m_FPS});
    auto m_BeginFrame = system_clock::now();
    auto m_EndFrame = m_BeginFrame + invFpsLimit;

    while (!m_ShouldTerminate)
    {
        m_ImGuiDrawDataSnapshot.Clear();

        ImGui::SetCurrentContext(m_ImguiContext);

        m_RenderCallback(*this);

        m_ImGuiDrawDataSnapshot.Update(ImGui::GetDrawData());

        auto usedImages = m_ImGuiDrawDataSnapshot.CollectUsedImages();

        BackendDetector::Instance().GetActiveRenderer()->MarkTexturesAsUsed(usedImages);

        BackendDetector::Instance().GetActiveRenderer()->CleanUpTextures();

        auto now = system_clock::now();

        std::chrono::duration<double> difference = m_EndFrame - now;

        if (difference.count() > 0.0)
        {
            Utils::osSleep(difference.count());
        }

        m_EndFrame = m_EndFrame + invFpsLimit;
        m_BeginFrame = now;
    }
}
