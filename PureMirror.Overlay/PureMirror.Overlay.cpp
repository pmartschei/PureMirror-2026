#include "pch.h"

#include "PureMirror.Overlay.h"

#include "ClientListener.h"
#include "CustomerQueue.h"
#include "WindowInput.h"
#include "imgui_extension.h"
#include "src/core/commands/CommandRegistry.h"
#include "src/core/logger/Logger.h"
#include "src/plugins/PluginManager.h"
#include "src/scripting/OverlayScriptHost.h"
#include "src/scripting/angelscript/AngelScriptEngine.h"
#include "src/ui/MainMenuBar.h"
#include "src/ui/console/ConsoleWindow.h"

#include <imgui.h>

const RendererAPI* g_rendererAPI;

ClientListener g_ClientListener;

CustomerQueue g_CustomerQueue;

std::unique_ptr<PureMirror::WindowInput> g_WindowInput;

PureMirror::Overlay::Logger g_Logger;
PureMirror::Overlay::CommandRegistry g_ConsoleCommands;
PureMirror::Overlay::ConsoleWindow g_ConsoleWindow(g_Logger, g_ConsoleCommands);
std::unique_ptr<PureMirror::Overlay::OverlayScriptHost> g_ScriptHost;
std::unique_ptr<PureMirror::Overlay::AngelScriptEngine> g_ScriptEngine;
std::unique_ptr<PureMirror::Overlay::PluginManager> g_PluginManager;
std::unique_ptr<PureMirror::Overlay::MainMenuBar> g_MainMenuBar;
std::atomic_bool g_IsOverlayActive{true};

Texture g_BtnErrorTexture;
Texture g_BtnErrorPressedTexture;
Texture g_BtnSuccessTexture;
Texture g_BtnSuccessPressedTexture;
Texture g_BtnNormalTexture;
Texture g_BtnNormalPressedTexture;
Texture g_IconNotAllowedTexture;
Texture g_IconAddFileTexture;
Texture g_IconSpeechBubbleTexture;
Texture g_IconTradeTexture;

namespace
{
    constexpr ImVec2 QueueWindowSize{430.0f, 360.0f};

