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
        const LogOrigin TestOrigin{
            .Type = LogOriginType::Host, .Identifier = "puremirror.tests", .DisplayName = "PureMirror.Tests"};
        const LogOrigin ExamplePluginOrigin{
            .Type = LogOriginType::Plugin, .Identifier = "example.plugin", .DisplayName = "Example Plugin"};

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

            logger.Log(LogLevel::Warning, ExamplePluginOrigin, "Something happened", "plugin.warning", &color);

            const auto messages = logger.Snapshot();
            Assert::AreEqual(std::size_t{1}, messages.size());
            Assert::IsTrue(messages[0].Origin.Type == LogOriginType::Plugin);
            Assert::AreEqual(std::string{"example.plugin"}, messages[0].Origin.Identifier);
            Assert::AreEqual(std::string{"Example Plugin"}, messages[0].Origin.DisplayName);
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
            logger.Info(TestOrigin, "first occurrence", "shared.message");
            logger.Warning(TestOrigin, "latest occurrence", "shared.message");

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
            logger.Info(TestOrigin, "first", "limited.message", 2);
            logger.Info(TestOrigin, "second", "limited.message", 2);
            logger.Info(TestOrigin, "third", "limited.message", 2);

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
                logger.Info(TestOrigin, "occurrence", "default-limited.message");

            Assert::AreEqual(std::size_t{10}, logger.Size());
        }

        TEST_METHOD(Log_DropsOldestMessageAtCapacity)
        {
            Logger logger(2);
            logger.Info(TestOrigin, "first");
            logger.Info(TestOrigin, "second");
            logger.Info(TestOrigin, "third");

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
            logger.Error(TestOrigin, "failure", "clear.message", 1);

            logger.Clear();

            Assert::AreEqual(std::size_t{0}, logger.Size());

            logger.Error(TestOrigin, "failure", "clear.message", 1);
            Assert::AreEqual(std::size_t{1}, logger.Size());
        }

        TEST_METHOD(Log_ForwardsAcceptedMessageToWriter)
        {
            const auto writer = std::make_shared<RecordingLogWriter>();
            Logger logger(writer, 4);

            logger.Warning(ExamplePluginOrigin, "Something happened", "plugin.warning");

            Assert::AreEqual(std::size_t{1}, writer->Messages.size());
            Assert::IsTrue(writer->Messages[0].Origin == ExamplePluginOrigin);
            Assert::AreEqual(std::string{"Something happened"}, writer->Messages[0].Content);
            Assert::AreEqual(std::string{"plugin.warning"}, writer->Messages[0].MessageId);
            Assert::IsTrue(writer->Messages[0].Level == LogLevel::Warning);
            Assert::AreEqual(std::uint64_t{1}, writer->Messages[0].Sequence);
        }

        TEST_METHOD(Log_DoesNotForwardMessageRejectedByOccurrenceLimit)
        {
            const auto writer = std::make_shared<RecordingLogWriter>();
            Logger logger(writer);

            logger.Info(TestOrigin, "first", "limited.message", 1);
            logger.Info(TestOrigin, "second", "limited.message", 1);

            Assert::AreEqual(std::size_t{1}, writer->Messages.size());
            Assert::AreEqual(std::string{"first"}, writer->Messages[0].Content);
        }

        TEST_METHOD(Log_DistinguishesHostAndPluginWithTheSameDisplayName)
        {
            Logger logger;
            const LogOrigin hostOrigin{
                .Type = LogOriginType::Host, .Identifier = "puremirror.console", .DisplayName = "PureMirror.Console"};
            const LogOrigin pluginOrigin{.Type = LogOriginType::Plugin,
                                         .Identifier = "third-party.console",
                                         .DisplayName = "PureMirror.Console"};

            logger.Info(hostOrigin, "host message");
            logger.Info(pluginOrigin, "plugin message");

            const auto messages = logger.Snapshot();
            Assert::AreEqual(std::size_t{2}, messages.size());
            Assert::IsFalse(messages[0].Origin == messages[1].Origin);
            Assert::IsTrue(messages[0].Origin.Type == LogOriginType::Host);
            Assert::IsTrue(messages[1].Origin.Type == LogOriginType::Plugin);
            Assert::AreEqual(messages[0].Origin.DisplayName, messages[1].Origin.DisplayName);
        }
    };
}  // namespace PureMirror::Overlay::Tests
