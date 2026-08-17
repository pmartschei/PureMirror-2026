#include "CppUnitTest.h"
#include "src/core/commands/CommandRegistry.h"

#include <string>
#include <string_view>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    TEST_CLASS(CommandRegistryTests)
    {
      public:
        TEST_METHOD(RegisterAndExecute_NormalizesNameAndPassesArguments)
        {
            CommandRegistry registry;
            std::string receivedArguments;
            const auto registered = registry.Register({.Name = "/Echo",
                                                       .Description = "Echoes text.",
                                                       .Origin = "example.plugin",
                                                       .Handler = [&](const std::string_view arguments)
                                                       {
                                                           receivedArguments = arguments;
                                                           return CommandResult::Success(std::string(arguments));
                                                       }});

            const auto result = registry.Execute("  /ECHO hello world  ");

            Assert::IsTrue(registered);
            Assert::IsTrue(result.Status == CommandStatus::Executed);
            Assert::AreEqual(std::string{"hello world"}, receivedArguments);
            Assert::AreEqual(std::string{"hello world"}, result.Message);
        }

        TEST_METHOD(Register_RejectsDuplicateCommand)
        {
            CommandRegistry registry;
            const auto handler = [](std::string_view) { return CommandResult::Success(); };

            const auto first = registry.Register({"clear", "First", "host", handler});
            const auto second = registry.Register({"CLEAR", "Second", "plugin", handler});

            Assert::IsTrue(first);
            Assert::IsFalse(second);
        }

        TEST_METHOD(Unregister_RequiresMatchingOrigin)
        {
            CommandRegistry registry;
            const auto registered =
                registry.Register({"test", "Test", "owner", [](std::string_view) { return CommandResult::Success(); }});

            Assert::IsTrue(registered);
            Assert::IsFalse(registry.Unregister("test", "another-plugin"));
            Assert::IsTrue(registry.Unregister("test", "owner"));
            Assert::IsTrue(registry.Execute("/test").Status == CommandStatus::UnknownCommand);
        }
    };
}  // namespace PureMirror::Overlay::Tests
