#include "pch.h"

#include "scripting/angelscript/bindings/async/ScriptTask.h"

namespace PureMirror::Overlay
{
    ScriptTask::ScriptTask(asIScriptEngine& engine, const int resultTypeId)
        : m_Engine(engine), m_ResultTypeId(resultTypeId)
    {
    }

    ScriptTask::~ScriptTask()
    {
        ReleaseResult();
        if (m_ResolvedTask != nullptr)
            m_ResolvedTask->Release();
    }

    void ScriptTask::AddRef() const noexcept
    {
        ++m_ReferenceCount;
    }

    void ScriptTask::Release() const noexcept
    {
        if (--m_ReferenceCount == 0)
            delete this;
    }

    void ScriptTask::Complete(asIScriptContext& context, const std::uint64_t completionOrder)
    {
        ReleaseResult();
        const auto normalizedTypeId = NormalizeTypeId(m_ResultTypeId);
        switch (normalizedTypeId)
        {
        case asTYPEID_VOID:
            break;
        case asTYPEID_INT64:
            switch (m_ResultTypeId)
            {
            case asTYPEID_INT8:
                m_IntegerResult = static_cast<asINT8>(context.GetReturnByte());
                break;
            case asTYPEID_INT16:
                m_IntegerResult = static_cast<asINT16>(context.GetReturnWord());
                break;
            case asTYPEID_INT32:
                m_IntegerResult = static_cast<asINT32>(context.GetReturnDWord());
                break;
            default:
                m_IntegerResult = static_cast<asINT64>(context.GetReturnQWord());
                break;
            }
            break;
        case asTYPEID_DOUBLE:
            m_DoubleResult = m_ResultTypeId == asTYPEID_FLOAT ? context.GetReturnFloat() : context.GetReturnDouble();
            break;
        case asTYPEID_BOOL:
            m_IntegerResult = context.GetReturnByte() != 0;
            break;
        default:
            m_ObjectType = m_Engine.GetTypeInfoById(m_ResultTypeId);
            if (m_ObjectType != nullptr)
            {
                auto* result = context.GetReturnObject();
                if ((m_ResultTypeId & asTYPEID_OBJHANDLE) != 0)
                {
                    m_ObjectResult = result;
                    if (m_ObjectResult != nullptr)
                        m_Engine.AddRefScriptObject(m_ObjectResult, m_ObjectType);
                }
                else if (result != nullptr)
                {
                    m_ObjectResult = m_Engine.CreateScriptObjectCopy(result, m_ObjectType);
                }
            }
            break;
        }
        m_CompletionOrder = completionOrder;
        m_IsCompleted = true;
        m_IsFailed = false;
        m_Error.clear();
    }

    void ScriptTask::Fail(std::string message, const std::uint64_t completionOrder)
    {
        ReleaseResult();
        m_Error = std::move(message);
        m_CompletionOrder = completionOrder;
        m_IsCompleted = true;
        m_IsFailed = true;
    }

    void ScriptTask::ResolveTo(ScriptTask& task)
    {
        if (m_ResolvedTask != nullptr)
            m_ResolvedTask->Release();
        m_ResolvedTask = &task;
        m_ResolvedTask->AddRef();
    }

    bool ScriptTask::IsCompleted() const noexcept
    {
        return m_ResolvedTask != nullptr ? ResolvedTask().IsCompleted() : m_IsCompleted;
    }

    bool ScriptTask::IsFailed() const noexcept
    {
        return m_ResolvedTask != nullptr ? ResolvedTask().IsFailed() : m_IsFailed;
    }

    std::uint64_t ScriptTask::CompletionOrder() const noexcept
    {
        return m_ResolvedTask != nullptr ? ResolvedTask().CompletionOrder() : m_CompletionOrder;
    }

    int ScriptTask::ResultTypeId() const noexcept
    {
        return m_ResolvedTask != nullptr ? ResolvedTask().ResultTypeId() : m_ResultTypeId;
    }

    std::string_view ScriptTask::Error() const noexcept
    {
        return m_ResolvedTask != nullptr ? ResolvedTask().Error() : std::string_view(m_Error);
    }

    bool ScriptTask::Retrieve(void* output, const int typeId) const
    {
        if (m_ResolvedTask != nullptr)
            return ResolvedTask().Retrieve(output, typeId);
        if (!m_IsCompleted || m_IsFailed || output == nullptr ||
            NormalizeTypeId(typeId) != NormalizeTypeId(m_ResultTypeId))
            return false;

        switch (NormalizeTypeId(typeId))
        {
        case asTYPEID_VOID:
            return true;
        case asTYPEID_INT64:
            switch (typeId)
            {
            case asTYPEID_INT8:
            case asTYPEID_UINT8:
                *static_cast<asBYTE*>(output) = static_cast<asBYTE>(m_IntegerResult);
                break;
            case asTYPEID_INT16:
            case asTYPEID_UINT16:
                *static_cast<asWORD*>(output) = static_cast<asWORD>(m_IntegerResult);
                break;
            case asTYPEID_INT32:
            case asTYPEID_UINT32:
                *static_cast<asDWORD*>(output) = static_cast<asDWORD>(m_IntegerResult);
                break;
            default:
                *static_cast<asQWORD*>(output) = static_cast<asQWORD>(m_IntegerResult);
                break;
            }
            return true;
        case asTYPEID_DOUBLE:
            if (typeId == asTYPEID_FLOAT)
                *static_cast<float*>(output) = static_cast<float>(m_DoubleResult);
            else
                *static_cast<double*>(output) = m_DoubleResult;
            return true;
        case asTYPEID_BOOL:
            *static_cast<bool*>(output) = m_IntegerResult != 0;
            return true;
        default:
            if (m_ObjectType == nullptr)
                return false;
            if ((typeId & asTYPEID_OBJHANDLE) != 0)
            {
                *static_cast<void**>(output) = m_ObjectResult;
                if (m_ObjectResult != nullptr)
                    m_Engine.AddRefScriptObject(m_ObjectResult, m_ObjectType);
                return true;
            }
            return m_ObjectResult != nullptr && m_Engine.AssignScriptObject(output, m_ObjectResult, m_ObjectType) >= 0;
        }
    }

