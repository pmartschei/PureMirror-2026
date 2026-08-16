#pragma once

#include "LogFileListenerOptions.h"
#include "LogFileSubscription.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <unordered_map>

namespace PureMirror
{
    class LogFileListener final
    {
      public:
        using StartPosition = LogFileStartPosition;
        using Options = LogFileListenerOptions;
        using SubscriptionId = std::uint64_t;
        using MatchCallback = LogFileMatchCallback;
        using ErrorCallback = std::function<void(const std::string& message)>;

        explicit LogFileListener(std::filesystem::path path);
        LogFileListener(std::filesystem::path path, Options options);
        ~LogFileListener();

        LogFileListener(const LogFileListener&) = delete;
        LogFileListener& operator=(const LogFileListener&) = delete;
        LogFileListener(LogFileListener&&) = delete;
        LogFileListener& operator=(LogFileListener&&) = delete;

        // Callbacks run synchronously on the listener's worker thread.
        SubscriptionId Subscribe(std::regex pattern, MatchCallback callback);
        bool Unsubscribe(SubscriptionId id);
        void SetErrorCallback(ErrorCallback callback);

        // Missing files are not an error: the listener waits until the file exists.
        bool Start();
        void Stop();
        [[nodiscard]] bool IsRunning() const noexcept;

      private:
        void Run();
        void Dispatch(const std::string& line);
        void ReportError(const std::string& message);
        bool WaitForNextPoll();

        std::filesystem::path m_Path;
        Options m_Options;
        std::atomic_bool m_Running{};
        std::thread m_Worker;
        std::mutex m_WaitMutex;
        std::condition_variable m_WaitCondition;

        std::mutex m_SubscriptionsMutex;
        std::unordered_map<SubscriptionId, LogFileSubscription> m_Subscriptions;
        SubscriptionId m_NextSubscriptionId{1};
        ErrorCallback m_ErrorCallback;
    };
}  // namespace PureMirror
