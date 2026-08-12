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

namespace
{
    constexpr ImVec2 QueueWindowSize{430.0f, 360.0f};

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
        const auto clicked = ImGui::Selectable(customer.Character.c_str(),
                                               false,
                                               ImGuiSelectableFlags_None,
                                               ImVec2(textSize.x, ImGui::GetTextLineHeight()));
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (clicked)
        {
            // ImGui::GetIO().WantCaptureMouse = true;
            // ImGui::SetNextFrameWantCaptureMouse(true);
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

    void BeginQueueWindow(const char* name, const ImVec2 defaultPosition)
    {
        ImGui::SetNextWindowPos(defaultPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(QueueWindowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.0f);
        auto windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoBackground;
        if (!ImGui::GetIO().KeyAlt)
            windowFlags |= ImGuiWindowFlags_NoResize;
        ImGui::Begin(name, nullptr, windowFlags);
    }

    bool RenderMoveAnchor()
    {
        if (!ImGui::GetIO().KeyAlt)
            return false;

        constexpr ImVec2 anchorSize{22.0f, 22.0f};
        ImGui::InvisibleButton("##move-anchor", anchorSize);

        const auto minimum = ImGui::GetItemRectMin();
        const auto maximum = ImGui::GetItemRectMax();
        const auto color = ImGui::IsItemActive()    ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                           : ImGui::IsItemHovered() ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                                    : ImGui::GetColorU32(ImGuiCol_Button);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(minimum, maximum, color, 4.0f);
        const auto lineColor = ImGui::GetColorU32(ImGuiCol_Text);
        for (float offset = 6.0f; offset <= 14.0f; offset += 4.0f)
            drawList->AddLine(ImVec2(minimum.x + offset, minimum.y + 5.0f),
                              ImVec2(minimum.x + offset, maximum.y - 5.0f),
                              lineColor,
                              1.0f);

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            const auto position = ImGui::GetWindowPos();
            const auto delta = ImGui::GetIO().MouseDelta;
            ImGui::SetWindowPos(ImVec2(position.x + delta.x, position.y + delta.y));
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drag to move");
        return true;
    }

    void RenderCustomers()
    {
        const auto* viewport = ImGui::GetMainViewport();
        BeginQueueWindow("Customers", ImVec2(viewport->Pos.x + 20.0f, viewport->Pos.y + 80.0f));
        if (RenderMoveAnchor())
            ImGui::SameLine();
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
        BeginQueueWindow(
            "Waiting Queue",
            ImVec2(viewport->Pos.x + (viewport->Size.x - QueueWindowSize.x) * 0.5f, viewport->Pos.y + 80.0f));
        if (RenderMoveAnchor())
            ImGui::SameLine();
        auto& waiting = g_CustomerQueue.Waiting();
        const auto waitingCount =
            std::count_if(waiting.begin(),
                          waiting.end(),
                          [](const Customer& customer) { return customer.State != CustomerState::Invited; });
        ImGui::Text("WAITING (%zu)", waitingCount);
        ImGui::Separator();

        enum class Action
        {
            None,
            Invite,
            SendPosition,
            Remove
        };
        Action action = Action::None;
        std::size_t actionIndex = 0;

        for (std::size_t index = 0; index < waiting.size(); ++index)
        {
            const auto& customer = waiting[index];
            if (customer.State == CustomerState::Invited)
                continue;

            ImGui::PushID(static_cast<int>(index));
            RenderCustomerName(customer);
            ImGui::SameLine();
            ImGui::TextDisabled("(waiting)");
            const auto position = g_CustomerQueue.WaitingPosition(index);
            ImGui::TextDisabled("Position #%zu | waiting %s", position.value_or(0), FormatWaitTime(customer).c_str());
            const auto safeName = IsSafeCharacterName(customer.Character);
            if (!safeName)
                ImGui::BeginDisabled();
            if (ImGui::Button("Invite"))
            {
                action = Action::Invite;
                actionIndex = index;
            }
            ImGui::SameLine();
            if (position)
            {
                const auto label = "Send position #" + std::to_string(*position);
                if (ImGui::Button(label.c_str()))
                {
                    action = Action::SendPosition;
                    actionIndex = index;
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

    void RenderInvitedCustomers()
    {
        const auto* viewport = ImGui::GetMainViewport();
        BeginQueueWindow(
            "Invited Customers",
            ImVec2(viewport->Pos.x + viewport->Size.x - QueueWindowSize.x - 20.0f, viewport->Pos.y + 80.0f));
        RenderMoveAnchor();

        auto& waiting = g_CustomerQueue.Waiting();
        const auto invitedCount =
            std::count_if(waiting.begin(),
                          waiting.end(),
                          [](const Customer& customer) { return customer.State == CustomerState::Invited; });
        enum class Action
        {
            None,
            Kick,
            Trade,
            Remove
        };
        Action action = Action::None;
        std::size_t actionIndex = 0;

        // Keep the first invited customer at the bottom and grow the list
        // upwards. The panel deliberately has no visible title.
        const auto& style = ImGui::GetStyle();
        const auto entryHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f + ImGui::GetFrameHeightWithSpacing() +
                                 style.ItemSpacing.y + 1.0f;
        const auto entriesHeight = entryHeight * static_cast<float>(invitedCount);
        const auto availableHeight = ImGui::GetContentRegionAvail().y;
        if (entriesHeight < availableHeight)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + availableHeight - entriesHeight);

        for (std::size_t index = waiting.size(); index-- > 0;)
        {
            const auto& customer = waiting[index];
            if (customer.State != CustomerState::Invited)
                continue;

            ImGui::PushID(static_cast<int>(index));
            RenderCustomerName(customer);
            ImGui::SameLine();
            ImGui::TextDisabled("(invited)");
            ImGui::TextDisabled("Invited | waiting %s", FormatWaitTime(customer).c_str());

            const auto safeName = IsSafeCharacterName(customer.Character);
            if (!safeName)
                ImGui::BeginDisabled();
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
            case Action::Kick:
                if (SendChatText("/kick " + character))
                    g_CustomerQueue.RemoveWaiting(actionIndex);
                break;
            case Action::Trade:
                SendChatText("/tradewith " + character);
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
    RenderInvitedCustomers();

    PureMirror::ImGuiExtension::SetWindowsVoidInputPassthrough({ImGui::FindWindowByName("Customers"),
                                                                ImGui::FindWindowByName("Waiting Queue"),
                                                                ImGui::FindWindowByName("Invited Customers")});
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
