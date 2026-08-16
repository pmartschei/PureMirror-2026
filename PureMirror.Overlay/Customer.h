#pragma once

#include "CustomerState.h"

#include <chrono>
#include <string>
#include <vector>

struct Customer
{
    std::string Character;
    std::vector<std::string> Messages;
    CustomerState State{CustomerState::Customer};
    bool WaitingOffered{};
    std::chrono::steady_clock::time_point QueuedAt{std::chrono::steady_clock::now()};
};
