#include "pch.h"

#include "AngelScriptEngine.h"

#include "angelscript.h"
#include "scriptstdstring.h"

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr auto ScriptExecutionTimeLimit = std::chrono::milliseconds(100);
    }

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
                m_Engine->ShutDownAndRelease();
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

        ScriptCallResult CallFunction(const std::string_view moduleId, const std::string_view functionDeclaration)
        {
            std::scoped_lock lock(m_Mutex);
            ScriptCallResult result{.ModuleId = std::string(moduleId),
                                    .FunctionDeclaration = std::string(functionDeclaration)};
            if (m_Engine == nullptr)
            {
                result.Status = ScriptCallStatus::Failed;
                result.Diagnostics.push_back(
                    {.Severity = ScriptDiagnosticSeverity::Error, .Message = "AngelScript engine is not initialized."});
                return result;
            }

            const std::string moduleName(moduleId);
            const std::string declaration(functionDeclaration);
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

            m_DidExecutionTimeOut = false;
            m_ExecutionDeadline = std::chrono::steady_clock::now() + ScriptExecutionTimeLimit;
            if (context->SetLineCallback(asMETHOD(Implementation, EnforceExecutionDeadline), this, asCALL_THISCALL) < 0)
            {
                context->Release();
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
            context->Release();
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
            m_Engine->DiscardModule(moduleName.c_str());
        }

      private:
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

            const auto successful = require(m_Engine->SetDefaultNamespace("log"), "namespace log") &&
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
                                    require(m_Engine->RegisterGlobalFunction("void end_window()",
                                                                             asMETHOD(Implementation, HostEndWindow),
                                                                             asCALL_THISCALL_ASGLOBAL,
                                                                             this),
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
            return m_ScriptHost != nullptr && m_ScriptHost->BeginWindow(ActivePluginId(), title);
        }

        void HostEndWindow()
        {
            if (m_ScriptHost != nullptr)
                m_ScriptHost->EndWindow(ActivePluginId());
        }

        void HostText(const std::string& value)
        {
            if (m_ScriptHost != nullptr)
                m_ScriptHost->Text(ActivePluginId(), value);
        }

        bool HostButton(const std::string& label)
        {
            return m_ScriptHost != nullptr && m_ScriptHost->Button(ActivePluginId(), label);
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
        std::mutex m_Mutex;
        std::chrono::steady_clock::time_point m_ExecutionDeadline;
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

    ScriptCallResult AngelScriptEngine::CallFunction(const std::string_view moduleId,
                                                     const std::string_view functionDeclaration)
    {
        if (m_Implementation == nullptr)
            return {.Status = ScriptCallStatus::Failed,
                    .ModuleId = std::string(moduleId),
                    .FunctionDeclaration = std::string(functionDeclaration),
                    .Diagnostics = {{.Severity = ScriptDiagnosticSeverity::Error,
                                     .Message = "AngelScript engine is not available."}}};
        return m_Implementation->CallFunction(moduleId, functionDeclaration);
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
