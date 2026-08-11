#pragma once

// clang-format off
#include "pch.h"
// clang-format on

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
    void Loop();

    std::atomic_bool m_IsRunning = false;
    std::atomic_bool m_ShouldTerminate = false;

    ImGuiContext* m_ImguiContext;
    std::function<void(RenderThread&)> m_RenderCallback;
    std::jthread m_Thread;

    ImGuiDrawDataSnapshot m_ImGuiDrawDataSnapshot;
};
