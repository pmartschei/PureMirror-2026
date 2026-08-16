#include "CppUnitTest.h"
#include "src/core/Logger.h"

#include <cstddef>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    TEST_CLASS(LoggerTests)
    {
      public:
        TEST_METHOD(Log_PreservesStructuredProperties)
        {
            Logger logger(4);
            const LogColor color{0.1f, 0.2f, 0.3f, 0.4f};

            logger.Log(LogLevel::Warning, "example.plugin", "Something happened", "plugin.warning", &color);

            const auto messages = logger.Snapshot();
            Assert::AreEqual(std::size_t{1}, messages.size());
            Assert::AreEqual(std::string{"example.plugin"}, messages[0].m_Origin);
            Assert::AreEqual(std::string{"Something happened"}, messages[0].m_Content);
            Assert::AreEqual(std::string{"plugin.warning"}, messages[0].m_MessageId);
            Assert::AreEqual(std::uint32_t{10}, messages[0].m_OccurrenceLimit);
            Assert::IsTrue(messages[0].m_Level == LogLevel::Warning);
            Assert::AreEqual(0.1f, messages[0].m_Color.m_Red);
            Assert::IsTrue(messages[0].m_Timestamp.time_since_epoch().count() > 0);
        }

        TEST_METHOD(Log_AppendsOccurrencesWithoutChangingHistory)
        {
            Logger logger(4);
            logger.Info("test", "first occurrence", "shared.message");
            logger.Warning("test", "latest occurrence", "shared.message");

            const auto messages = logger.Snapshot();
            Assert::AreEqual(std::size_t{2}, messages.size());
            Assert::AreEqual(std::string{"first occurrence"}, messages[0].m_Content);
            Assert::IsTrue(messages[0].m_Level == LogLevel::Info);
            Assert::AreEqual(std::string{"latest occurrence"}, messages[1].m_Content);
            Assert::IsTrue(messages[1].m_Level == LogLevel::Warning);
        }

        TEST_METHOD(Log_StopsAppendingMessageAfterItsOccurrenceLimit)
        {
            Logger logger(4);
            logger.Info("test", "first", "limited.message", 2);
            logger.Info("test", "second", "limited.message", 2);
            logger.Info("test", "third", "limited.message", 2);

            const auto messages = logger.Snapshot();
            Assert::AreEqual(std::size_t{2}, messages.size());
            Assert::AreEqual(std::string{"first"}, messages[0].m_Content);
            Assert::AreEqual(std::string{"second"}, messages[1].m_Content);
            Assert::AreEqual(std::uint32_t{2}, messages[0].m_OccurrenceLimit);
        }

        TEST_METHOD(Log_UsesDefaultOccurrenceLimitOfTen)
        {
            Logger logger(20);
            for (std::uint32_t occurrence = 0; occurrence < 11; ++occurrence)
                logger.Info("test", "occurrence", "default-limited.message");

            Assert::AreEqual(std::size_t{10}, logger.Size());
        }

        TEST_METHOD(Log_DropsOldestMessageAtCapacity)
        {
            Logger logger(2);
            logger.Info("test", "first");
            logger.Info("test", "second");
            logger.Info("test", "third");

            const auto messages = logger.Snapshot();

            Assert::AreEqual(std::size_t{2}, messages.size());
            Assert::AreEqual(std::string{"second"}, messages[0].m_Content);
            Assert::AreEqual(std::string{"third"}, messages[1].m_Content);
            Assert::IsFalse(messages[0].m_MessageId.empty());
            Assert::IsTrue(messages[0].m_Sequence < messages[1].m_Sequence);
        }

        TEST_METHOD(Clear_RemovesAllMessages)
        {
            Logger logger;
            logger.Error("test", "failure", "clear.message", 1);

            logger.Clear();

            Assert::AreEqual(std::size_t{0}, logger.Size());

            logger.Error("test", "failure", "clear.message", 1);
            Assert::AreEqual(std::size_t{1}, logger.Size());
        }
    };
}  // namespace PureMirror::Overlay::Tests
