#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>

namespace PureMirror::Overlay
{
    struct SemanticVersion
    {
        std::uint64_t Major{};
        std::uint64_t Minor{};
        std::uint64_t Patch{};

        auto operator<=>(const SemanticVersion&) const = default;

        [[nodiscard]] static std::optional<SemanticVersion> Parse(std::string_view value);
    };
}  // namespace PureMirror::Overlay
