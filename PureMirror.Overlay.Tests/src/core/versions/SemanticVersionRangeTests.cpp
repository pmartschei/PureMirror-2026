#include "CppUnitTest.h"
#include "src/core/versions/SemanticVersion.h"
#include "src/core/versions/SemanticVersionRange.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    TEST_CLASS(SemanticVersionRangeTests)
    {
      public:
        TEST_METHOD(Contains_AcceptsVersionInsideConjunctiveRange)
        {
            const auto version = SemanticVersion::Parse("1.5.2");

            Assert::IsTrue(version.has_value());
            Assert::IsTrue(SemanticVersionRange::Contains(">=1.2.0 <2.0.0", *version));
        }

        TEST_METHOD(Contains_AcceptsVersionInsideConjunctiveRangeWithLowerBound)
        {
            const auto version = SemanticVersion::Parse("1.5.2");

            Assert::IsTrue(version.has_value());
            Assert::IsTrue(SemanticVersionRange::Contains(">=1.2.0", *version));
        }

        TEST_METHOD(Contains_RejectsVersionOutsideConjunctiveRange)
        {
            const auto version = SemanticVersion::Parse("2.0.0");

            Assert::IsTrue(version.has_value());
            Assert::IsFalse(SemanticVersionRange::Contains(">=1.2.0 <2.0.0", *version));
        }

        TEST_METHOD(IsValid_RejectsUnsupportedRangeSyntax)
        {
            Assert::IsFalse(SemanticVersionRange::IsValid("^1.2.0"));
            Assert::IsFalse(SemanticVersionRange::IsValid("1.2"));
        }
    };
}  // namespace PureMirror::Overlay::Tests
