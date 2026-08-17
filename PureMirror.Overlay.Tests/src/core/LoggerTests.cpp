#include "CppUnitTest.h"
#include "src/core/Logger.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace PureMirror::Overlay::Tests
{
    namespace
    {
        class RecordingLogWriter final : public ILogWriter
        {
          public:
            void Write(const LogMessage& message) override
            {
                Messages.push_back(message);
            }

            std::vector<LogMessage> Messages;
        };
    }  // namespace

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
            Assert::AreEqual(std::string{"example.plugin"}, messages[0].Origin);
            Assert::AreEqual(std::string{"Something happened"}, messages[0].Content);
            Assert::AreEqual(std::string{"plugin.warning"}, messages[0].MessageId);
            Assert::AreEqual(std::uint32_t{10}, messages[0].OccurrenceLimit);
            Assert::IsTrue(messages[0].Level == LogLevel::Warning);
            Assert::AreEqual(0.1f, messages[0].Color.Red);
            Assert::IsTrue(messages[0].Timestamp.time_since_epoch().count() > 0);
        }

        TEST_METHOD(Log_AppendsOccurrencesWithoutChangingHistory)
        {
            Logger logger(4);
            logger.Info("test", "first occurrence", "shared.message");
            logger.Warning("test", "latest occurrence", "shared.message");

            const auto messages = logger.Snapshot();
            Assert::AreEqual(std::size_t{2}, messages.size());
            Assert::AreEqual(std::string{"first occurrence"}, messages[0].Content);
            Assert::IsTrue(messages[0].Level == LogLevel::Info);
            Assert::AreEqual(std::string{"latest occurrence"}, messages[1].Content);
            Assert::IsTrue(messages[1].Level == LogLevel::Warning);
        }

        TEST_METHOD(Log_StopsAppendingMessageAfterItsOccurrenceLimit)
        {
            Logger logger(4);
            logger.Info("test", "first", "limited.message", 2);
            logger.Info("test", "second", "limited.message", 2);
            logger.Info("test", "third", "limited.message", 2);

            const auto messages = logger.Snapshot();
            Assert::AreEqual(std::size_t{2}, messages.size());
            Assert::AreEqual(std::string{"first"}, messages[0].Content);
            Assert::AreEqual(std::string{"second"}, messages[1].Content);
            Assert::AreEqual(std::uint32_t{2}, messages[0].OccurrenceLimit);
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
            Assert::AreEqual(std::string{"second"}, messages[0].Content);
            Assert::AreEqual(std::string{"third"}, messages[1].Content);
            Assert::IsFalse(messages[0].MessageId.empty());
            Assert::IsTrue(messages[0].Sequence < messages[1].Sequence);
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

        TEST_METHOD(Log_ForwardsAcceptedMessageToWriter)
        {
            const auto writer = std::make_shared<RecordingLogWriter>();
            Logger logger(writer, 4);

            logger.Warning("example.plugin", "Something happened", "plugin.warning");

            Assert::AreEqual(std::size_t{1}, writer->Messages.size());
            Assert::AreEqual(std::string{"example.plugin"}, writer->Messages[0].Origin);
            Assert::AreEqual(std::string{"Something happened"}, writer->Messages[0].Content);
            Assert::AreEqual(std::string{"plugin.warning"}, writer->Messages[0].MessageId);
            Assert::IsTrue(writer->Messages[0].Level == LogLevel::Warning);
            Assert::AreEqual(std::uint64_t{1}, writer->Messages[0].Sequence);
        }

        TEST_METHOD(Log_DoesNotForwardMessageRejectedByOccurrenceLimit)
        {
            const auto writer = std::make_shared<RecordingLogWriter>();
            Logger logger(writer);

            logger.Info("test", "first", "limited.message", 1);
            logger.Info("test", "second", "limited.message", 1);

            Assert::AreEqual(std::size_t{1}, writer->Messages.size());
            Assert::AreEqual(std::string{"first"}, writer->Messages[0].Content);
        }
    };
}  // namespace PureMirror::Overlay::Tests
