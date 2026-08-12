#pragma once
// clang-format off
#include "pch.h"
// clang-format on

#include "LogFileListener.h"

struct ClientMessage
{
    std::string Character;
    std::string Text;
};

class ClientListener
{
  public:
    ClientListener() : m_listener(R"(D:\Spiele\Path of Exile\logs\LatestClient.txt)")
    {
        m_listener.SetErrorCallback([](const std::string& error) { std::cerr << "Log error: " << error << '\n'; });

        m_subscription = m_listener.Subscribe(
            std::regex{R"(@From\s+(?:<[^>]*>\s+)?([^:]+):\s*(.*)$)"},
            [this](const std::string&, const std::smatch& match)
            {
                std::scoped_lock lock(m_mutex);
                m_messages.push_back({.Character = match[1].str(), .Text = match[2].str()});
            });

        m_listener.Start();
    }

    ~ClientListener()
    {
        m_listener.Unsubscribe(m_subscription);
        m_listener.Stop();
    }

    std::vector<ClientMessage> TakeMessages()
    {
        std::scoped_lock lock(m_mutex);
        std::vector<ClientMessage> messages = std::move(m_messages);
        m_messages.clear();
        return messages;
    }

  private:
    PureMirror::LogFileListener m_listener;
    PureMirror::LogFileListener::SubscriptionId m_subscription{};
    std::mutex m_mutex;
    std::vector<ClientMessage> m_messages;
};
