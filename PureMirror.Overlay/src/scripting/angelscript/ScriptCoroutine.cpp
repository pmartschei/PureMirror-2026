#include "pch.h"

#include "ScriptCoroutine.h"

#include "ScriptCoroutineArgument.h"

namespace PureMirror::Overlay
{
    ScriptCoroutine::ScriptCoroutine(std::string moduleId, asIScriptContext& context, ScriptTask& task)
        : ModuleId(std::move(moduleId)), Context(context), Task(task)
    {
    }

    ScriptCoroutine::~ScriptCoroutine()
    {
        Context.Release();
        Task.Release();
    }

    void ScriptCoroutine::KeepArgument(std::unique_ptr<ScriptCoroutineArgument> argument)
    {
        m_Arguments.push_back(std::move(argument));
    }
}  // namespace PureMirror::Overlay
