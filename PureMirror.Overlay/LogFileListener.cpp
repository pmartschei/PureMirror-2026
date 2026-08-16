#include "pch.h"

#include "LogFileListener.h"

#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

namespace PureMirror
{
    LogFileListener::LogFileListener(std::filesystem::path path) : LogFileListener(std::move(path), Options{}) {}

    LogFileListener::LogFileListener(std::filesystem::path path, Options options)
        : m_Path(std::move(path)), m_Options(options)
    {
        if (m_Options.PollInterval <= std::chrono::milliseconds::zero())
            m_Options.PollInterval = std::chrono::milliseconds{1};
    }

    LogFileListener::~LogFileListener()
    {
        Stop();
    }

    LogFileListener::SubscriptionId LogFileListener::Subscribe(std::regex pattern, MatchCallback callback)
    {
        if (!callback)
            return 0;

        std::scoped_lock lock(m_SubscriptionsMutex);
        const auto id = m_NextSubscriptionId++;
        m_Subscriptions.emplace(id,
                                LogFileSubscription{.Pattern = std::move(pattern), .Callback = std::move(callback)});
        return id;
    }

    bool LogFileListener::Unsubscribe(const SubscriptionId id)
    {
        std::scoped_lock lock(m_SubscriptionsMutex);
        return m_Subscriptions.erase(id) != 0;
    }

    void LogFileListener::SetErrorCallback(ErrorCallback callback)
    {
        std::scoped_lock lock(m_SubscriptionsMutex);
        m_ErrorCallback = std::move(callback);
    }

    bool LogFileListener::Start()
    {
        bool expected = false;
        if (!m_Running.compare_exchange_strong(expected, true))
            return false;

        if (m_Worker.joinable())
            m_Worker.join();

        try
        {
            m_Worker = std::thread(&LogFileListener::Run, this);
        }
        catch (...)
        {
            m_Running = false;
            throw;
        }
        return true;
    }

    void LogFileListener::Stop()
    {
        m_Running = false;
        m_WaitCondition.notify_all();

        // A callback may request a stop. Joining is then deferred to its owner thread.
        if (m_Worker.joinable() && m_Worker.get_id() != std::this_thread::get_id())
            m_Worker.join();
    }

    bool LogFileListener::IsRunning() const noexcept
    {
        return m_Running;
    }

    void LogFileListener::Run()
    {
        std::uintmax_t offset = 0;
        std::string pending;
        bool errorReported = false;

        // Capture the position at listener startup. If the file does not exist yet,
        // offset zero ensures content created after Start() is not skipped.
        if (m_Options.InitialPosition == StartPosition::End)
        {
            std::error_code initialError;
            const auto initialSize = std::filesystem::file_size(m_Path, initialError);
            if (!initialError)
                offset = initialSize;
        }

        while (m_Running)
        {
            std::error_code error;
            const auto fileSize = std::filesystem::file_size(m_Path, error);
            if (error)
            {
                // The game may create the log after the listener has started.
                if (error != std::errc::no_such_file_or_directory && !errorReported)
                {
                    ReportError("Could not inspect log file: " + error.message());
                    errorReported = true;
                }
                if (WaitForNextPoll())
                    break;
                continue;
            }
            errorReported = false;

            if (fileSize < offset)
            {
                // The game truncated or rotated the log.
                offset = 0;
                pending.clear();
            }

            if (fileSize > offset)
            {
                std::ifstream stream(m_Path, std::ios::binary);
                if (!stream)
                {
                    if (!errorReported)
                    {
                        ReportError("Could not open log file for reading.");
                        errorReported = true;
                    }
                }
                else
                {
                    stream.seekg(static_cast<std::streamoff>(offset));
                    const auto bytesToRead = fileSize - offset;
                    std::string appended(static_cast<std::size_t>(bytesToRead), '\0');
                    stream.read(appended.data(), static_cast<std::streamsize>(appended.size()));
                    const auto bytesRead = static_cast<std::size_t>(stream.gcount());

                    if (bytesRead > 0)
                    {
                        appended.resize(bytesRead);
                        offset += bytesRead;
                        pending += appended;

                        std::size_t lineStart = 0;
                        auto newline = pending.find('\n', lineStart);
                        while (newline != std::string::npos)
                        {
                            auto line = pending.substr(lineStart, newline - lineStart);
                            if (!line.empty() && line.back() == '\r')
                                line.pop_back();
                            Dispatch(line);
                            lineStart = newline + 1;
                            newline = pending.find('\n', lineStart);
                        }
                        pending.erase(0, lineStart);
                    }
                }
            }

            if (WaitForNextPoll())
                break;
        }

        m_Running = false;
    }

    void LogFileListener::Dispatch(const std::string& line)
    {
        std::vector<LogFileSubscription> subscriptions;
        {
            std::scoped_lock lock(m_SubscriptionsMutex);
            subscriptions.reserve(m_Subscriptions.size());
            for (const auto& [id, subscription] : m_Subscriptions)
                subscriptions.push_back(subscription);
        }

        for (const auto& subscription : subscriptions)
        {
            std::smatch match;
            if (!std::regex_search(line, match, subscription.Pattern))
                continue;

            try
            {
                subscription.Callback(line, match);
            }
            catch (const std::exception& exception)
            {
                ReportError("Log callback failed: " + std::string(exception.what()));
            }
            catch (...)
            {
                ReportError("Log callback failed with an unknown exception.");
            }
        }
    }

    void LogFileListener::ReportError(const std::string& message)
    {
        ErrorCallback callback;
        {
            std::scoped_lock lock(m_SubscriptionsMutex);
            callback = m_ErrorCallback;
        }

        if (!callback)
            return;

        try
        {
            callback(message);
        }
        catch (...)
        {
            // Error handlers must not terminate the listener thread.
        }
    }

    bool LogFileListener::WaitForNextPoll()
    {
        std::unique_lock lock(m_WaitMutex);
        return m_WaitCondition.wait_for(lock, m_Options.PollInterval, [this] { return !m_Running.load(); });
    }
}  // namespace PureMirror
