#include "pch.h"

#include "ScriptNumber.h"

namespace PureMirror::Overlay
{
    template <typename T> ScriptNumber<T>::ScriptNumber(const T value) noexcept : m_Value(value) {}

    template <typename T> void ScriptNumber<T>::AddRef() const noexcept
    {
        ++m_ReferenceCount;
    }

    template <typename T> void ScriptNumber<T>::Release() const noexcept
    {
        if (--m_ReferenceCount == 0)
            delete this;
    }

    template <typename T> ScriptNumber<T>& ScriptNumber<T>::Assign(const T value) noexcept
    {
        m_Value = value;
        return *this;
    }

    template <typename T> ScriptNumber<T>& ScriptNumber<T>::Assign(const ScriptNumber& value) noexcept
    {
        return Assign(value.m_Value);
    }

    template <typename T> T ScriptNumber<T>::GetValue() const noexcept
    {
        return m_Value;
    }

    template <typename T> void ScriptNumber<T>::SetValue(const T value) noexcept
    {
        m_Value = value;
    }

    template <typename T> T ScriptNumber<T>::ToPrimitive() const noexcept
    {
        return m_Value;
    }

    template class ScriptNumber<std::int32_t>;
    template class ScriptNumber<std::uint32_t>;
    template class ScriptNumber<std::int64_t>;
    template class ScriptNumber<std::uint64_t>;
}  // namespace PureMirror::Overlay