    std::filesystem::path OverlayModuleDirectory()
    {
        HMODULE module{};
        const auto address = reinterpret_cast<LPCWSTR>(std::addressof(g_Logger));
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                address,
                                &module))
            return std::filesystem::current_path();

        std::array<wchar_t, 32'768> path{};
        const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length == path.size())
            return std::filesystem::current_path();
        return std::filesystem::path(std::wstring_view(path.data(), length)).parent_path();
    }

    bool AnimatedButton(ImTextureID bgNormal,
                        ImTextureID bgPressed,
                        ImTextureID overlay,
                        ImVec2 size,
                        ImVec2 overlaySize,
                        const char* str_id)
    {
        ImVec2 pos = ImGui::GetCursorScreenPos();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        std::string id = "##";
        id += str_id;

        bool clicked = ImGui::InvisibleButton(id.c_str(), size);

        bool hoveredAndPressed = ImGui::IsItemActive() && ImGui::IsItemHovered();

        ImTextureID bgTexture = hoveredAndPressed ? bgPressed : bgNormal;

        drawList->AddImage(bgTexture, pos, ImVec2(pos.x + size.x, pos.y + size.y));

        if (overlay)
        {
            ImVec2 overlayPos =
                ImVec2(pos.x + (size.x - overlaySize.x) * 0.5f, pos.y + (size.y - overlaySize.y) * 0.5f);

            drawList->AddImage(overlay, overlayPos, ImVec2(overlayPos.x + overlaySize.x, overlayPos.y + overlaySize.y));
        }

        return clicked;
    }

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
        const auto hovered = ImGui::IsItemHovered();

        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (clicked)
        {
            OpenWhisper(customer.Character);
        }
        if (hovered)
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted("History:");
            ImGui::SameLine();
            ImGui::TextUnformatted(customer.Character.c_str());
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

    constexpr auto QueuePanelPadding = 8.0f;
    constexpr auto QueuePanelRounding = 8.0f;
    constexpr auto QueuePanelButtonSize = 32.0f;

    struct QueuePanelLayout
    {
        ImVec2 Cursor;
        float RowTop;
        float ContentHeight;
        float Height;
    };

    float QueuePanelContentHeight()
    {
        const auto textBlockHeight = ImGui::GetTextLineHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
        return textBlockHeight > QueuePanelButtonSize ? textBlockHeight : QueuePanelButtonSize;
    }

    float QueuePanelHeight()
    {
        return QueuePanelContentHeight() + QueuePanelPadding * 2.0f;
    }

    QueuePanelLayout BeginQueuePanel()
    {
        const auto cursor = ImGui::GetCursorPos();
        const auto minimum = ImGui::GetCursorScreenPos();
        const auto contentHeight = QueuePanelContentHeight();
        const auto height = contentHeight + QueuePanelPadding * 2.0f;
        const auto maximum = ImVec2(minimum.x + ImGui::GetContentRegionAvail().x, minimum.y + height);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(minimum, maximum, ImGui::GetColorU32(ImGuiCol_FrameBg), QueuePanelRounding);
        drawList->AddRect(minimum, maximum, ImGui::GetColorU32(ImGuiCol_Border), QueuePanelRounding);

        const auto rowTop = cursor.y + QueuePanelPadding;
        ImGui::SetCursorPos(ImVec2(cursor.x + QueuePanelPadding, rowTop));
        return {.Cursor = cursor, .RowTop = rowTop, .ContentHeight = contentHeight, .Height = height};
    }

    void SetQueuePanelButtonRow(const QueuePanelLayout& panel, const float buttonsWidth)
    {
        const auto verticalOffset = (panel.ContentHeight - QueuePanelButtonSize) * 0.5f;
        ImGui::SetCursorPos(
            ImVec2(ImGui::GetContentRegionMax().x - buttonsWidth - QueuePanelPadding, panel.RowTop + verticalOffset));
    }

    void EndQueuePanel(const QueuePanelLayout& panel)
    {
        ImGui::SetCursorPos(ImVec2(panel.Cursor.x, panel.Cursor.y + panel.Height + ImGui::GetStyle().ItemSpacing.y));
    }

    float TextButtonWidth(const char* label)
    {
        return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    }

    void SimulateCustomer()
    {
        static std::size_t nextCustomerNumber = 1;

        std::string character;
        bool characterExists = false;
        do
        {
            character = "\xE6\xB5\x81"
                        "\xE6\x94\xBE"
                        "\xE4\xB9\x8B"
                        "\xE8\xB7\xAF"
                        "\xE7\x8E\xA9"
                        "\xE5\xAE\xB6" +
                        std::to_string(nextCustomerNumber++);
            const auto hasCharacter = [&](const std::vector<Customer>& customers)
            {
                return std::any_of(customers.begin(),
                                   customers.end(),
                                   [&](const Customer& customer) { return customer.Character == character; });
            };
            characterExists = hasCharacter(g_CustomerQueue.Customers()) || hasCharacter(g_CustomerQueue.Waiting());
        } while (characterExists);

        g_CustomerQueue.Process({.Character = std::move(character), .Text = "Need uber elder (simulated customer)"});
    }

    void RenderCustomers()
    {
        const auto* viewport = ImGui::GetMainViewport();
        BeginQueueWindow("Customers", ImVec2(viewport->Pos.x + 20.0f, viewport->Pos.y + 80.0f));
        if (RenderMoveAnchor())
            ImGui::SameLine();
        auto& customers = g_CustomerQueue.Customers();
        if (ImGui::GetIO().KeyAlt)
        {
            if (ImGui::Button("+ Test Customer"))
                SimulateCustomer();

            ImGui::SameLine();
            constexpr auto clearLabel = "Clear All";
            const auto clearButtonWidth = TextButtonWidth(clearLabel);
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - clearButtonWidth);
            bool isEmpty = customers.empty();
            if (isEmpty)
                ImGui::BeginDisabled();
            if (ImGui::Button(clearLabel))
                g_CustomerQueue.ClearCustomers();
            if (isEmpty)
                ImGui::EndDisabled();
        }

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

            const auto panel = BeginQueuePanel();
            RenderCustomerName(customer);
            if (customer.WaitingOffered)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(waiting for yes)");
            }
            ImGui::SetCursorPosX(panel.Cursor.x + QueuePanelPadding);
            ImGui::TextDisabled("Position #%zu | waiting %s", index + 1, FormatWaitTime(customer).c_str());
            const auto safeName = IsSafeCharacterName(customer.Character);
            if (!safeName)
                ImGui::BeginDisabled();

            const auto buttonsWidth = QueuePanelButtonSize * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
            SetQueuePanelButtonRow(panel, buttonsWidth);
            if (AnimatedButton(g_BtnSuccessTexture.TextureID,
                               g_BtnSuccessPressedTexture.TextureID,
                               g_IconAddFileTexture.TextureID,
                               ImVec2(32, 32),
                               ImVec2(20, 20),
                               "Invite"))
            {
                action = Action::Invite;
                actionIndex = index;
            }
            ImGui::SameLine();

            if (AnimatedButton(g_BtnNormalTexture.TextureID,
                               g_BtnNormalPressedTexture.TextureID,
                               g_IconSpeechBubbleTexture.TextureID,
                               ImVec2(32, 32),
                               ImVec2(20, 20),
                               "Message"))
            {
                action = Action::OfferWaiting;
                actionIndex = index;
            }
            if (!safeName)
                ImGui::EndDisabled();

            ImGui::SameLine();

            if (AnimatedButton(g_BtnErrorTexture.TextureID,
                               g_BtnErrorPressedTexture.TextureID,
                               g_IconNotAllowedTexture.TextureID,
                               ImVec2(32, 32),
                               ImVec2(20, 20),
                               "Remove"))
            {
                action = Action::Remove;
                actionIndex = index;
            }
            EndQueuePanel(panel);
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
        RenderMoveAnchor();
        auto& waiting = g_CustomerQueue.Waiting();

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
            const auto panel = BeginQueuePanel();
            RenderCustomerName(customer);
            const auto position = g_CustomerQueue.WaitingPosition(index);
            ImGui::SetCursorPosX(panel.Cursor.x + QueuePanelPadding);
            ImGui::TextDisabled("Position #%zu | waiting %s", position.value_or(0), FormatWaitTime(customer).c_str());
            const auto safeName = IsSafeCharacterName(customer.Character);
            if (!safeName)
                ImGui::BeginDisabled();

            const auto buttonsWidth = QueuePanelButtonSize * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
            SetQueuePanelButtonRow(panel, buttonsWidth);
            if (AnimatedButton(g_BtnSuccessTexture.TextureID,
                               g_BtnSuccessPressedTexture.TextureID,
                               g_IconAddFileTexture.TextureID,
                               ImVec2(32, 32),
                               ImVec2(20, 20),
                               "Invite"))
            {
                action = Action::Invite;
                actionIndex = index;
            }
            ImGui::SameLine();
            if (position)
            {
                if (AnimatedButton(g_BtnNormalTexture.TextureID,
                                   g_BtnNormalPressedTexture.TextureID,
                                   g_IconSpeechBubbleTexture.TextureID,
                                   ImVec2(32, 32),
                                   ImVec2(20, 20),
                                   "Message"))
                {
                    action = Action::SendPosition;
                    actionIndex = index;
                }
            }
            if (!safeName)
                ImGui::EndDisabled();

            ImGui::SameLine();
            if (AnimatedButton(g_BtnErrorTexture.TextureID,
                               g_BtnErrorPressedTexture.TextureID,
                               g_IconNotAllowedTexture.TextureID,
                               ImVec2(32, 32),
                               ImVec2(20, 20),
                               "Remove"))
            {
                action = Action::Remove;
                actionIndex = index;
            }
            EndQueuePanel(panel);
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
        const auto entryHeight = QueuePanelHeight() + style.ItemSpacing.y;
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
            const auto panel = BeginQueuePanel();
            RenderCustomerName(customer);
            ImGui::SetCursorPosX(panel.Cursor.x + QueuePanelPadding);
            ImGui::TextDisabled("waiting %s", FormatWaitTime(customer).c_str());

            const auto safeName = IsSafeCharacterName(customer.Character);
            if (!safeName)
                ImGui::BeginDisabled();

            const auto buttonsWidth = QueuePanelButtonSize * 2.0f + ImGui::GetStyle().ItemSpacing.x;
            SetQueuePanelButtonRow(panel, buttonsWidth);

            if (AnimatedButton(g_BtnSuccessTexture.TextureID,
                               g_BtnSuccessPressedTexture.TextureID,
                               g_IconTradeTexture.TextureID,
                               ImVec2(32, 32),
                               ImVec2(20, 20),
                               "Trade"))
            {
                action = Action::Trade;
                actionIndex = index;
            }
            ImGui::SameLine();

            if (AnimatedButton(g_BtnErrorTexture.TextureID,
                               g_BtnErrorPressedTexture.TextureID,
                               g_IconNotAllowedTexture.TextureID,
                               ImVec2(32, 32),
                               ImVec2(20, 20),
                               "Kick"))
            {
                action = Action::Kick;
                actionIndex = index;
            }
            if (!safeName)
                ImGui::EndDisabled();
            EndQueuePanel(panel);
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
    g_IsOverlayActive.store(true, std::memory_order_release);
    g_WindowInput = std::make_unique<PureMirror::WindowInput>(g_rendererAPI->GetWindow());

    const auto clearRegistered = g_ConsoleCommands.Register({.Name = "clear",
                                                             .Description = "Clears all console messages.",
                                                             .Origin = "PureMirror.Overlay",
                                                             .Handler = [](std::string_view)
                                                             {
                                                                 g_Logger.Clear();
                                                                 return PureMirror::Overlay::CommandResult::Success();
                                                             }});
    const auto helpRegistered = g_ConsoleCommands.Register(
        {.Name = "help",
         .Description = "Lists all registered commands.",
         .Origin = "PureMirror.Overlay",
         .Handler = [](std::string_view)
         {
             std::string help = "Available commands:";
             for (const auto& command : g_ConsoleCommands.Commands())
                 help += "\n/" + command.Name + " - " + command.Description + " [" + command.Origin + ']';
             return PureMirror::Overlay::CommandResult::Success(std::move(help));
         }});
    static_cast<void>(clearRegistered);
    static_cast<void>(helpRegistered);
    const PureMirror::Overlay::LogOrigin overlayOrigin{.Type = PureMirror::Overlay::LogOriginType::Host,
                                                       .Identifier = "puremirror.overlay",
                                                       .DisplayName = "PureMirror.Overlay"};
    g_Logger.Info(overlayOrigin, "Console initialized. Type /help for available commands.", "console.ready");

    g_ScriptHost = std::make_unique<PureMirror::Overlay::OverlayScriptHost>(g_Logger);
    g_ScriptEngine = std::make_unique<PureMirror::Overlay::AngelScriptEngine>(g_ScriptHost.get());
    g_MainMenuBar.reset();
    g_PluginManager = std::make_unique<PureMirror::Overlay::PluginManager>(*g_ScriptEngine, g_Logger);
    g_MainMenuBar = std::make_unique<PureMirror::Overlay::MainMenuBar>(
        *g_PluginManager,
        g_Logger,
        OverlayModuleDirectory() / "puremirror",
        [] { g_IsOverlayActive.store(false, std::memory_order_release); });
    static_cast<void>(g_PluginManager->LoadStartupPlugins(g_MainMenuBar->PluginsRoot()));
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
    if (!g_IsOverlayActive.load(std::memory_order_acquire))
        return 0;

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
    if (!g_IsOverlayActive.load(std::memory_order_acquire))
        return;

    g_BtnErrorTexture = g_rendererAPI->LoadTexture("puremirror/btn_error.png");
    g_BtnErrorPressedTexture = g_rendererAPI->LoadTexture("puremirror/btn_error_pressed.png");
    g_BtnSuccessTexture = g_rendererAPI->LoadTexture("puremirror/btn_success.png");
    g_BtnSuccessPressedTexture = g_rendererAPI->LoadTexture("puremirror/btn_success_pressed.png");
    g_BtnNormalTexture = g_rendererAPI->LoadTexture("puremirror/btn_normal.png");
    g_BtnNormalPressedTexture = g_rendererAPI->LoadTexture("puremirror/btn_normal_pressed.png");
    g_IconNotAllowedTexture = g_rendererAPI->LoadTexture("puremirror/icon_not_allowed.png");
    g_IconAddFileTexture = g_rendererAPI->LoadTexture("puremirror/icon_add_file.png");
    g_IconSpeechBubbleTexture = g_rendererAPI->LoadTexture("puremirror/icon_speech_bubble.png");
    g_IconTradeTexture = g_rendererAPI->LoadTexture("puremirror/icon_trade.png");

    for (auto& message : g_ClientListener.TakeMessages())
        g_CustomerQueue.Process(std::move(message));

    g_MainMenuBar->Render();
    if (!g_IsOverlayActive.load(std::memory_order_acquire))
        return;

    RenderCustomers();
    RenderWaitingQueue();
    RenderInvitedCustomers();
    if (g_PluginManager)
        g_PluginManager->Render();
    g_ConsoleWindow.Render();

    PureMirror::ImGuiExtension::SetWindowsVoidInputPassthrough({ImGui::FindWindowByName("Customers"),
                                                                ImGui::FindWindowByName("Waiting Queue"),
                                                                ImGui::FindWindowByName("Invited Customers")});
}

static OverlayAPI g_OverlayAPI = MAKE_OVERLAY_API(.Initialize = Initialize,
                                                  .GetImguiVersion = GetImguiVersion,
                                                  .SetContext = SetContext,
                                                  .HandleInput = HandleInput,
                                                  .Render = Render);

extern "C"
{
    PUREMIRROROVERLAY_API OverlayAPI* GetOverlayAPI(void)
    {
        return &g_OverlayAPI;
    }
}
