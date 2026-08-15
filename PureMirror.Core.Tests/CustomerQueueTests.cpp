#include "CppUnitTest.h"
#include "CustomerQueue.h"

#include <cstddef>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Core::Tests
{
    TEST_CLASS(CustomerQueueTests)
    {
      public:
        TEST_METHOD(Process_IgnoresUnrelatedMessages)
        {
            CustomerQueue queue;

            queue.Process({.Character = "Alice", .Text = "Hello there"});

            Assert::IsTrue(queue.Customers().empty());
            Assert::IsTrue(queue.Waiting().empty());
        }

        TEST_METHOD(Process_AddsCustomerForUberElderRequestIgnoringCaseAndWhitespace)
        {
            CustomerQueue queue;

            queue.Process({.Character = "  Alice  ", .Text = "  I NEED UBER ELDER please  "});

            Assert::AreEqual(std::size_t{1}, queue.Customers().size());
            Assert::AreEqual(std::string{"Alice"}, queue.Customers()[0].Character);
            Assert::AreEqual(std::string{"I NEED UBER ELDER please"}, queue.Customers()[0].Messages[0]);
            Assert::IsTrue(queue.Customers()[0].State == CustomerState::Customer);
        }

        TEST_METHOD(Process_AppendsMessagesToAnExistingCustomerIgnoringCharacterCase)
        {
            CustomerQueue queue;
            queue.Process({.Character = "Alice", .Text = "Need uber elder"});

            queue.Process({.Character = "alice", .Text = "I am ready"});

            Assert::AreEqual(std::size_t{1}, queue.Customers().size());
            Assert::AreEqual(std::size_t{2}, queue.Customers()[0].Messages.size());
            Assert::AreEqual(std::string{"I am ready"}, queue.Customers()[0].Messages[1]);
        }

        TEST_METHOD(Process_MovesCustomerToWaitingAfterOfferIsAccepted)
        {
            CustomerQueue queue;
            queue.Process({.Character = "Alice", .Text = "Need uber elder"});
            queue.MarkWaitingOffered(0);

            queue.Process({.Character = "Alice", .Text = " YES "});

            Assert::IsTrue(queue.Customers().empty());
            Assert::AreEqual(std::size_t{1}, queue.Waiting().size());
            Assert::IsTrue(queue.Waiting()[0].State == CustomerState::Waiting);
            Assert::IsFalse(queue.Waiting()[0].WaitingOffered);
            Assert::AreEqual(std::string{"yes"}, queue.Waiting()[0].Messages.back());
        }

        TEST_METHOD(InviteCustomer_MovesCustomerToWaitingAsInvited)
        {
            CustomerQueue queue;
            queue.Process({.Character = "Alice", .Text = "Need uber elder"});

            queue.InviteCustomer(0);

            Assert::IsTrue(queue.Customers().empty());
            Assert::AreEqual(std::size_t{1}, queue.Waiting().size());
            Assert::IsTrue(queue.Waiting()[0].State == CustomerState::Invited);
            Assert::IsFalse(queue.WaitingPosition(0).has_value());
        }

        TEST_METHOD(WaitingPosition_SkipsInvitedCustomers)
        {
            CustomerQueue queue;
            queue.Process({.Character = "Alice", .Text = "Need uber elder"});
            queue.Process({.Character = "Bob", .Text = "Need uber elder"});
            queue.Process({.Character = "Charlie", .Text = "Need uber elder"});
            queue.InviteCustomer(0);
            queue.MarkWaitingOffered(0);
            queue.Process({.Character = "Bob", .Text = "yes"});
            queue.MarkWaitingOffered(0);
            queue.Process({.Character = "Charlie", .Text = "yes"});

            const auto bobPosition = queue.WaitingPosition(1);
            const auto charliePosition = queue.WaitingPosition(2);

            Assert::IsTrue(bobPosition.has_value());
            Assert::IsTrue(charliePosition.has_value());
            Assert::AreEqual(std::size_t{1}, *bobPosition);
            Assert::AreEqual(std::size_t{2}, *charliePosition);
        }
    };
}  // namespace PureMirror::Core::Tests
