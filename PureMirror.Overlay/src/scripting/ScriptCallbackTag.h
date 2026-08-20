#pragma once

#include <cstdint>

namespace PureMirror::Overlay
{
    enum class ScriptCallbackTag : std::uint8_t
    {
        None = 0,
        Suspendable = 1 << 0,
        Ui = 1 << 1,
        Coroutine = 1 << 2
    };

    [[nodiscard]] constexpr ScriptCallbackTag operator|(const ScriptCallbackTag left,
                                                        const ScriptCallbackTag right) noexcept
    {
        return static_cast<ScriptCallbackTag>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr bool HasTag(const ScriptCallbackTag tags, const ScriptCallbackTag tag) noexcept
    {
        return (static_cast<std::uint8_t>(tags) & static_cast<std::uint8_t>(tag)) != 0;
    }
}  // namespace PureMirror::Overlay
