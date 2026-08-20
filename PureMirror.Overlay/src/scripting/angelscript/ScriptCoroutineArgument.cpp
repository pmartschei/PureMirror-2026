#include "pch.h"

#include "ScriptCoroutineArgument.h"

#include <cstring>

namespace PureMirror::Overlay
{
    ScriptCoroutineArgument::ScriptCoroutineArgument(asIScriptEngine& engine, void* value, const int typeId)
        : m_Engine(engine)
    {
        m_IsObject = (typeId & asTYPEID_MASK_OBJECT) != 0;
        if (!m_IsObject)
        {
            std::memcpy(m_PrimitiveValue.data(), value, engine.GetSizeOfPrimitiveType(typeId));
            return;
        }

        m_ObjectType = engine.GetTypeInfoById(typeId);
        auto* object = (typeId & asTYPEID_OBJHANDLE) != 0 ? *static_cast<void**>(value) : value;
        if (object == nullptr || m_ObjectType == nullptr)
            return;
        if ((typeId & asTYPEID_OBJHANDLE) != 0)
        {
            m_ObjectValue = object;
            engine.AddRefScriptObject(m_ObjectValue, m_ObjectType);
        }
        else
        {
            m_ObjectValue = engine.CreateScriptObjectCopy(object, m_ObjectType);
        }
    }

    ScriptCoroutineArgument::~ScriptCoroutineArgument()
    {
        if (m_ObjectValue != nullptr && m_ObjectType != nullptr)
            m_Engine.ReleaseScriptObject(m_ObjectValue, m_ObjectType);
    }

    void* ScriptCoroutineArgument::Value() noexcept
    {
        return m_IsObject ? m_ObjectValue : m_PrimitiveValue.data();
    }
}  // namespace PureMirror::Overlay
