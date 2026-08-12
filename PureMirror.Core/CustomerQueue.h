#pragma once

#include "ClientListener.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class CustomerState
{
    Customer,
    Waiting,
    Invited
};

struct Customer
{
    std::string Character;
    std::vector<std::string> Messages;
    CustomerState State{CustomerState::Customer};
    bool WaitingOffered{false};
    std::chrono::steady_clock::time_point QueuedAt{std::chrono::steady_clock::now()};
};

class CustomerQueue final
{
  public:
    void Process(ClientMessage message);

    [[nodiscard]] std::vector<Customer>& Customers() noexcept;
    [[nodiscard]] std::vector<Customer>& Waiting() noexcept;

    void MarkWaitingOffered(std::size_t customerIndex);
    void InviteCustomer(std::size_t customerIndex);
    void InviteWaiting(std::size_t waitingIndex);
    void RemoveCustomer(std::size_t customerIndex);
    void RemoveWaiting(std::size_t waitingIndex);

    [[nodiscard]] std::optional<std::size_t> WaitingPosition(std::size_t waitingIndex) const;

  private:
    static bool EqualsIgnoreCase(std::string_view left, std::string_view right);
    static bool ContainsIgnoreCase(std::string_view text, std::string_view needle);
    static std::string Trim(std::string text);
    Customer* Find(std::string_view character);
    void MoveToWaiting(std::size_t customerIndex, CustomerState state);

    std::vector<Customer> m_customers;
    std::vector<Customer> m_waiting;
};
