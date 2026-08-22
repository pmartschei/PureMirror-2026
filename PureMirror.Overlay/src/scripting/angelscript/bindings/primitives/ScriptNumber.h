#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace PureMirror::Overlay
{
    template <typename T> class ScriptNumber
    {
        static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);

      public:
        explicit ScriptNumber(T value = {}) noexcept;

        void AddRef() const noexcept;
        void Release() const noexcept;

        ScriptNumber& Assign(T value) noexcept;
        ScriptNumber& Assign(const ScriptNumber& value) noexcept;

        [[nodiscard]] T GetValue() const noexcept;
        void SetValue(T value) noexcept;
        [[nodiscard]] T ToPrimitive() const noexcept;

      private:
        ~ScriptNumber() = default;

        mutable std::atomic_int m_ReferenceCount{1};
        T m_Value{};
    };

    using ScriptInt = ScriptNumber<std::int32_t>;
    using ScriptUInt = ScriptNumber<std::uint32_t>;
    using ScriptLong = ScriptNumber<std::int64_t>;
    using ScriptULong = ScriptNumber<std::uint64_t>;

    extern template class ScriptNumber<std::int32_t>;
    extern template class ScriptNumber<std::uint32_t>;
    extern template class ScriptNumber<std::int64_t>;
    extern template class ScriptNumber<std::uint64_t>;
}  // namespace PureMirror::Overlay
