// clang-format off
#include "pch.h"
// clang-format on

#include "PureMirror.Core.h"

#include "ClientListener.h"
#include "CustomerQueue.h"
#include "WindowInput.h"
#include "imgui_extension.h"

#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <utility>

const RendererAPI* g_rendererAPI;

ClientListener g_ClientListener;

CustomerQueue g_CustomerQueue;

std::unique_ptr<PureMirror::WindowInput> g_WindowInput;
std::atomic_bool g_CaptureMouse{false};

namespace
{
    bool IsSafeCharacterName(const std::string_view character)
    {
        return !character.empty() &&
               std::all_of(character.begin(),
                           character.end(),
                           [](const unsigned char value)
                           { return value > 0x20 && value != 0x7F && value != '/' && value != '@'; });
    }

    bool SendChatText(const std::string& text)
    {
        if (!g_WindowInput || !g_WindowInput->IsForeground())
            return false;

        if (!g_WindowInput->SendKey(VK_RETURN))
            return false;

        if (!g_WindowInput->SendKeyDown(VK_CONTROL))
            return false;
        const auto selected = g_WindowInput->SendKey('A');
        const auto controlReleased = g_WindowInput->SendKeyUp(VK_CONTROL);
        if (!selected || !controlReleased || !g_WindowInput->SendKey(VK_DELETE))
            return false;

        if (!g_WindowInput->SendTextUtf8(text) || !g_WindowInput->SendKey(VK_RETURN))
            return false;

        // Restore the previous chat entry, matching Awakened PoE Trade's sequence.
        return g_WindowInput->SendKey(VK_RETURN) && g_WindowInput->SendKey(VK_UP) && g_WindowInput->SendKey(VK_UP) &&
               g_WindowInput->SendKey(VK_ESCAPE);
    }

    bool OpenWhisper(const std::string& character)
    {
        if (!g_WindowInput || !g_WindowInput->IsForeground() || !IsSafeCharacterName(character))
            return false;

        if (!g_WindowInput->SendKey(VK_RETURN))
            return false;

        if (!g_WindowInput->SendKeyDown(VK_CONTROL))
            return false;
        const auto selected = g_WindowInput->SendKey('A');
        const auto controlReleased = g_WindowInput->SendKeyUp(VK_CONTROL);
        if (!selected || !controlReleased || !g_WindowInput->SendKey(VK_DELETE))
            return false;

        return g_WindowInput->SendTextUtf8("@" + character + " ");
    }

    std::string FormatWaitTime(const Customer& customer)
    {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - customer.QueuedAt)
                .count();
        const auto hours = elapsed / 3600;
        const auto minutes = (elapsed % 3600) / 60;
        const auto seconds = elapsed % 60;

