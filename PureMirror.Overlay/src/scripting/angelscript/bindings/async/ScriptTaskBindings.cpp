#include "pch.h"

#include "scripting/angelscript/bindings/async/ScriptTaskBindings.h"

#include "angelscript.h"
#include "scriptarray.h"
#include "scripting/angelscript/IScriptTaskRuntime.h"
#include "scripting/angelscript/bindings/ScriptBindingUtils.h"
#include "scripting/angelscript/bindings/async/ScriptTask.h"

#include <format>

namespace PureMirror::Overlay
{
    namespace
    {
        constexpr std::string_view CoreNamespace = "Core";
        constexpr std::string_view TaskName = "Task";
        constexpr std::string_view TypedTaskName = "TypedTask";

        void TaskRetrieveGeneric(asIScriptGeneric* generic)
        {
            auto* task = static_cast<ScriptTask*>(generic->GetObject());
            if (task != nullptr && task->Retrieve(generic->GetArgAddress(0), generic->GetArgTypeId(0)))
                return;
            auto* context = asGetActiveContext();
            if (context != nullptr)
            {
                const auto message = task != nullptr && task->IsFailed()
                                         ? std::string(task->Error())
                                         : "Task result is not available or has a different type.";
                static_cast<void>(context->SetException(message.c_str()));
            }
        }

        void TypedTaskRetrieveGeneric(asIScriptGeneric* generic)
        {
            auto* task = static_cast<ScriptTask*>(generic->GetObject());
            if (task != nullptr && task->Retrieve(generic->GetArgAddress(0), generic->GetArgTypeId(0)))
                return;
            auto* context = asGetActiveContext();
            if (context != nullptr)
            {
                const auto message = task != nullptr && task->IsFailed() ? std::string(task->Error())
                                                                         : "Typed task result is not available.";
                static_cast<void>(context->SetException(message.c_str()));
            }
        }

        void TaskCastGeneric(asIScriptGeneric* generic)
        {
            auto* task = static_cast<ScriptTask*>(generic->GetObject());
            if (task != nullptr)
                static_cast<void>(task->Cast(generic->GetArgAddress(0), generic->GetArgTypeId(0)));
        }

        void TypedTaskToTaskGeneric(asIScriptGeneric* generic)
        {
            static_cast<void>(generic->SetReturnObject(generic->GetObject()));
        }

        bool TypedTaskTemplateCallback(asITypeInfo* type, bool& dontGarbageCollect)
        {
            dontGarbageCollect = true;
            return type != nullptr && type->GetSubTypeId() != asTYPEID_VOID;
        }

        IScriptTaskRuntime* TaskRuntime(asIScriptGeneric* generic)
        {
            return generic != nullptr ? static_cast<IScriptTaskRuntime*>(generic->GetAuxiliary()) : nullptr;
        }

