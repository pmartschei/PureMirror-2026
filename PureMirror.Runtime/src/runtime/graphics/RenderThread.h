#pragma once
#include "pch.h"

#include "ImGuiDrawDataSnapshot.h"

#include <imgui.h>
#include <include/RendererApi.h>

enum class RenderThreadError
{
    AlreadyRunning,
    InvalidRenderCallback,
    ThreadCreationFailed
};

class RenderThread
{
  public:
    std::expected<void, RenderThreadError> Start(ImGuiContext& imguiContext,
                                                 std::function<void(RenderThread&)> renderCallback);
    ImDrawData* BeginRead();
    void Stop();

  private:
    void Loop(std::stop_token stopToken);

    std::atomic_bool m_IsRunning = false;

    ImGuiContext* m_ImguiContext = nullptr;
    std::function<void(RenderThread&)> m_RenderCallback;
    std::jthread m_Thread;

    std::mutex m_SleepMutex;
    std::condition_variable_any m_SleepCv;

    ImGuiDrawDataSnapshot m_ImGuiDrawDataSnapshot;
};
