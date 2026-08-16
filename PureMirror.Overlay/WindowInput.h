#pragma once

#include <string_view>
#include <windows.h>

namespace PureMirror
{
    // Injects keyboard input through Windows. For safety, input is only generated
    // while the configured target is the foreground window.
    class WindowInput final
    {
      public:
        explicit WindowInput(HWND window = nullptr) noexcept;

        void SetWindow(HWND window) noexcept;
        [[nodiscard]] HWND GetWindow() const noexcept;
        [[nodiscard]] bool IsAvailable() const noexcept;
        [[nodiscard]] bool IsForeground() const noexcept;

        bool SendKey(UINT virtualKey) const;
        bool SendKeyDown(UINT virtualKey) const;
        bool SendKeyUp(UINT virtualKey) const;
        bool SendText(std::wstring_view text) const;
        bool SendTextUtf8(std::string_view text) const;

      private:
        static INPUT CreateKeyInput(UINT virtualKey, bool released);
        static INPUT CreateCharacterInput(wchar_t character, bool released);
        [[nodiscard]] bool CanSend() const noexcept;

        HWND m_Window;
    };
}  // namespace PureMirror
