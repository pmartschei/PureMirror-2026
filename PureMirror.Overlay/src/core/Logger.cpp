#include "pch.h"

#include "Logger.h"

namespace PureMirror::Overlay
{
    Logger::Logger(const std::size_t capacity) : Logger(nullptr, capacity) {}

    Logger::Logger(std::shared_ptr<ILogWriter> writer, const std::size_t capacity)
        : m_Capacity(std::max<std::size_t>(capacity, 1)), m_Writer(std::move(writer))
    {
    }

    void Logger::Log(const LogLevel level,
                     const std::string_view origin,
                     const std::string_view content,
                     const std::string_view messageId,
                     const LogColor* color,
                     const std::uint32_t occurrenceLimit)
    {
        std::scoped_lock writeLock(m_WriteMutex);

        LogMessage message{.MessageId = std::string(messageId),
                           .Timestamp = std::chrono::system_clock::now(),
                           .Content = std::string(content),
                           .Level = level,
                           .Color = color ? *color : DefaultColor(level),
                           .Origin = origin.empty() ? "PureMirror" : std::string(origin),
                           .OccurrenceLimit = occurrenceLimit};

        {
            std::scoped_lock lock(m_Mutex);
            if (!message.MessageId.empty())
            {
                auto& occurrenceCount = m_OccurrenceCounts[message.MessageId];
                if (occurrenceCount >= message.OccurrenceLimit)
                    return;
                ++occurrenceCount;
            }

            message.Sequence = m_NextSequence++;
            if (message.MessageId.empty())
                message.MessageId = "log." + std::to_string(message.Sequence);

            if (m_Messages.size() == m_Capacity)
                m_Messages.pop_front();
            m_Messages.push_back(message);
        }

        if (m_Writer)
            m_Writer->Write(message);
    }

    void Logger::Trace(const std::string_view origin,
                       const std::string_view content,
                       const std::string_view messageId,
                       const std::uint32_t occurrenceLimit)
    {
        Log(LogLevel::Trace, origin, content, messageId, nullptr, occurrenceLimit);
    }

    void Logger::Debug(const std::string_view origin,
                       const std::string_view content,
                       const std::string_view messageId,
                       const std::uint32_t occurrenceLimit)
    {
        Log(LogLevel::Debug, origin, content, messageId, nullptr, occurrenceLimit);
    }

    void Logger::Info(const std::string_view origin,
                      const std::string_view content,
                      const std::string_view messageId,
                      const std::uint32_t occurrenceLimit)
    {
        Log(LogLevel::Info, origin, content, messageId, nullptr, occurrenceLimit);
    }

    void Logger::Warning(const std::string_view origin,
                         const std::string_view content,
                         const std::string_view messageId,
                         const std::uint32_t occurrenceLimit)
    {
        Log(LogLevel::Warning, origin, content, messageId, nullptr, occurrenceLimit);
    }

    void Logger::Error(const std::string_view origin,
                       const std::string_view content,
                       const std::string_view messageId,
                       const std::uint32_t occurrenceLimit)
    {
        Log(LogLevel::Error, origin, content, messageId, nullptr, occurrenceLimit);
    }

    std::vector<LogMessage> Logger::Snapshot() const
    {
        std::scoped_lock lock(m_Mutex);
        return {m_Messages.begin(), m_Messages.end()};
    }

    std::size_t Logger::Size() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Messages.size();
    }

    std::size_t Logger::Capacity() const
    {
        return m_Capacity;
    }

    void Logger::Clear()
    {
        std::scoped_lock lock(m_Mutex);
        m_Messages.clear();
        m_OccurrenceCounts.clear();
    }

    LogColor Logger::DefaultColor(const LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Trace:
            return {0.62f, 0.62f, 0.68f, 1.0f};
        case LogLevel::Debug:
            return {0.45f, 0.72f, 1.0f, 1.0f};
        case LogLevel::Info:
            return {0.88f, 0.88f, 0.90f, 1.0f};
        case LogLevel::Warning:
            return {1.0f, 0.72f, 0.20f, 1.0f};
        case LogLevel::Error:
            return {1.0f, 0.32f, 0.32f, 1.0f};
        }
        return {};
    }

    const char* Logger::LevelName(const LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        }
        return "UNKNOWN";
    }
}  // namespace PureMirror::Overlay
