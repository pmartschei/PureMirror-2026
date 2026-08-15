// clang-format off
#include "pch.h"
// clang-format on

#include "CustomerQueue.h"

#include <algorithm>
#include <cctype>
#include <utility>

void CustomerQueue::Process(ClientMessage message)
{
    message.Character = Trim(std::move(message.Character));
    message.Text = Trim(std::move(message.Text));
    if (message.Character.empty() || message.Text.empty())
        return;

    if (auto* customer = Find(message.Character))
    {
        customer->Messages.push_back(message.Text);
        if (customer->State == CustomerState::Customer && customer->WaitingOffered &&
            EqualsIgnoreCase(message.Text, "yes"))
        {
            const auto index = static_cast<std::size_t>(customer - m_customers.data());
            MoveToWaiting(index, CustomerState::Waiting);
        }
        return;
    }

    if (ContainsIgnoreCase(message.Text, "Need uber elder"))
    {
        m_customers.push_back({.Character = std::move(message.Character),
                               .Messages = {std::move(message.Text)},
                               .State = CustomerState::Customer});
    }
}

std::vector<Customer>& CustomerQueue::Customers() noexcept
{
    return m_customers;
}

std::vector<Customer>& CustomerQueue::Waiting() noexcept
{
    return m_waiting;
}

void CustomerQueue::MarkWaitingOffered(const std::size_t customerIndex)
{
    if (customerIndex < m_customers.size())
        m_customers[customerIndex].WaitingOffered = true;
}

void CustomerQueue::InviteCustomer(const std::size_t customerIndex)
{
    MoveToWaiting(customerIndex, CustomerState::Invited);
}

void CustomerQueue::InviteWaiting(const std::size_t waitingIndex)
{
    if (waitingIndex < m_waiting.size())
        m_waiting[waitingIndex].State = CustomerState::Invited;
}

void CustomerQueue::RemoveCustomer(const std::size_t customerIndex)
{
    if (customerIndex < m_customers.size())
        m_customers.erase(m_customers.begin() + static_cast<std::ptrdiff_t>(customerIndex));
}

void CustomerQueue::RemoveWaiting(const std::size_t waitingIndex)
{
    if (waitingIndex < m_waiting.size())
        m_waiting.erase(m_waiting.begin() + static_cast<std::ptrdiff_t>(waitingIndex));
}

void CustomerQueue::ClearCustomers() noexcept
{
    m_customers.clear();
}

std::optional<std::size_t> CustomerQueue::WaitingPosition(const std::size_t waitingIndex) const
{
    if (waitingIndex >= m_waiting.size() || m_waiting[waitingIndex].State == CustomerState::Invited)
        return std::nullopt;

    std::size_t position = 0;
    for (std::size_t index = 0; index <= waitingIndex; ++index)
    {
        if (m_waiting[index].State != CustomerState::Invited)
            ++position;
    }
    return position;
}

bool CustomerQueue::EqualsIgnoreCase(const std::string_view left, const std::string_view right)
{
    if (left.size() != right.size())
        return false;

    return std::equal(
        left.begin(),
        left.end(),
        right.begin(),
        [](const char a, const char b)
        { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
}

bool CustomerQueue::ContainsIgnoreCase(const std::string_view text, const std::string_view needle)
{
    return std::search(text.begin(),
                       text.end(),
                       needle.begin(),
                       needle.end(),
                       [](const char a, const char b) {
                           return std::tolower(static_cast<unsigned char>(a)) ==
                                  std::tolower(static_cast<unsigned char>(b));
                       }) != text.end();
}

std::string CustomerQueue::Trim(std::string text)
{
    const auto isSpace = [](const unsigned char character) { return std::isspace(character) != 0; };
    text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
    text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
    return text;
}

Customer* CustomerQueue::Find(const std::string_view character)
{
    const auto findIn = [&](auto& customers) -> Customer*
    {
        const auto found =
            std::find_if(customers.begin(),
                         customers.end(),
                         [&](const Customer& customer) { return EqualsIgnoreCase(customer.Character, character); });
        return found == customers.end() ? nullptr : &*found;
    };

    if (auto* customer = findIn(m_customers))
        return customer;
    return findIn(m_waiting);
}

void CustomerQueue::MoveToWaiting(const std::size_t customerIndex, const CustomerState state)
{
    if (customerIndex >= m_customers.size())
        return;

    auto customer = std::move(m_customers[customerIndex]);
    customer.State = state;
    customer.WaitingOffered = false;
    m_customers.erase(m_customers.begin() + static_cast<std::ptrdiff_t>(customerIndex));
    m_waiting.push_back(std::move(customer));
}
