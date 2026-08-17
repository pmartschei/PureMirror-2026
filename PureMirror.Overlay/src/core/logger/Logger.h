#pragma once

#include "ILogWriter.h"
#include "LogColor.h"
#include "LogLevel.h"
#include "LogMessage.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PureMirror::Overlay
{
    class Logger
    {
      public:
        explicit Logger(std::size_t capacity = 2'000);
        explicit Logger(std::shared_ptr<ILogWriter> writer, std::size_t capacity = 2'000);

        void Log(LogLevel level,
                 const LogOrigin& origin,
                 std::string_view content,
                 std::string_view messageId = {},
                 const LogColor* color = nullptr,
                 std::uint32_t occurrenceLimit = LogMessage::DefaultOccurrenceLimit);

        void Trace(const LogOrigin& origin,
                   std::string_view content,
                   std::string_view messageId = {},
                   std::uint32_t occurrenceLimit = LogMessage::DefaultOccurrenceLimit);
        void Debug(const LogOrigin& origin,
                   std::string_view content,
                   std::string_view messageId = {},
                   std::uint32_t occurrenceLimit = LogMessage::DefaultOccurrenceLimit);
        void Info(const LogOrigin& origin,
                  std::string_view content,
                  std::string_view messageId = {},
                  std::uint32_t occurrenceLimit = LogMessage::DefaultOccurrenceLimit);
        void Warning(const LogOrigin& origin,
                     std::string_view content,
                     std::string_view messageId = {},
                     std::uint32_t occurrenceLimit = LogMessage::DefaultOccurrenceLimit);
        void Error(const LogOrigin& origin,
                   std::string_view content,
                   std::string_view messageId = {},
                   std::uint32_t occurrenceLimit = LogMessage::DefaultOccurrenceLimit);

        [[nodiscard]] std::vector<LogMessage> Snapshot() const;
        [[nodiscard]] std::size_t Size() const;
        [[nodiscard]] std::size_t Capacity() const;
        void Clear();

        [[nodiscard]] static LogColor DefaultColor(LogLevel level);
        [[nodiscard]] static const char* LevelName(LogLevel level);

      private:
        const std::size_t m_Capacity;
        const std::shared_ptr<ILogWriter> m_Writer;
        std::mutex m_WriteMutex;
        mutable std::mutex m_Mutex;
        std::deque<LogMessage> m_Messages;
        std::unordered_map<std::string, std::uint32_t> m_OccurrenceCounts;
        std::uint64_t m_NextSequence{1};
    };
}  // namespace PureMirror::Overlay
