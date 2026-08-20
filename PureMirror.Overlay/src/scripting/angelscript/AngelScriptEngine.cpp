#include "pch.h"

#include "AngelScriptEngine.h"

#include "angelscript.h"
#include "scriptstdstring.h"

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
    }  // namespace

    class AngelScriptEngine::Implementation
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
                if (m_CurrentFrame < suspendedCall->ResumeFrame ||
                    std::chrono::steady_clock::now() < suspendedCall->ResumeTime)
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
                              call.Context->Release();
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

            const auto successful =
                require(m_Engine->SetDefaultNamespace("Utils"), "namespace Utils") &&
                require(m_Engine->RegisterGlobalFunction(
                            "void Yield()", asMETHOD(Implementation, HostYield), asCALL_THISCALL_ASGLOBAL, this),
                        "Utils::Yield") &&
                require(m_Engine->RegisterGlobalFunction("void Sleep(uint64 timeInMs)",
                                                         asMETHOD(Implementation, HostSleep),
                                                         asCALL_THISCALL_ASGLOBAL,
                                                         this),
                        "Utils::Sleep") &&
                require(m_Engine->SetDefaultNamespace("log"), "namespace log") &&
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
                        "ui::button");
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

        void HostYield()
        {
            if (!RequireActiveTag(ScriptCallbackTag::Suspendable, "Utils::Yield() is not available in this callback."))
                return;
            auto* context = asGetActiveContext();
            if (context != nullptr)
                static_cast<void>(context->Suspend());
        }

        void HostSleep(const asQWORD timeInMs)
        {
            if (!RequireActiveTag(ScriptCallbackTag::Suspendable, "Utils::Sleep() is not available in this callback."))
                return;

            const auto now = std::chrono::steady_clock::now();
            const auto maximumDelay = std::chrono::duration_cast<std::chrono::milliseconds>(
                (std::chrono::steady_clock::time_point::max)() - now);
            const auto requestedDelay = std::min<asQWORD>(timeInMs, static_cast<asQWORD>(maximumDelay.count()));
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
        std::mutex m_Mutex;
        std::chrono::steady_clock::time_point m_ExecutionDeadline;
        std::chrono::steady_clock::time_point m_RequestedResumeTime;
        std::uint64_t m_CurrentFrame{};
        std::uint64_t m_RequestedResumeFrame{};
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
