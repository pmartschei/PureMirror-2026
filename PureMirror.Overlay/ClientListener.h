#pragma once

#include "ClientMessage.h"
#include "LogFileListener.h"

#include <mutex>
#include <vector>

class ClientListener
{
  public:
    ClientListener();
    ~ClientListener();

    [[nodiscard]] std::vector<ClientMessage> TakeMessages();

  private:
    PureMirror::LogFileListener m_Listener;
    PureMirror::LogFileListener::SubscriptionId m_Subscription{};
    std::mutex m_Mutex;
    std::vector<ClientMessage> m_Messages;
};
