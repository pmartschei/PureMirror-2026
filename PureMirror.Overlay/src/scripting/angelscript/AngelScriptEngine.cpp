#include "pch.h"

#include "AngelScriptEngine.h"

#include "IScriptSuspensionRuntime.h"
#include "IScriptTaskRuntime.h"
#include "ScriptContextWait.h"
#include "ScriptCoroutine.h"
#include "ScriptCoroutineArgument.h"
#include "ScriptSuspensionBindings.h"
#include "ScriptTask.h"
#include "ScriptTaskBindings.h"
#include "angelscript.h"
#include "scriptarray.h"
#include "scriptstdstring.h"

#include <limits>
#include <type_traits>

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr auto ScriptExecutionTimeLimit = std::chrono::milliseconds(100);

        struct SuspendedScriptCall
        {
            std::string ModuleId;
            std::string FunctionDeclaration;
            ScriptCallbackTag Tags{ScriptCallbackTag::None};
            asIScriptContext* Context{};
            std::uint64_t ResumeFrame{};
            std::chrono::steady_clock::time_point ResumeTime;
        };

        bool SetCallbackArgument(asIScriptContext& context, const ScriptCallbackArgument& argument)
        {
            if (argument.Index > (std::numeric_limits<asUINT>::max)())
                return false;
            const auto index = static_cast<asUINT>(argument.Index);
            return std::visit(
                [&](const auto value)
                {
                    using ValueType = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::is_same_v<ValueType, bool> || std::is_same_v<ValueType, std::int8_t> ||
                                  std::is_same_v<ValueType, std::uint8_t>)
                        return context.SetArgByte(index, static_cast<asBYTE>(value)) >= 0;
                    else if constexpr (std::is_same_v<ValueType, std::int16_t> ||
                                       std::is_same_v<ValueType, std::uint16_t>)
                        return context.SetArgWord(index, static_cast<asWORD>(value)) >= 0;
                    else if constexpr (std::is_same_v<ValueType, std::int32_t> ||
                                       std::is_same_v<ValueType, std::uint32_t>)
                        return context.SetArgDWord(index, static_cast<asDWORD>(value)) >= 0;
                    else if constexpr (std::is_same_v<ValueType, std::int64_t> ||
                                       std::is_same_v<ValueType, std::uint64_t>)
                        return context.SetArgQWord(index, static_cast<asQWORD>(value)) >= 0;
                    else if constexpr (std::is_same_v<ValueType, float>)
                        return context.SetArgFloat(index, value) >= 0;
                    else
                        return context.SetArgDouble(index, value) >= 0;
                },
                argument.Value);
        }
    }  // namespace

    class AngelScriptEngine::Implementation final : public IScriptTaskRuntime, public IScriptSuspensionRuntime
    {
      public:
        explicit Implementation(IScriptHost* scriptHost) : m_ScriptHost(scriptHost)
        {
            m_Engine = asCreateScriptEngine();
            if (m_Engine == nullptr)
                return;
            if (m_Engine->SetMessageCallback(asFUNCTION(MessageCallback), this, asCALL_CDECL) < 0)
            {
                m_Engine->ShutDownAndRelease();
                m_Engine = nullptr;
                return;
            }
            RegisterStdString(m_Engine);
            RegisterScriptArray(m_Engine, true);
            if (!RegisterHostBindings())
            {
                m_Engine->ShutDownAndRelease();
                m_Engine = nullptr;
            }
        }

        ~Implementation()
        {
            if (m_Engine != nullptr)
            {
                m_ContextWaits.clear();
                m_Coroutines.clear();
                for (const auto& call : m_SuspendedCalls)
                    call.Context->Release();
                m_Engine->ShutDownAndRelease();
            }
        }

        bool IsInitialized() const noexcept
        {
            return m_Engine != nullptr;
        }

        ScriptModuleLoadResult LoadModule(const std::string_view moduleId, const std::vector<ScriptSource>& sources)
        {
            std::scoped_lock lock(m_Mutex);
            m_Diagnostics.clear();
            ScriptModuleLoadResult result{.ModuleId = std::string(moduleId)};
            if (m_Engine == nullptr)
            {
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Message = m_InitializationError.empty()
                                                             ? "AngelScript engine initialization failed."
                                                             : m_InitializationError});
                return result;
            }
            if (moduleId.empty() || sources.empty())
            {
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Message = "A module id and at least one script source are required."});
                return result;
            }

            const std::string moduleName(moduleId);
            auto* module = m_Engine->GetModule(moduleName.c_str(), asGM_ALWAYS_CREATE);
            if (module == nullptr)
            {
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Message = "AngelScript module could not be created."});
                return result;
            }
            for (const auto& source : sources)
            {
                if (module->AddScriptSection(source.Name.c_str(), source.Code.data(), source.Code.size()) < 0)
                {
                    result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                                  .Section = source.Name,
                                                  .Message = "Script section could not be added."});
                    m_Engine->DiscardModule(moduleName.c_str());
                    return result;
                }
            }

            result.IsLoaded = module->Build() >= 0;
            result.Diagnostics = m_Diagnostics;
            if (!result.IsLoaded)
                m_Engine->DiscardModule(moduleName.c_str());
            return result;
        }

        void AdvanceFrame()
        {
            std::scoped_lock lock(m_Mutex);
            ++m_CurrentFrame;
            const auto coroutineCount = m_Coroutines.size();
            for (std::size_t index{}; index < coroutineCount; ++index)
                RunCoroutine(*m_Coroutines[index]);
            std::erase_if(m_Coroutines, [](const auto& coroutine) { return coroutine->IsFinished; });
        }

        ScriptCallResult CallFunction(const std::string_view moduleId, const ScriptCallback& callback)
        {
            std::scoped_lock lock(m_Mutex);
            ScriptCallResult result{.ModuleId = std::string(moduleId),
                                    .FunctionDeclaration = std::string(callback.FunctionDeclaration)};
            if (m_Engine == nullptr)
            {
                result.Status = ScriptCallStatus::Failed;
                result.Diagnostics.push_back(
                    {.Severity = ScriptDiagnosticSeverity::Error, .Message = "AngelScript engine is not initialized."});
                return result;
            }

            const std::string moduleName(moduleId);
            const std::string declaration(callback.FunctionDeclaration);
            const auto suspendedCall =
                std::ranges::find_if(m_SuspendedCalls,
                                     [&](const SuspendedScriptCall& call) {
                                         return call.ModuleId == moduleName && call.FunctionDeclaration == declaration;
                                     });
            if (suspendedCall != m_SuspendedCalls.end())
            {
                if (!CanResumeContext(*suspendedCall->Context, suspendedCall->ResumeFrame, suspendedCall->ResumeTime))
                {
                    result.Status = ScriptCallStatus::Suspended;
                    return result;
                }

                if (suspendedCall->Tags != callback.Tags)
                {
                    result.Status = ScriptCallStatus::Failed;
                    result.Diagnostics.push_back(
                        {.Severity = ScriptDiagnosticSeverity::Error,
                         .Message = "A suspended callback cannot be resumed with different capability tags."});
                    return result;
                }

                result = ExecuteContext(moduleId, callback, suspendedCall->Context);
                if (result.Status == ScriptCallStatus::Suspended)
                {
                    suspendedCall->ResumeFrame = m_RequestedResumeFrame;
                    suspendedCall->ResumeTime = m_RequestedResumeTime;
                    return result;
                }

                suspendedCall->Context->Release();
                m_ContextWaits.erase(suspendedCall->Context);
                m_SuspendedCalls.erase(suspendedCall);
                return result;
            }

            auto* module = m_Engine->GetModule(moduleName.c_str(), asGM_ONLY_IF_EXISTS);
            auto* function = module != nullptr ? module->GetFunctionByDecl(declaration.c_str()) : nullptr;
            if (function == nullptr)
                return result;

            auto* context = m_Engine->CreateContext();
            if (context == nullptr || context->Prepare(function) < 0)
            {
                if (context != nullptr)
                    context->Release();
                result.Status = ScriptCallStatus::Failed;
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Message = "AngelScript context could not prepare the callback."});
                return result;
            }

            for (const auto& argument : callback.Arguments)
            {
                if (SetCallbackArgument(*context, argument))
                    continue;
                context->Release();
                result.Status = ScriptCallStatus::Failed;
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Message = "AngelScript callback argument at index " +
                                                         std::to_string(argument.Index) + " could not be set."});
                return result;
            }

            result = ExecuteContext(moduleId, callback, context);
            if (result.Status == ScriptCallStatus::Suspended)
            {
                m_SuspendedCalls.push_back({.ModuleId = moduleName,
                                            .FunctionDeclaration = declaration,
                                            .Tags = callback.Tags,
                                            .Context = context,
                                            .ResumeFrame = m_RequestedResumeFrame,
                                            .ResumeTime = m_RequestedResumeTime});
            }
            else
            {
                context->Release();
            }
            return result;
        }

        ScriptModuleLoadResult BindModuleImports(const std::string_view moduleId)
        {
            std::scoped_lock lock(m_Mutex);
            m_Diagnostics.clear();
            ScriptModuleLoadResult result{.ModuleId = std::string(moduleId)};
            if (m_Engine == nullptr)
            {
                result.Diagnostics.push_back(
                    {.Severity = ScriptDiagnosticSeverity::Error, .Message = "AngelScript engine is not initialized."});
                return result;
            }

            const std::string moduleName(moduleId);
            auto* module = m_Engine->GetModule(moduleName.c_str(), asGM_ONLY_IF_EXISTS);
            if (module == nullptr)
            {
                result.Diagnostics.push_back(
                    {.Severity = ScriptDiagnosticSeverity::Error, .Message = "AngelScript module does not exist."});
                return result;
            }

            result.IsLoaded = module->BindAllImportedFunctions() >= 0;
            result.Diagnostics = m_Diagnostics;
            if (!result.IsLoaded && result.Diagnostics.empty())
            {
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Message = "Not all imported functions could be bound."});
            }
            return result;
        }

        void UnloadModule(const std::string_view moduleId)
        {
            std::scoped_lock lock(m_Mutex);
            if (m_Engine == nullptr || moduleId.empty())
                return;
            const std::string moduleName(moduleId);
            std::erase_if(m_SuspendedCalls,
                          [&](const SuspendedScriptCall& call)
                          {
                              if (call.ModuleId != moduleName)
                                  return false;
                              m_ContextWaits.erase(call.Context);
                              call.Context->Release();
                              return true;
                          });
            std::erase_if(m_Coroutines,
                          [&](const auto& coroutine)
                          {
                              if (coroutine->ModuleId != moduleName)
                                  return false;
                              if (!coroutine->Task.IsCompleted())
                                  coroutine->Task.Fail("The owning plugin was unloaded.", ++m_CompletionOrder);
                              m_ContextWaits.erase(&coroutine->Context);
                              return true;
                          });
            m_Engine->DiscardModule(moduleName.c_str());
        }

      private:
        ScriptCallResult ExecuteContext(const std::string_view moduleId,
                                        const ScriptCallback& callback,
                                        asIScriptContext* context)
        {
            ScriptCallResult result{.ModuleId = std::string(moduleId),
                                    .FunctionDeclaration = std::string(callback.FunctionDeclaration)};
            m_DidExecutionTimeOut = false;
            m_ActiveCallbackTags = callback.Tags;
            m_RequestedResumeFrame = m_CurrentFrame + 1;
            m_RequestedResumeTime = std::chrono::steady_clock::now();
            m_ExecutionDeadline = std::chrono::steady_clock::now() + ScriptExecutionTimeLimit;
            if (context->SetLineCallback(asMETHOD(Implementation, EnforceExecutionDeadline), this, asCALL_THISCALL) < 0)
            {
                result.Status = ScriptCallStatus::Failed;
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Message = "AngelScript execution deadline could not be installed."});
                return result;
            }

            if (m_ScriptHost != nullptr)
                m_ScriptHost->BeginScriptCall(moduleId);
            const auto executionStartedAt = std::chrono::steady_clock::now();
            const auto execution = context->Execute();
            const auto executionDuration = std::chrono::steady_clock::now() - executionStartedAt;
            if (m_ScriptHost != nullptr)
                m_ScriptHost->EndScriptCall(moduleId);
            context->ClearLineCallback();
            m_ActiveCallbackTags = ScriptCallbackTag::None;

            if (m_DidExecutionTimeOut || executionDuration > ScriptExecutionTimeLimit)
            {
                result.Status = ScriptCallStatus::Failed;
                result.Diagnostics.push_back({.Severity = ScriptDiagnosticSeverity::Error,
                                              .Message = "Script execution exceeded the 100 ms time limit."});
            }
            else if (execution == asEXECUTION_FINISHED)
            {
                result.Status = ScriptCallStatus::Executed;
            }
            else if (execution == asEXECUTION_SUSPENDED)
            {
                result.Status = ScriptCallStatus::Suspended;
            }
            else
            {
                result.Status = ScriptCallStatus::Failed;
                int column{};
                const char* section{};
                const auto row = context->GetExceptionLineNumber(&column, &section);
                const auto* exception = context->GetExceptionString();
                result.Diagnostics.push_back(
                    {.Severity = ScriptDiagnosticSeverity::Error,
                     .Section = section != nullptr ? section : "",
                     .Row = row > 0 ? static_cast<std::size_t>(row) : 0,
                     .Column = column > 0 ? static_cast<std::size_t>(column) : 0,
                     .Message = exception != nullptr ? exception : "AngelScript callback failed."});
            }
            return result;
        }

        bool CanResumeContext(asIScriptContext& context,
                              const std::uint64_t resumeFrame,
                              const std::chrono::steady_clock::time_point resumeTime)
        {
            if (m_CurrentFrame < resumeFrame || std::chrono::steady_clock::now() < resumeTime)
                return false;
            const auto wait = m_ContextWaits.find(&context);
            if (wait == m_ContextWaits.end())
                return true;
            if (!wait->second->IsReady())
                return false;
            wait->second->PrepareResume();
            m_ContextWaits.erase(wait);
            return true;
        }

        void RunCoroutine(ScriptCoroutine& coroutine)
        {
            if (coroutine.IsFinished ||
                !CanResumeContext(coroutine.Context, coroutine.ResumeFrame, coroutine.ResumeTime))
                return;

            const auto previousTags = m_ActiveCallbackTags;
            const auto previousDeadline = m_ExecutionDeadline;
            const auto previousResumeFrame = m_RequestedResumeFrame;
            const auto previousResumeTime = m_RequestedResumeTime;
            const auto previousTimeout = m_DidExecutionTimeOut;

            m_ActiveCallbackTags = coroutine.CallbackTags;
            m_RequestedResumeFrame = m_CurrentFrame + 1;
            m_RequestedResumeTime = std::chrono::steady_clock::now();
            m_DidExecutionTimeOut = false;
            m_ExecutionDeadline = std::chrono::steady_clock::now() + ScriptExecutionTimeLimit;

            int execution{asEXECUTION_ERROR};
            if (coroutine.Context.SetLineCallback(
                    asMETHOD(Implementation, EnforceExecutionDeadline), this, asCALL_THISCALL) >= 0)
            {
                if (m_ScriptHost != nullptr)
                    m_ScriptHost->BeginScriptCall(coroutine.ModuleId);
                const auto executionStartedAt = std::chrono::steady_clock::now();
                execution = coroutine.Context.Execute();
                const auto executionDuration = std::chrono::steady_clock::now() - executionStartedAt;
                if (m_ScriptHost != nullptr)
                    m_ScriptHost->EndScriptCall(coroutine.ModuleId);
                coroutine.Context.ClearLineCallback();
                if (executionDuration > ScriptExecutionTimeLimit)
                    m_DidExecutionTimeOut = true;
            }

            if (m_DidExecutionTimeOut)
            {
                coroutine.Task.Fail("Coroutine execution exceeded the 100 ms time limit.", ++m_CompletionOrder);
                coroutine.IsFinished = true;
            }
            else if (execution == asEXECUTION_FINISHED)
            {
                coroutine.Task.Complete(coroutine.Context, ++m_CompletionOrder);
                coroutine.IsFinished = true;
            }
            else if (execution == asEXECUTION_SUSPENDED)
            {
                coroutine.ResumeFrame = m_RequestedResumeFrame;
                coroutine.ResumeTime = m_RequestedResumeTime;
            }
            else
            {
                const auto* exception = coroutine.Context.GetExceptionString();
                coroutine.Task.Fail(exception != nullptr ? exception : "Coroutine execution failed.",
                                    ++m_CompletionOrder);
                coroutine.IsFinished = true;
            }

            if (coroutine.IsFinished)
                m_ContextWaits.erase(&coroutine.Context);
            m_ActiveCallbackTags = previousTags;
            m_ExecutionDeadline = previousDeadline;
            m_RequestedResumeFrame = previousResumeFrame;
            m_RequestedResumeTime = previousResumeTime;
            m_DidExecutionTimeOut = previousTimeout;
        }

        bool SetCoroutineArgument(ScriptCoroutine& coroutine,
                                  asIScriptFunction& function,
                                  const asUINT parameterIndex,
                                  void* argument,
                                  const int argumentTypeId)
        {
            int parameterTypeId{};
            asDWORD parameterFlags{};
            if (function.GetParam(parameterIndex, &parameterTypeId, &parameterFlags) < 0)
                return false;
            const auto comparableParameterTypeId = parameterTypeId & ~asTYPEID_HANDLETOCONST;
            const auto comparableArgumentTypeId = argumentTypeId & ~asTYPEID_HANDLETOCONST;
            if (comparableParameterTypeId != comparableArgumentTypeId)
                return false;

            const auto referenceMode = parameterFlags & asTM_INOUTREF;
            if (referenceMode == asTM_OUTREF || referenceMode == asTM_INOUTREF)
                return false;
            auto& context = coroutine.Context;
            if (referenceMode == asTM_INREF)
            {
                auto ownedArgument = std::make_unique<ScriptCoroutineArgument>(*m_Engine, argument, argumentTypeId);
                const auto result = (parameterTypeId & asTYPEID_MASK_OBJECT) != 0
                                        ? context.SetArgObject(parameterIndex, ownedArgument->Value())
                                        : context.SetArgAddress(parameterIndex, ownedArgument->Value());
                if (result < 0)
                    return false;
                coroutine.KeepArgument(std::move(ownedArgument));
                return true;
            }
            if ((parameterTypeId & asTYPEID_MASK_OBJECT) != 0)
            {
                auto* object = (argumentTypeId & asTYPEID_OBJHANDLE) != 0 ? *static_cast<void**>(argument) : argument;
                return context.SetArgObject(parameterIndex, object) >= 0;
            }
            switch (parameterTypeId)
            {
            case asTYPEID_BOOL:
            case asTYPEID_INT8:
            case asTYPEID_UINT8:
                return context.SetArgByte(parameterIndex, *static_cast<asBYTE*>(argument)) >= 0;
            case asTYPEID_INT16:
            case asTYPEID_UINT16:
                return context.SetArgWord(parameterIndex, *static_cast<asWORD*>(argument)) >= 0;
            case asTYPEID_INT32:
            case asTYPEID_UINT32:
                return context.SetArgDWord(parameterIndex, *static_cast<asDWORD*>(argument)) >= 0;
            case asTYPEID_FLOAT:
                return context.SetArgFloat(parameterIndex, *static_cast<float*>(argument)) >= 0;
            case asTYPEID_DOUBLE:
                return context.SetArgDouble(parameterIndex, *static_cast<double*>(argument)) >= 0;
            default:
                return context.SetArgQWord(parameterIndex, *static_cast<asQWORD*>(argument)) >= 0;
            }
        }

        void HostAsync(asIScriptGeneric& generic) override
        {
            if (!RequireActiveTag(ScriptCallbackTag::Coroutine, "Async() is not available in this callback."))
                return;

            auto* activeContext = asGetActiveContext();
            auto* functionReference = static_cast<asIScriptFunction**>(generic.GetArgAddress(0));
            auto* function = functionReference != nullptr ? *functionReference : nullptr;
            if (activeContext == nullptr || function == nullptr ||
                function->GetParamCount() + 1 != static_cast<asUINT>(generic.GetArgCount()))
            {
                if (activeContext != nullptr)
                    static_cast<void>(
                        activeContext->SetException("Async() requires a function and its exact arguments."));
                return;
            }

            auto* context = m_Engine->CreateContext();
            if (context == nullptr || context->Prepare(function) < 0)
            {
                if (context != nullptr)
                    context->Release();
                static_cast<void>(activeContext->SetException("Async() could not prepare the coroutine."));
                return;
            }
            auto* task = new ScriptTask(*m_Engine, function->GetReturnTypeId());
            const auto moduleId = std::string(ActivePluginId());
            auto coroutine = std::make_unique<ScriptCoroutine>(
                moduleId, *context, *task, m_ActiveCallbackTags | ScriptCallbackTag::Suspendable);
            for (asUINT index{}; index < function->GetParamCount(); ++index)
            {
                if (SetCoroutineArgument(*coroutine,
                                         *function,
                                         index,
                                         generic.GetArgAddress(index + 1),
                                         generic.GetArgTypeId(index + 1)))
                    continue;
                static_cast<void>(activeContext->SetException("Async() argument types do not match the function."));
                return;
            }

            RunCoroutine(*coroutine);
            static_cast<void>(generic.SetReturnObject(task));
            if (!coroutine->IsFinished)
                m_Coroutines.push_back(std::move(coroutine));
        }

        void HostWait(ScriptTask* task) override
        {
            if (!RequireActiveTag(ScriptCallbackTag::Suspendable, "Wait() is not available in this callback."))
                return;

            auto* context = asGetActiveContext();
            if (context == nullptr || task == nullptr)
            {
                if (context != nullptr)
                    static_cast<void>(context->SetException("Wait() requires a task."));
                return;
            }
            if (task->IsCompleted())
                return;
            m_ContextWaits[context] =
                std::make_unique<ScriptContextWait>(ScriptWaitMode::One, std::vector<ScriptTask*>{task});
            static_cast<void>(context->Suspend());
        }

        void HostWaitAll(CScriptArray* tasks) override
        {
            if (!RequireActiveTag(ScriptCallbackTag::Suspendable, "WaitAll() is not available in this callback."))
                return;

            auto* context = asGetActiveContext();
            auto taskList = TasksFromArray(tasks);
            if (context == nullptr || tasks == nullptr || taskList.size() != tasks->GetSize())
            {
                if (context != nullptr)
                    static_cast<void>(context->SetException("WaitAll() does not accept null tasks."));
                return;
            }
            if (std::ranges::all_of(taskList, &ScriptTask::IsCompleted))
                return;
            m_ContextWaits[context] = std::make_unique<ScriptContextWait>(ScriptWaitMode::All, std::move(taskList));
            static_cast<void>(context->Suspend());
        }

        void HostWaitAny(asIScriptGeneric& generic) override
        {
            if (!RequireActiveTag(ScriptCallbackTag::Suspendable, "WaitAny() is not available in this callback."))
                return;

            auto* context = asGetActiveContext();
            auto* tasks = static_cast<CScriptArray*>(generic.GetArgObject(0));
            auto taskList = TasksFromArray(tasks);
            if (context == nullptr || tasks == nullptr || taskList.empty() || taskList.size() != tasks->GetSize())
            {
                if (context != nullptr)
                    static_cast<void>(context->SetException("WaitAny() requires at least one non-null task."));
                return;
            }

            auto* firstCompleted = FirstCompletedTask(taskList);
            if (firstCompleted != nullptr)
            {
                static_cast<void>(generic.SetReturnObject(firstCompleted));
                return;
            }

            auto* resultTask = new ScriptTask(*m_Engine, asTYPEID_VOID);
            static_cast<void>(generic.SetReturnObject(resultTask));
            m_ContextWaits[context] =
                std::make_unique<ScriptContextWait>(ScriptWaitMode::Any, std::move(taskList), resultTask);
            resultTask->Release();
            static_cast<void>(context->Suspend());
        }

        static std::vector<ScriptTask*> TasksFromArray(CScriptArray* tasks)
        {
            std::vector<ScriptTask*> result;
            if (tasks == nullptr)
                return result;
            result.reserve(tasks->GetSize());
            for (asUINT index{}; index < tasks->GetSize(); ++index)
            {
                auto* task = *static_cast<ScriptTask**>(tasks->At(index));
                if (task != nullptr)
                    result.push_back(task);
            }
            return result;
        }

        static ScriptTask* FirstCompletedTask(const std::vector<ScriptTask*>& tasks)
        {
            ScriptTask* result{};
            for (const auto task : tasks)
            {
                if (!task->IsCompleted() || (result != nullptr && result->CompletionOrder() <= task->CompletionOrder()))
                    continue;
                result = task;
            }
            return result;
        }

        void EnforceExecutionDeadline(asIScriptContext* context)
        {
            if (context == nullptr || std::chrono::steady_clock::now() <= m_ExecutionDeadline)
                return;
            m_DidExecutionTimeOut = true;
            static_cast<void>(context->Abort());
        }

        bool RegisterHostBindings()
        {
            const auto require = [&](const int code, const std::string_view operation)
            {
                if (code >= 0)
                    return true;
                m_InitializationError = "AngelScript host binding failed at " + std::string(operation) + " with code " +
                                        std::to_string(code) + '.';
                return false;
            };

            auto successful = RegisterScriptTaskBindings(*m_Engine, *this, m_InitializationError);
            if (successful)
                successful = RegisterScriptSuspensionBindings(*m_Engine, *this, m_InitializationError);

            successful =
                successful && require(m_Engine->SetDefaultNamespace("log"), "namespace log") &&
                require(m_Engine->RegisterGlobalFunction("void info(const string &in)",
                                                         asMETHOD(Implementation, HostLogInfo),
                                                         asCALL_THISCALL_ASGLOBAL,
                                                         this),
                        "log::info") &&
                require(m_Engine->SetDefaultNamespace("ui"), "namespace ui") &&
                require(m_Engine->RegisterGlobalFunction("bool begin_window(const string &in)",
                                                         asMETHOD(Implementation, HostBeginWindow),
                                                         asCALL_THISCALL_ASGLOBAL,
                                                         this),
                        "ui::begin_window") &&
                require(
                    m_Engine->RegisterGlobalFunction(
                        "void end_window()", asMETHOD(Implementation, HostEndWindow), asCALL_THISCALL_ASGLOBAL, this),
                    "ui::end_window") &&
                require(m_Engine->RegisterGlobalFunction("void text(const string &in)",
                                                         asMETHOD(Implementation, HostText),
                                                         asCALL_THISCALL_ASGLOBAL,
                                                         this),
                        "ui::text") &&
                require(m_Engine->RegisterGlobalFunction("bool button(const string &in)",
                                                         asMETHOD(Implementation, HostButton),
                                                         asCALL_THISCALL_ASGLOBAL,
                                                         this),
                        "ui::button") &&
                require(m_Engine->RegisterGlobalFunction("bool begin_menu(const string &in)",
                                                         asMETHOD(Implementation, HostBeginMenu),
                                                         asCALL_THISCALL_ASGLOBAL,
                                                         this),
                        "ui::begin_menu") &&
                require(m_Engine->RegisterGlobalFunction(
                            "void end_menu()", asMETHOD(Implementation, HostEndMenu), asCALL_THISCALL_ASGLOBAL, this),
                        "ui::end_menu") &&
                require(m_Engine->RegisterGlobalFunction("bool menu_item(const string &in)",
                                                         asMETHOD(Implementation, HostMenuItem),
                                                         asCALL_THISCALL_ASGLOBAL,
                                                         this),
                        "ui::menu_item") &&
                require(m_Engine->RegisterGlobalFunction("void menu_separator()",
                                                         asMETHOD(Implementation, HostMenuSeparator),
                                                         asCALL_THISCALL_ASGLOBAL,
                                                         this),
                        "ui::menu_separator");
            const auto reset = require(m_Engine->SetDefaultNamespace(""), "default namespace");
            return successful && reset;
        }

        static std::string_view ActivePluginId()
        {
            auto* context = asGetActiveContext();
            auto* function = context != nullptr ? context->GetFunction() : nullptr;
            const auto* moduleName = function != nullptr ? function->GetModuleName() : nullptr;
            return moduleName != nullptr ? std::string_view(moduleName) : std::string_view{};
        }

        void HostLogInfo(const std::string& message)
        {
            if (m_ScriptHost != nullptr)
                m_ScriptHost->LogInfo(ActivePluginId(), message);
        }

        bool HostBeginWindow(const std::string& title)
        {
            if (!RequireActiveTag(ScriptCallbackTag::Ui, "UI functions are not available in this callback."))
                return false;
            return m_ScriptHost != nullptr && m_ScriptHost->BeginWindow(ActivePluginId(), title);
        }

        void HostEndWindow()
        {
            if (!RequireActiveTag(ScriptCallbackTag::Ui, "UI functions are not available in this callback."))
                return;
            if (m_ScriptHost != nullptr)
                m_ScriptHost->EndWindow(ActivePluginId());
        }

        void HostText(const std::string& value)
        {
            if (!RequireActiveTag(ScriptCallbackTag::Ui, "UI functions are not available in this callback."))
                return;
            if (m_ScriptHost != nullptr)
                m_ScriptHost->Text(ActivePluginId(), value);
        }

        bool HostButton(const std::string& label)
        {
            if (!RequireActiveTag(ScriptCallbackTag::Ui, "UI functions are not available in this callback."))
                return false;
            return m_ScriptHost != nullptr && m_ScriptHost->Button(ActivePluginId(), label);
        }

        bool HostBeginMenu(const std::string& label)
        {
            if (!RequireActiveTag(ScriptCallbackTag::MenuUi, "Menu UI functions are not available in this callback."))
                return false;
            return m_ScriptHost != nullptr && m_ScriptHost->BeginMenu(ActivePluginId(), label);
        }

        void HostEndMenu()
        {
            if (!RequireActiveTag(ScriptCallbackTag::MenuUi, "Menu UI functions are not available in this callback."))
                return;
            if (m_ScriptHost != nullptr)
                m_ScriptHost->EndMenu(ActivePluginId());
        }

        bool HostMenuItem(const std::string& label)
        {
            if (!RequireActiveTag(ScriptCallbackTag::MenuUi, "Menu UI functions are not available in this callback."))
                return false;
            return m_ScriptHost != nullptr && m_ScriptHost->MenuItem(ActivePluginId(), label);
        }

        void HostMenuSeparator()
        {
            if (!RequireActiveTag(ScriptCallbackTag::MenuUi, "Menu UI functions are not available in this callback."))
                return;
            if (m_ScriptHost != nullptr)
                m_ScriptHost->MenuSeparator(ActivePluginId());
        }

        void HostYield() override
        {
            if (!RequireActiveTag(ScriptCallbackTag::Suspendable, "Utils::Yield() is not available in this callback."))
                return;
            auto* context = asGetActiveContext();
            if (context != nullptr)
                static_cast<void>(context->Suspend());
        }

        void HostSleep(const std::uint64_t timeInMs) override
        {
            if (!RequireActiveTag(ScriptCallbackTag::Suspendable, "Utils::Sleep() is not available in this callback."))
                return;

            const auto now = std::chrono::steady_clock::now();
            const auto maximumDelay = std::chrono::duration_cast<std::chrono::milliseconds>(
                (std::chrono::steady_clock::time_point::max)() - now);
            const auto requestedDelay =
                std::min<std::uint64_t>(timeInMs, static_cast<std::uint64_t>(maximumDelay.count()));
            m_RequestedResumeTime =
                now + std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(requestedDelay));
            auto* context = asGetActiveContext();
            if (context != nullptr)
                static_cast<void>(context->Suspend());
        }

        bool RequireActiveTag(const ScriptCallbackTag tag, const char* message) const
        {
            if (HasTag(m_ActiveCallbackTags, tag))
                return true;
            auto* context = asGetActiveContext();
            if (context != nullptr)
                static_cast<void>(context->SetException(message));
            return false;
        }

        static void MessageCallback(const asSMessageInfo* message, void* parameter)
        {
            if (message == nullptr || parameter == nullptr)
                return;
            auto& implementation = *static_cast<Implementation*>(parameter);
            ScriptDiagnosticSeverity severity{ScriptDiagnosticSeverity::Information};
            if (message->type == asMSGTYPE_WARNING)
                severity = ScriptDiagnosticSeverity::Warning;
            else if (message->type == asMSGTYPE_ERROR)
                severity = ScriptDiagnosticSeverity::Error;
            implementation.m_Diagnostics.push_back(
                {.Severity = severity,
                 .Section = message->section != nullptr ? message->section : "",
                 .Row = message->row > 0 ? static_cast<std::size_t>(message->row) : 0,
                 .Column = message->col > 0 ? static_cast<std::size_t>(message->col) : 0,
                 .Message = message->message != nullptr ? message->message : ""});
        }

        asIScriptEngine* m_Engine{};
        IScriptHost* m_ScriptHost{};
        std::string m_InitializationError;
        std::vector<ScriptDiagnostic> m_Diagnostics;
        std::vector<SuspendedScriptCall> m_SuspendedCalls;
        std::vector<std::unique_ptr<ScriptCoroutine>> m_Coroutines;
        std::unordered_map<asIScriptContext*, std::unique_ptr<ScriptContextWait>> m_ContextWaits;
        std::mutex m_Mutex;
        std::chrono::steady_clock::time_point m_ExecutionDeadline;
        std::chrono::steady_clock::time_point m_RequestedResumeTime;
        std::uint64_t m_CurrentFrame{};
        std::uint64_t m_RequestedResumeFrame{};
        std::uint64_t m_CompletionOrder{};
        ScriptCallbackTag m_ActiveCallbackTags{ScriptCallbackTag::None};
        bool m_DidExecutionTimeOut{};
    };

    AngelScriptEngine::AngelScriptEngine(IScriptHost* scriptHost)
        : m_Implementation(std::make_unique<Implementation>(scriptHost))
    {
    }

    AngelScriptEngine::~AngelScriptEngine() = default;
    AngelScriptEngine::AngelScriptEngine(AngelScriptEngine&&) noexcept = default;
    AngelScriptEngine& AngelScriptEngine::operator=(AngelScriptEngine&&) noexcept = default;

    bool AngelScriptEngine::IsInitialized() const noexcept
    {
        return m_Implementation != nullptr && m_Implementation->IsInitialized();
    }

    ScriptModuleLoadResult AngelScriptEngine::LoadModule(const std::string_view moduleId,
                                                         const std::vector<ScriptSource>& sources)
    {
        if (m_Implementation == nullptr)
            return {.ModuleId = std::string(moduleId),
                    .Diagnostics = {{.Severity = ScriptDiagnosticSeverity::Error,
                                     .Message = "AngelScript engine is not available."}}};
        return m_Implementation->LoadModule(moduleId, sources);
    }

    void AngelScriptEngine::AdvanceFrame()
    {
        if (m_Implementation != nullptr)
            m_Implementation->AdvanceFrame();
    }

    ScriptCallResult AngelScriptEngine::CallFunction(const std::string_view moduleId, const ScriptCallback& callback)
    {
        if (m_Implementation == nullptr)
            return {.Status = ScriptCallStatus::Failed,
                    .ModuleId = std::string(moduleId),
                    .FunctionDeclaration = std::string(callback.FunctionDeclaration),
                    .Diagnostics = {{.Severity = ScriptDiagnosticSeverity::Error,
                                     .Message = "AngelScript engine is not available."}}};
        return m_Implementation->CallFunction(moduleId, callback);
    }

    ScriptModuleLoadResult AngelScriptEngine::BindModuleImports(const std::string_view moduleId)
    {
        if (m_Implementation == nullptr)
            return {.ModuleId = std::string(moduleId),
                    .Diagnostics = {{.Severity = ScriptDiagnosticSeverity::Error,
                                     .Message = "AngelScript engine is not available."}}};
        return m_Implementation->BindModuleImports(moduleId);
    }

    void AngelScriptEngine::UnloadModule(const std::string_view moduleId)
    {
        if (m_Implementation != nullptr)
            m_Implementation->UnloadModule(moduleId);
    }
}  // namespace PureMirror::Overlay
