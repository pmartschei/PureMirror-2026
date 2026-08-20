#include "pch.h"

#include "ScriptContextWait.h"

namespace PureMirror::Overlay
{
    ScriptContextWait::ScriptContextWait(const ScriptWaitMode mode,
                                         std::vector<ScriptTask*> tasks,
                                         ScriptTask* resultTask)
        : m_Mode(mode), m_Tasks(std::move(tasks)), m_ResultTask(resultTask)
    {
        for (const auto task : m_Tasks)
            task->AddRef();
        if (m_ResultTask != nullptr)
            m_ResultTask->AddRef();
    }

    ScriptContextWait::~ScriptContextWait()
    {
        for (const auto task : m_Tasks)
            task->Release();
        if (m_ResultTask != nullptr)
            m_ResultTask->Release();
    }

    bool ScriptContextWait::IsReady() const noexcept
    {
        if (m_Mode == ScriptWaitMode::All)
            return std::ranges::all_of(m_Tasks, &ScriptTask::IsCompleted);
        return FirstCompletedTask() != nullptr;
    }

    void ScriptContextWait::PrepareResume()
    {
        if (m_Mode != ScriptWaitMode::Any || m_ResultTask == nullptr)
            return;
        auto* completedTask = FirstCompletedTask();
        if (completedTask != nullptr)
            m_ResultTask->ResolveTo(*completedTask);
    }

    ScriptTask* ScriptContextWait::FirstCompletedTask() const noexcept
    {
        ScriptTask* firstCompleted{};
        for (const auto task : m_Tasks)
        {
            if (!task->IsCompleted())
                continue;
            if (firstCompleted == nullptr || task->CompletionOrder() < firstCompleted->CompletionOrder())
                firstCompleted = task;
        }
        return firstCompleted;
    }
}  // namespace PureMirror::Overlay