        char buffer[32]{};
        if (hours > 0)
            std::snprintf(buffer, sizeof(buffer), "%lld:%02lld:%02lld", hours, minutes, seconds);
        else
            std::snprintf(buffer, sizeof(buffer), "%02lld:%02lld", minutes, seconds);
        return buffer;
    }

    void RenderCustomerName(const Customer& customer)
    {
        const auto textSize = ImGui::CalcTextSize(customer.Character.c_str());
        const auto clicked = ImGui::Selectable(
            customer.Character.c_str(), false, ImGuiSelectableFlags_None, ImVec2(textSize.x, ImGui::GetTextLineHeight()));
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (clicked)
        {
            ImGui::GetIO().WantCaptureMouse = true;
            ImGui::SetNextFrameWantCaptureMouse(true);
            OpenWhisper(customer.Character);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("History: %s", customer.Character.c_str());
            ImGui::TextDisabled("Click to whisper");
            ImGui::Separator();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            for (const auto& message : customer.Messages)
                ImGui::BulletText("%s", message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void RenderCustomers()
    {
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x * 0.5f, viewport->Size.y));
        ImGui::SetNextWindowBgAlpha(0.0f);
        constexpr auto windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("Customers", nullptr, windowFlags);
        auto& customers = g_CustomerQueue.Customers();
        ImGui::Text("CUSTOMERS (%zu)", customers.size());
        ImGui::Separator();

        enum class Action
        {
            None,
            Invite,
            OfferWaiting,
            Remove
        };
        Action action = Action::None;
        std::size_t actionIndex = 0;

        for (std::size_t index = 0; index < customers.size(); ++index)
        {
            const auto& customer = customers[index];
            ImGui::PushID(static_cast<int>(index));
            RenderCustomerName(customer);
            if (customer.WaitingOffered)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(waiting for yes)");
            }
            ImGui::TextDisabled("Position #%zu | waiting %s", index + 1, FormatWaitTime(customer).c_str());
            const auto safeName = IsSafeCharacterName(customer.Character);
            if (!safeName)
                ImGui::BeginDisabled();
            if (ImGui::Button("Invite"))
            {
                action = Action::Invite;
                actionIndex = index;
            }
            ImGui::SameLine();
            if (ImGui::Button("Full / wait"))
            {
                action = Action::OfferWaiting;
                actionIndex = index;
            }
            if (!safeName)
                ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                action = Action::Remove;
                actionIndex = index;
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        if (action != Action::None && actionIndex < customers.size())
        {
            const auto character = customers[actionIndex].Character;
            switch (action)
            {
            case Action::Invite:
                if (SendChatText("/invite " + character))
                    g_CustomerQueue.InviteCustomer(actionIndex);
                break;
            case Action::OfferWaiting:
                if (SendChatText("@" + character + " full sry, do you want to wait? reply yes"))
                    g_CustomerQueue.MarkWaitingOffered(actionIndex);
                break;
            case Action::Remove:
                g_CustomerQueue.RemoveCustomer(actionIndex);
                break;
            case Action::None:
                break;
            }
        }

        ImGui::End();
    }

    void RenderWaitingQueue()
    {
        const auto* viewport = ImGui::GetMainViewport();
        const auto leftWidth = viewport->Size.x * 0.5f;
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + leftWidth, viewport->Pos.y));
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x - leftWidth, viewport->Size.y));
        ImGui::SetNextWindowBgAlpha(0.0f);
        constexpr auto windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("Waiting Queue", nullptr, windowFlags);
        auto& waiting = g_CustomerQueue.Waiting();
        const auto waitingCount =
            std::count_if(waiting.begin(),
                          waiting.end(),
                          [](const Customer& customer) { return customer.State != CustomerState::Invited; });
        ImGui::Text("WAITING (%zu) | INVITED (%zu)", waitingCount, waiting.size() - waitingCount);
        ImGui::Separator();

        enum class Action
        {
            None,
            Invite,
            Kick,
            Trade,
            SendPosition,
            Remove
        };
        Action action = Action::None;
        std::size_t actionIndex = 0;

        // Every entry has two text rows, one button row and a separator. Anchor
        // that block to the bottom; reverse iteration keeps queue position #1
        // as the bottom-most entry while later positions grow upwards.
        const auto& style = ImGui::GetStyle();
        const auto entryHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f + ImGui::GetFrameHeightWithSpacing() +
                                 style.ItemSpacing.y + 1.0f;
        const auto entriesHeight = entryHeight * static_cast<float>(waiting.size());
        const auto availableHeight = ImGui::GetContentRegionAvail().y;
        if (entriesHeight < availableHeight)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availableHeight - entriesHeight);

        for (std::size_t index = waiting.size(); index-- > 0;)
        {
            const auto& customer = waiting[index];
            ImGui::PushID(static_cast<int>(index));
            RenderCustomerName(customer);
            ImGui::SameLine();
            ImGui::TextDisabled(customer.State == CustomerState::Invited ? "(invited)" : "(waiting)");
            if (const auto position = g_CustomerQueue.WaitingPosition(index))
                ImGui::TextDisabled("Position #%zu | waiting %s", *position, FormatWaitTime(customer).c_str());
            else
                ImGui::TextDisabled("Invited | waiting %s", FormatWaitTime(customer).c_str());
            const auto safeName = IsSafeCharacterName(customer.Character);
            if (!safeName)
                ImGui::BeginDisabled();
            if (customer.State == CustomerState::Invited)
            {
                if (ImGui::Button("Kick"))
                {
                    action = Action::Kick;
                    actionIndex = index;
                }
                ImGui::SameLine();
                if (ImGui::Button("Trade"))
                {
                    action = Action::Trade;
                    actionIndex = index;
                }
            }
            else
            {
                if (ImGui::Button("Invite"))
                {
                    action = Action::Invite;
                    actionIndex = index;
                }
                ImGui::SameLine();
                if (const auto position = g_CustomerQueue.WaitingPosition(index))
                {
                    const auto label = "Send position #" + std::to_string(*position);
                    if (ImGui::Button(label.c_str()))
                    {
                        action = Action::SendPosition;
                        actionIndex = index;
                    }
                }
            }
            if (!safeName)
                ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                action = Action::Remove;
                actionIndex = index;
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        if (action != Action::None && actionIndex < waiting.size())
        {
            const auto character = waiting[actionIndex].Character;
            switch (action)
            {
            case Action::Invite:
                if (SendChatText("/invite " + character))
                    g_CustomerQueue.InviteWaiting(actionIndex);
                break;
            case Action::Kick:
                if (SendChatText("/kick " + character))
                    g_CustomerQueue.RemoveWaiting(actionIndex);
                break;
            case Action::Trade:
                SendChatText("/tradewith " + character);
                break;
            case Action::SendPosition:
                if (const auto position = g_CustomerQueue.WaitingPosition(actionIndex))
                    SendChatText("@" + character + " you are #" + std::to_string(*position) + " in the waiting queue");
                break;
            case Action::Remove:
                g_CustomerQueue.RemoveWaiting(actionIndex);
                break;
            case Action::None:
                break;
            }
        }

        ImGui::End();
    }
}  // namespace

