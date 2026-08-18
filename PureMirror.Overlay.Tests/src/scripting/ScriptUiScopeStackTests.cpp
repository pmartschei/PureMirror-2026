#include "CppUnitTest.h"
#include "src/scripting/ScriptUiScopeStack.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    TEST_CLASS(ScriptUiScopeStackTests)
    {
      public:
        TEST_METHOD(Close_OutOfOrderRecoversNestedScopesInReverseOrder)
        {
            ScriptUiScopeStack scopes;
            std::vector<std::string> closedCommands;
            std::vector<std::string> recoveredCommands;
            scopes.Open("plugin", "ImGui::Begin()", "ImGui::End()", [&] { closedCommands.emplace_back("End"); });
            scopes.Open(
                "plugin", "ImGui::BeginChild()", "ImGui::EndChild()", [&] { closedCommands.emplace_back("EndChild"); });
            scopes.Open(
                "plugin", "ImGui::BeginTable()", "ImGui::EndTable()", [&] { closedCommands.emplace_back("EndTable"); });

            const auto closed = scopes.Close(
                "ImGui::End()", [&](const ScriptUiScope& scope) { recoveredCommands.push_back(scope.ClosingCommand); });

            Assert::IsTrue(closed);
            Assert::IsTrue(scopes.IsEmpty());
            Assert::AreEqual(std::size_t{3}, closedCommands.size());
            Assert::AreEqual(std::string{"EndTable"}, closedCommands[0]);
            Assert::AreEqual(std::string{"EndChild"}, closedCommands[1]);
            Assert::AreEqual(std::string{"End"}, closedCommands[2]);
            Assert::AreEqual(std::size_t{2}, recoveredCommands.size());
            Assert::AreEqual(std::string{"ImGui::EndTable()"}, recoveredCommands[0]);
            Assert::AreEqual(std::string{"ImGui::EndChild()"}, recoveredCommands[1]);
        }

        TEST_METHOD(CloseAll_RecoversEveryScopeAndUnmatchedCloseChangesNothing)
        {
            ScriptUiScopeStack scopes;
            std::vector<std::string> recoveredCommands;
            scopes.Open("plugin", "Begin A", "End A", [] {});
            scopes.Open("plugin", "Begin B", "End B", [] {});

            const auto unmatched = scopes.Close("End C", [](const ScriptUiScope&) {});
            Assert::IsFalse(unmatched);
            Assert::AreEqual(std::size_t{2}, scopes.Size());

            scopes.CloseAll([&](const ScriptUiScope& scope) { recoveredCommands.push_back(scope.ClosingCommand); });

            Assert::IsTrue(scopes.IsEmpty());
            Assert::AreEqual(std::string{"End B"}, recoveredCommands[0]);
            Assert::AreEqual(std::string{"End A"}, recoveredCommands[1]);
        }
    };
}  // namespace PureMirror::Overlay::Tests
