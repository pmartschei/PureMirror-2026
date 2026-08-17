#include "pch.h"

#include "SemanticVersion.h"

#include <limits>

namespace PureMirror::Overlay
{
    std::optional<SemanticVersion> SemanticVersion::Parse(const std::string_view value)
    {
        SemanticVersion version;
        std::uint64_t* components[]{&version.Major, &version.Minor, &version.Patch};
        std::size_t position{};

        for (std::size_t component{}; component < 3; ++component)
        {
            const auto start = position;
            while (position < value.size() && value[position] >= '0' && value[position] <= '9')
            {
                const auto digit = static_cast<std::uint64_t>(value[position] - '0');
                if (*components[component] > ((std::numeric_limits<std::uint64_t>::max)() - digit) / 10)
                    return std::nullopt;
                *components[component] = *components[component] * 10 + digit;
                ++position;
            }
            if (start == position || (position - start > 1 && value[start] == '0'))
                return std::nullopt;
            if (component + 1 < 3)
            {
                if (position >= value.size() || value[position] != '.')
                    return std::nullopt;
                ++position;
            }
        }
        return position == value.size() ? std::optional{version} : std::nullopt;
    }
}  // namespace PureMirror::Overlay