        void AsyncGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = TaskRuntime(generic); runtime != nullptr)
                runtime->HostAsync(*generic);
        }

        void WaitGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = TaskRuntime(generic); runtime != nullptr)
                runtime->HostWait(static_cast<ScriptTask*>(generic->GetArgObject(0)));
        }

        void WaitAllGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = TaskRuntime(generic); runtime != nullptr)
                runtime->HostWaitAll(static_cast<CScriptArray*>(generic->GetArgObject(0)));
        }

        void WaitAnyGeneric(asIScriptGeneric* generic)
        {
            if (auto* runtime = TaskRuntime(generic); runtime != nullptr)
                runtime->HostWaitAny(*generic);
        }
    }  // namespace

    bool RegisterScriptTaskBindings(asIScriptEngine& engine, IScriptTaskRuntime& runtime, std::string& error)
    {
        error.clear();
        const ScriptBindingUtils require{"task"};

        const auto typedTaskType = std::format("{}<T>", TypedTaskName);
        const auto typedTaskDeclaration = std::format("{}<class T>", TypedTaskName);
        const auto qualifiedTaskType = std::format("{}::{}", CoreNamespace, TaskName);
        const auto qualifiedTypedTaskType = std::format("{}::{}", CoreNamespace, TypedTaskName);
        const auto taskHandleType = std::format("{}@", TaskName);
        const auto constTaskHandleType = std::format("const {}", taskHandleType);
        const auto taskCastDeclaration = std::format("{} opCast()", taskHandleType);
        const auto taskImplicitCastDeclaration = std::format("{} opImplCast()", taskHandleType);
        const auto constTaskCastDeclaration = std::format("{} opCast() const", constTaskHandleType);
        const auto constTaskImplicitCastDeclaration = std::format("{} opImplCast() const", constTaskHandleType);
        const auto taskOperation = [&](const std::string_view operation)
        { return std::format("{} {}", qualifiedTaskType, operation); };
        const auto typedTaskOperation = [&](const std::string_view operation)
        { return std::format("{} {}", qualifiedTypedTaskType, operation); };

        auto successful =
            require(
                engine.SetDefaultNamespace(CoreNamespace.data()), std::format("namespace {}", CoreNamespace), error) &&
            require(engine.RegisterObjectType(TaskName.data(), 0, asOBJ_REF), taskOperation("type"), error) &&
            require(engine.RegisterObjectBehaviour(
                        TaskName.data(), asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptTask, AddRef), asCALL_THISCALL),
                    taskOperation("addref"),
                    error) &&
            require(engine.RegisterObjectBehaviour(
                        TaskName.data(), asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptTask, Release), asCALL_THISCALL),
                    taskOperation("release"),
                    error) &&
            require(engine.RegisterObjectMethod(TaskName.data(),
                                                "bool get_IsCompleted() const property",
                                                asMETHOD(ScriptTask, IsCompleted),
                                                asCALL_THISCALL),
                    taskOperation("IsCompleted"),
                    error) &&
            require(engine.RegisterObjectMethod(TaskName.data(),
                                                "bool get_IsFailed() const property",
                                                asMETHOD(ScriptTask, IsFailed),
                                                asCALL_THISCALL),
                    taskOperation("IsFailed"),
                    error) &&
            require(engine.RegisterObjectMethod(
                        TaskName.data(), "void Retrieve(?&out) const", asFUNCTION(TaskRetrieveGeneric), asCALL_GENERIC),
                    taskOperation("Retrieve"),
                    error) &&
            require(engine.RegisterObjectMethod(
                        TaskName.data(), "void opCast(?&out)", asFUNCTION(TaskCastGeneric), asCALL_GENERIC),
                    taskOperation("opCast"),
                    error) &&
            require(engine.RegisterObjectMethod(
                        TaskName.data(), "void opImplCast(?&out)", asFUNCTION(TaskCastGeneric), asCALL_GENERIC),
                    taskOperation("opImplCast"),
                    error) &&
            require(engine.RegisterObjectMethod(
                        TaskName.data(), "void opCast(?&out) const", asFUNCTION(TaskCastGeneric), asCALL_GENERIC),
                    taskOperation("const opCast"),
                    error) &&
            require(engine.RegisterObjectMethod(
                        TaskName.data(), "void opImplCast(?&out) const", asFUNCTION(TaskCastGeneric), asCALL_GENERIC),
                    taskOperation("const opImplCast"),
                    error) &&
            require(engine.RegisterObjectType(typedTaskDeclaration.c_str(), 0, asOBJ_REF | asOBJ_TEMPLATE),
                    typedTaskOperation("type"),
                    error) &&
            require(engine.RegisterObjectBehaviour(typedTaskType.c_str(),
                                                   asBEHAVE_TEMPLATE_CALLBACK,
                                                   "bool f(int&in, bool&out)",
                                                   asFUNCTION(TypedTaskTemplateCallback),
                                                   asCALL_CDECL),
                    typedTaskOperation("template callback"),
                    error) &&
            require(
                engine.RegisterObjectBehaviour(
                    typedTaskType.c_str(), asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptTask, AddRef), asCALL_THISCALL),
                typedTaskOperation("addref"),
                error) &&
            require(engine.RegisterObjectBehaviour(typedTaskType.c_str(),
                                                   asBEHAVE_RELEASE,
                                                   "void f()",
                                                   asMETHOD(ScriptTask, Release),
                                                   asCALL_THISCALL),
                    typedTaskOperation("release"),
                    error) &&
            require(engine.RegisterObjectMethod(typedTaskType.c_str(),
                                                "void Retrieve(T&out) const",
                                                asFUNCTION(TypedTaskRetrieveGeneric),
                                                asCALL_GENERIC),
                    typedTaskOperation("Retrieve"),
                    error) &&
            require(engine.RegisterObjectMethod(typedTaskType.c_str(),
                                                taskCastDeclaration.c_str(),
                                                asFUNCTION(TypedTaskToTaskGeneric),
                                                asCALL_GENERIC),
                    typedTaskOperation("opCast"),
                    error) &&
            require(engine.RegisterObjectMethod(typedTaskType.c_str(),
                                                taskImplicitCastDeclaration.c_str(),
                                                asFUNCTION(TypedTaskToTaskGeneric),
                                                asCALL_GENERIC),
                    typedTaskOperation("opImplCast"),
                    error) &&
            require(engine.RegisterObjectMethod(typedTaskType.c_str(),
                                                constTaskCastDeclaration.c_str(),
                                                asFUNCTION(TypedTaskToTaskGeneric),
                                                asCALL_GENERIC),
                    typedTaskOperation("const opCast"),
                    error) &&
            require(engine.RegisterObjectMethod(typedTaskType.c_str(),
                                                constTaskImplicitCastDeclaration.c_str(),
                                                asFUNCTION(TypedTaskToTaskGeneric),
                                                asCALL_GENERIC),
                    typedTaskOperation("const opImplCast"),
                    error);

        const auto reset = require(engine.SetDefaultNamespace(""), "default namespace for task functions", error);
        successful = successful && reset;
        for (std::size_t parameterCount{1}; successful && parameterCount <= 10; ++parameterCount)
        {
            auto declaration = std::format("{}@ Async(", qualifiedTaskType);
            for (std::size_t parameter{}; parameter < parameterCount; ++parameter)
            {
                if (parameter != 0)
                    declaration += ", ";
                declaration += "?&in";
            }
            declaration += ')';
            successful = require(
                engine.RegisterGlobalFunction(declaration.c_str(), asFUNCTION(AsyncGeneric), asCALL_GENERIC, &runtime),
                declaration,
                error);
        }

        const auto qualifiedTaskHandleType = std::format("{}@", qualifiedTaskType);
        const auto waitDeclaration = std::format("void Wait({}+ task)", qualifiedTaskHandleType);
        const auto waitAllDeclaration = std::format("void WaitAll({}[] &in tasks)", qualifiedTaskHandleType);
        const auto waitAnyDeclaration =
            std::format("{} WaitAny({}[] &in tasks)", qualifiedTaskHandleType, qualifiedTaskHandleType);

        return successful &&
               require(engine.RegisterGlobalFunction(
                           waitDeclaration.c_str(), asFUNCTION(WaitGeneric), asCALL_GENERIC, &runtime),
                       "Wait",
                       error) &&
               require(engine.RegisterGlobalFunction(
                           waitAllDeclaration.c_str(), asFUNCTION(WaitAllGeneric), asCALL_GENERIC, &runtime),
                       "WaitAll",
                       error) &&
               require(engine.RegisterGlobalFunction(
                           waitAnyDeclaration.c_str(), asFUNCTION(WaitAnyGeneric), asCALL_GENERIC, &runtime),
                       "WaitAny",
                       error);
    }
}  // namespace PureMirror::Overlay