    bool ScriptTask::Retrieve(asINT64& output) const noexcept
    {
        if (m_ResolvedTask != nullptr)
            return ResolvedTask().Retrieve(output);
        if (!m_IsCompleted || m_IsFailed || NormalizeTypeId(m_ResultTypeId) != asTYPEID_INT64)
            return false;
        output = m_IntegerResult;
        return true;
    }

    bool ScriptTask::Retrieve(double& output) const noexcept
    {
        if (m_ResolvedTask != nullptr)
            return ResolvedTask().Retrieve(output);
        if (!m_IsCompleted || m_IsFailed || NormalizeTypeId(m_ResultTypeId) != asTYPEID_DOUBLE)
            return false;
        output = m_DoubleResult;
        return true;
    }

    bool ScriptTask::Retrieve(asIScriptGeneric& generic) const
    {
        if (m_ResolvedTask != nullptr)
            return ResolvedTask().Retrieve(generic);
        const auto typeId = generic.GetReturnTypeId();
        if (!m_IsCompleted || m_IsFailed || NormalizeTypeId(typeId) != NormalizeTypeId(m_ResultTypeId))
            return false;
        switch (NormalizeTypeId(typeId))
        {
        case asTYPEID_INT64:
            switch (typeId)
            {
            case asTYPEID_INT8:
            case asTYPEID_UINT8:
                return generic.SetReturnByte(static_cast<asBYTE>(m_IntegerResult)) >= 0;
            case asTYPEID_INT16:
            case asTYPEID_UINT16:
                return generic.SetReturnWord(static_cast<asWORD>(m_IntegerResult)) >= 0;
            case asTYPEID_INT32:
            case asTYPEID_UINT32:
                return generic.SetReturnDWord(static_cast<asDWORD>(m_IntegerResult)) >= 0;
            default:
                return generic.SetReturnQWord(static_cast<asQWORD>(m_IntegerResult)) >= 0;
            }
        case asTYPEID_DOUBLE:
            return typeId == asTYPEID_FLOAT ? generic.SetReturnFloat(static_cast<float>(m_DoubleResult)) >= 0
                                            : generic.SetReturnDouble(m_DoubleResult) >= 0;
        case asTYPEID_BOOL:
            return generic.SetReturnByte(m_IntegerResult != 0) >= 0;
        default:
            return generic.SetReturnObject(m_ObjectResult) >= 0;
        }
    }

    bool ScriptTask::Cast(void* output, const int typeId) const
    {
        if (m_ResolvedTask != nullptr)
            return ResolvedTask().Cast(output, typeId);
        if (output == nullptr)
            return false;
        auto* targetType = m_Engine.GetTypeInfoById(typeId);
        if (targetType == nullptr || std::string_view(targetType->GetNamespace()) != "Core" ||
            std::string_view(targetType->GetName()) != "TypedTask" ||
            NormalizeTypeId(targetType->GetSubTypeId()) != NormalizeTypeId(m_ResultTypeId))
        {
            *static_cast<ScriptTask**>(output) = nullptr;
            return false;
        }
        *static_cast<ScriptTask**>(output) = const_cast<ScriptTask*>(this);
        AddRef();
        return true;
    }

    int ScriptTask::NormalizeTypeId(const int typeId) noexcept
    {
        switch (typeId)
        {
        case asTYPEID_INT8:
        case asTYPEID_INT16:
        case asTYPEID_INT32:
        case asTYPEID_INT64:
        case asTYPEID_UINT8:
        case asTYPEID_UINT16:
        case asTYPEID_UINT32:
        case asTYPEID_UINT64:
            return asTYPEID_INT64;
        case asTYPEID_FLOAT:
        case asTYPEID_DOUBLE:
            return asTYPEID_DOUBLE;
        default:
            return typeId & ~asTYPEID_HANDLETOCONST;
        }
    }

    void ScriptTask::ReleaseResult() noexcept
    {
        if (m_ObjectResult != nullptr && m_ObjectType != nullptr)
            m_Engine.ReleaseScriptObject(m_ObjectResult, m_ObjectType);
        m_ObjectResult = nullptr;
        m_ObjectType = nullptr;
    }

    const ScriptTask& ScriptTask::ResolvedTask() const noexcept
    {
        return m_ResolvedTask != nullptr ? m_ResolvedTask->ResolvedTask() : *this;
    }
}  // namespace PureMirror::Overlay
