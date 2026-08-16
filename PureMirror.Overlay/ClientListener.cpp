#include "pch.h"

#include "ClientListener.h"

#include <iostream>
#include <regex>
#include <string>
#include <utility>

ClientListener::ClientListener() : m_Listener(R"(D:\Spiele\Path of Exile\logs\LatestClient.txt)")
{
    m_Listener.SetErrorCallback([](const std::string& error) { std::cerr << "Log error: " << error << '\n'; });

    m_Subscription =
        m_Listener.Subscribe(std::regex{R"(@From\s+(?:<[^>]*>\s+)?([^:]+):\s*(.*)$)"},
                             [this](const std::string&, const std::smatch& match)
                             {
                                 std::scoped_lock lock(m_Mutex);
                                 m_Messages.push_back({.Character = match[1].str(), .Text = match[2].str()});
                             });

    m_Listener.Start();
}

ClientListener::~ClientListener()
{
    m_Listener.Unsubscribe(m_Subscription);
    m_Listener.Stop();
}

std::vector<ClientMessage> ClientListener::TakeMessages()
{
    std::scoped_lock lock(m_Mutex);
    auto messages = std::move(m_Messages);
    m_Messages.clear();
    return messages;
}
