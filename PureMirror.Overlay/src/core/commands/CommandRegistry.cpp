#include "pch.h"

#include "CommandRegistry.h"

namespace PureMirror::Overlay
{
    namespace
    {
        std::string_view Trim(const std::string_view value)
        {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos)
                return {};
            const auto last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }
    }  // namespace

    bool CommandRegistry::Register(ConsoleCommand command)
    {
        command.Name = NormalizeName(command.Name);
        if (command.Name.empty() || !command.Handler || command.Origin.empty())
            return false;

        std::scoped_lock lock(m_Mutex);
        const auto name = command.Name;
        return m_Commands.emplace(name, std::move(command)).second;
    }

    bool CommandRegistry::Unregister(const std::string_view name, const std::string_view origin)
    {
        const auto normalized = NormalizeName(name);
        std::scoped_lock lock(m_Mutex);
        const auto command = m_Commands.find(normalized);
        if (command == m_Commands.end() || command->second.Origin != origin)
            return false;
        m_Commands.erase(command);
        return true;
    }

    CommandResult CommandRegistry::Execute(const std::string_view input) const
    {
        auto remaining = Trim(input);
        if (remaining.empty() || remaining.front() != '/')
            return {.Status = CommandStatus::NotACommand, .Message = "Commands start with '/'."};

        remaining.remove_prefix(1);
        const auto separator = remaining.find_first_of(" \t\r\n");
        const auto name = NormalizeName(remaining.substr(0, separator));
        const auto arguments =
            separator == std::string_view::npos ? std::string_view{} : Trim(remaining.substr(separator));

        CommandHandler handler;
        {
            std::scoped_lock lock(m_Mutex);
            const auto command = m_Commands.find(name);
            if (command == m_Commands.end())
                return {.Status = CommandStatus::UnknownCommand, .Message = "Unknown command: /" + name};
            handler = command->second.Handler;
        }

        try
        {
            return handler(arguments);
        }
        catch (const std::exception& exception)
        {
            return CommandResult::Failure(exception.what());
        }
        catch (...)
        {
            return CommandResult::Failure("Command failed with an unknown error.");
        }
    }

    std::vector<CommandDescriptor> CommandRegistry::Commands() const
    {
        std::scoped_lock lock(m_Mutex);
        std::vector<CommandDescriptor> result;
        result.reserve(m_Commands.size());
        for (const auto& [name, command] : m_Commands)
            result.push_back({.Name = name, .Description = command.Description, .Origin = command.Origin});
        std::ranges::sort(result, {}, &CommandDescriptor::Name);
        return result;
    }

    std::string CommandRegistry::NormalizeName(std::string_view name)
    {
        name = Trim(name);
        if (!name.empty() && name.front() == '/')
            name.remove_prefix(1);
        if (name.empty() || name.find_first_of(" \t\r\n/") != std::string_view::npos)
            return {};

        std::string normalized(name);
        std::ranges::transform(normalized,
                               normalized.begin(),
                               [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
        return normalized;
    }
}  // namespace PureMirror::Overlay