void Initialize(const RendererAPI* rendererAPI)
{
    g_rendererAPI = rendererAPI;
    g_WindowInput = std::make_unique<PureMirror::WindowInput>(g_rendererAPI->GetWindow());
}

const char* GetImguiVersion()
{
    return ImGui::GetVersion();
}

void SetContext(ImGuiContext* context)
{
    ImGui::SetCurrentContext(context);
}

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT HandleInput(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto result = ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

    if (result != 0)
        return result;

    auto& io = ImGui::GetIO();

    switch (uMsg)
    {
    case WM_KILLFOCUS:
        return 0;
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
        return 0;
    case WM_LBUTTONUP:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONUP:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEHWHEEL:
    case WM_MOUSEWHEEL:
    {
        return io.WantCaptureMouse;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        return io.WantCaptureKeyboard;
    }
    default:
        return 0;
    }

    return 0;
}

void Render(const RenderContext* renderContext)
{
    for (auto& message : g_ClientListener.TakeMessages())
        g_CustomerQueue.Process(std::move(message));

    RenderCustomers();
    RenderWaitingQueue();

    SetWindowVoidInputPassthrough(ImGui::FindWindowByName("Customers"));
    SetWindowVoidInputPassthrough(ImGui::FindWindowByName("Waiting Queue"));
}

static CoreAPI g_CoreAPI = MAKE_CORE_API(.Initialize = Initialize,
                                         .GetImguiVersion = GetImguiVersion,
                                         .SetContext = SetContext,
                                         .HandleInput = HandleInput,
                                         .Render = Render);

extern "C"
{
    PUREMIRRORCORE_API CoreAPI* GetCoreAPI(void)
    {
        return &g_CoreAPI;
    }
}
