#include "pch.h"

#include "ScriptBool.h"

namespace PureMirror::Overlay
{
    ScriptBool::ScriptBool(const bool value) noexcept : m_Value(value) {}

    void ScriptBool::AddRef() const noexcept
    {
        ++m_ReferenceCount;
    }

    void ScriptBool::Release() const noexcept
    {
        if (--m_ReferenceCount == 0)
            delete this;
    }

    ScriptBool& ScriptBool::Assign(const bool value) noexcept
    {
        m_Value = value;
        return *this;
    }

    ScriptBool& ScriptBool::Assign(const ScriptBool& value) noexcept
    {
        return Assign(value.m_Value);
    }

    bool ScriptBool::GetValue() const noexcept
    {
        return m_Value;
    }

    void ScriptBool::SetValue(const bool value) noexcept
    {
        m_Value = value;
    }

    bool ScriptBool::ToBool() const noexcept
    {
        return m_Value;
    }
}  // namespace PureMirror::Overlay
