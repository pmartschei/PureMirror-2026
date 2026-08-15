// clang-format off
#include "pch.h"
// clang-format on

#include "WindowInput.h"

#include <RendererApi.h>
#include <iterator>
#include <string>

namespace PureMirror
{
    WindowInput::WindowInput(const HWND window) noexcept : m_window(window) {}

    void WindowInput::SetWindow(const HWND window) noexcept
    {
        m_window = window;
    }

    HWND WindowInput::GetWindow() const noexcept
    {
        return m_window;
    }

    bool WindowInput::IsAvailable() const noexcept
    {
        return m_window != nullptr && IsWindow(m_window);
    }

    bool WindowInput::IsForeground() const noexcept
    {
        if (!IsAvailable())
            return false;

        const auto foreground = GetForegroundWindow();
        return foreground == m_window || GetAncestor(foreground, GA_ROOT) == m_window;
    }

    bool WindowInput::SendKey(const UINT virtualKey) const
    {
        if (!CanSend())
            return false;

        INPUT inputs[] = {CreateKeyInput(virtualKey, false), CreateKeyInput(virtualKey, true)};
        return SendInput(static_cast<UINT>(std::size(inputs)), inputs, sizeof(INPUT)) == std::size(inputs);
    }

    bool WindowInput::SendKeyDown(const UINT virtualKey) const
    {
        if (!CanSend())
            return false;

        auto input = CreateKeyInput(virtualKey, false);
        return SendInput(1, &input, sizeof(INPUT)) == 1;
    }

    bool WindowInput::SendKeyUp(const UINT virtualKey) const
    {
        if (!CanSend())
            return false;

        auto input = CreateKeyInput(virtualKey, true);
        return SendInput(1, &input, sizeof(INPUT)) == 1;
    }

    bool WindowInput::SendText(const std::wstring_view text) const
    {
        if (!CanSend())
            return false;

        for (const auto character : text)
        {
            INPUT inputs[] = {CreateCharacterInput(character, false), CreateCharacterInput(character, true)};
            if (SendInput(static_cast<UINT>(std::size(inputs)), inputs, sizeof(INPUT)) != std::size(inputs))
                return false;
        }
        return true;
    }

    bool WindowInput::SendTextUtf8(const std::string_view text) const
    {
        if (text.empty())
            return true;

        const auto inputLength = static_cast<int>(text.size());
        const auto outputLength =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), inputLength, nullptr, 0);
        if (outputLength == 0)
            return false;

        std::wstring wideText(static_cast<std::size_t>(outputLength), L'\0');
        if (MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), inputLength, wideText.data(), outputLength) == 0)
            return false;

        return SendText(wideText);
    }

    INPUT WindowInput::CreateKeyInput(const UINT virtualKey, const bool released)
    {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));
        input.ki.dwFlags = KEYEVENTF_SCANCODE;
        input.ki.dwExtraInfo = DIRECT_GAME_INPUT_MARKER;

        switch (virtualKey)
        {
        case VK_RMENU:
        case VK_RCONTROL:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_DIVIDE:
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            break;
        }

        if (released)
            input.ki.dwFlags |= KEYEVENTF_KEYUP;

        return input;
    }

    INPUT WindowInput::CreateCharacterInput(const wchar_t character, const bool released)
    {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = character;
        input.ki.dwFlags = KEYEVENTF_UNICODE | (released ? KEYEVENTF_KEYUP : 0);
        input.ki.dwExtraInfo = DIRECT_GAME_INPUT_MARKER;
        return input;
    }

    bool WindowInput::CanSend() const noexcept
    {
        return IsForeground();
    }
}  // namespace PureMirror
