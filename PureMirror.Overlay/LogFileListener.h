#pragma once

// clang-format off
#include "pch.h"
// clang-format on

namespace PureMirror
{
    class LogFileListener final
    {
      public:
        enum class StartPosition
        {
            End,
            Beginning
        };

        struct Options
        {
            std::chrono::milliseconds PollInterval{100};
            StartPosition InitialPosition{StartPosition::End};
        };

        using SubscriptionId = std::uint64_t;
        using MatchCallback = std::function<void(const std::string& line, const std::smatch& match)>;
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
        struct Subscription
        {
            std::regex Pattern;
            MatchCallback Callback;
        };

        void Run();
        void Dispatch(const std::string& line);
        void ReportError(const std::string& message);
        bool WaitForNextPoll();

        std::filesystem::path m_path;
        Options m_options;
        std::atomic_bool m_running{false};
        std::thread m_worker;
        std::mutex m_waitMutex;
        std::condition_variable m_waitCondition;

        std::mutex m_subscriptionsMutex;
        std::unordered_map<SubscriptionId, Subscription> m_subscriptions;
        SubscriptionId m_nextSubscriptionId{1};
        ErrorCallback m_errorCallback;
    };
}  // namespace PureMirror
