#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>

namespace PureMirror::Overlay
{
    using ScriptCallbackArgumentValue = std::variant<bool,
                                                     std::int8_t,
                                                     std::uint8_t,
                                                     std::int16_t,
                                                     std::uint16_t,
                                                     std::int32_t,
                                                     std::uint32_t,
                                                     std::int64_t,
                                                     std::uint64_t,
                                                     float,
                                                     double>;

    struct ScriptCallbackArgument
    {
        std::size_t Index{};
        ScriptCallbackArgumentValue Value;
    };
}  // namespace PureMirror::Overlay