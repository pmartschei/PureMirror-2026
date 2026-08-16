#pragma once

#include "CommandDescriptor.h"
#include "CommandResult.h"
#include "ConsoleCommand.h"

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PureMirror::Overlay
{
    class CommandRegistry
    {
      public:
        [[nodiscard]] bool Register(ConsoleCommand command);
        [[nodiscard]] bool Unregister(std::string_view name, std::string_view origin);
        [[nodiscard]] CommandResult Execute(std::string_view input) const;
        [[nodiscard]] std::vector<CommandDescriptor> Commands() const;

      private:
        [[nodiscard]] static std::string NormalizeName(std::string_view name);

        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, ConsoleCommand> m_Commands;
    };
}  // namespace PureMirror::Overlay
