#pragma once

#include "ScriptTask.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace PureMirror::Overlay
{
    class ScriptCoroutineArgument;

    class ScriptCoroutine
    {
      public:
        ScriptCoroutine(std::string moduleId, asIScriptContext& context, ScriptTask& task);
        ~ScriptCoroutine();

        ScriptCoroutine(const ScriptCoroutine&) = delete;
        ScriptCoroutine& operator=(const ScriptCoroutine&) = delete;

        void KeepArgument(std::unique_ptr<ScriptCoroutineArgument> argument);

        std::string ModuleId;
        asIScriptContext& Context;
        ScriptTask& Task;
        std::uint64_t ResumeFrame{};
        std::chrono::steady_clock::time_point ResumeTime;
        bool IsFinished{};

      private:
        std::vector<std::unique_ptr<ScriptCoroutineArgument>> m_Arguments;
    };
}  // namespace PureMirror::Overlay
