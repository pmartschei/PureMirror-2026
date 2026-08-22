#include "CppUnitTest.h"
#include "src/scripting/angelscript/bindings/ScriptBindingUtils.h"

#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    TEST_CLASS(ScriptBindingUtilsTests)
    {
      public:
        TEST_METHOD(Require_AcceptsNonnegativeCodesAndFormatsFailures)
        {
            std::string error{"unchanged"};
            const auto binding = []
            {
                const std::string bindingGroup{"task"};
                return ScriptBindingUtils{bindingGroup};
            }();

            Assert::IsTrue(binding.Require(0, "Core::Task type", error));
            Assert::AreEqual(std::string{"unchanged"}, error);
            Assert::IsFalse(binding.Require(-17, "Core::Task type", error));
            Assert::AreEqual(std::string{"AngelScript task binding failed at Core::Task type with code -17."}, error);
        }
    };
}  // namespace PureMirror::Overlay::Tests
