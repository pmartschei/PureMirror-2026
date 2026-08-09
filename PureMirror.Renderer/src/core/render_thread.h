#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include <imgui.h>
#include <src/utils/imgui_drawdata_snapshot.h>

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
    void EndRead();
    void Stop();

    void AddImageUsage(ImTextureID textureID);

  private:
    void Loop();
    std::atomic_bool m_IsRunning = false;
    std::atomic_bool m_ShouldTerminate = false;

    ImGuiContext* m_ImguiContext;
    std::function<void(RenderThread&)> m_RenderCallback;
    std::jthread m_Thread;

    ImGuiDrawDataSnapshot m_ImGuiDrawDataSnapshot;
};
