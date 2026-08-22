#pragma once

#include <atomic>

namespace PureMirror::Overlay
{
    class ScriptBool
    {
      public:
        explicit ScriptBool(bool value = false) noexcept;

        void AddRef() const noexcept;
        void Release() const noexcept;

        ScriptBool& Assign(bool value) noexcept;
        ScriptBool& Assign(const ScriptBool& value) noexcept;

        [[nodiscard]] bool GetValue() const noexcept;
        void SetValue(bool value) noexcept;
        [[nodiscard]] bool ToBool() const noexcept;

      private:
        ~ScriptBool() = default;

        mutable std::atomic_int m_ReferenceCount{1};
        bool m_Value{};
    };
}  // namespace PureMirror::Overlay
