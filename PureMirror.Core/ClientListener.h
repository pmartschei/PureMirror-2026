#pragma once
// clang-format off
#include "pch.h"
// clang-format on

#include "LogFileListener.h"

class ClientListener
{
  public:
    ClientListener() : m_listener(R"(D:\Spiele\Path of Exile\logs\LatestClient.txt)")
    {
        m_listener.SetErrorCallback([](const std::string& error) { std::cerr << "Log error: " << error << '\n'; });

        m_subscription = m_listener.Subscribe(
            std::regex{R"(@From\s+(?:<[^>]*>\s+)?([^:]+):\s*(.*\bNeed uber elder\b.*)$)", std::regex::icase},
            [this](const std::string& completeLine, const std::smatch& match)
            {
                const std::string player = match[1].str();
                const std::string message = match[2].str();

                // Callback läuft auf dem Listener-Thread.
                std::scoped_lock lock(m_mutex);
                m_messages.push_back(player + ": " + message);
            });

        m_listener.Start();
    }

    ~ClientListener()
    {
        m_listener.Unsubscribe(m_subscription);
        m_listener.Stop();
    }

    std::vector<std::string> TakeMessages()
    {
        std::scoped_lock lock(m_mutex);
        std::vector<std::string> messages = std::move(m_messages);
        m_messages.clear();
        return messages;
    }

  private:
    PureMirror::LogFileListener m_listener;
    PureMirror::LogFileListener::SubscriptionId m_subscription{};
    std::mutex m_mutex;
    std::vector<std::string> m_messages;
};
