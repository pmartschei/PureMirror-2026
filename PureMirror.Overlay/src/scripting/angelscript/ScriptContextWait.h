#pragma once

#include "ScriptTask.h"

#include <vector>

namespace PureMirror::Overlay
{
    enum class ScriptWaitMode
    {
        One,
        All,
        Any
    };

    class ScriptContextWait
    {
      public:
        ScriptContextWait(ScriptWaitMode mode, std::vector<ScriptTask*> tasks, ScriptTask* resultTask = nullptr);
        ~ScriptContextWait();

        ScriptContextWait(const ScriptContextWait&) = delete;
        ScriptContextWait& operator=(const ScriptContextWait&) = delete;

        [[nodiscard]] bool IsReady() const noexcept;
        void PrepareResume();

      private:
        [[nodiscard]] ScriptTask* FirstCompletedTask() const noexcept;

        ScriptWaitMode m_Mode;
        std::vector<ScriptTask*> m_Tasks;
        ScriptTask* m_ResultTask{};
    };
}  // namespace PureMirror::Overlay
