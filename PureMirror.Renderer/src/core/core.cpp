// clang-format off
#include "pch.h"
// clang-format on

#include "core.h"

#include "BackendDetector.h"
#include "console/console.h"
#include "external/imgui/imgui_impl_win32.h"
#include "graphics/TextureManager.h"
#include "graphics/dx12/DX12GpuUploader.h"
#include "utils/utils.h"

#include <array>
#include <filesystem>
#include <imgui.h>
#include <include/CoreApi.h>
#include <include/Texture.h>
#include <initializer_list>
#include <optional>

namespace Core
{
    using GetCoreAPI_t = CoreAPI* (*)();
    static HINSTANCE g_coreDLL = NULL;
    static std::vector<CoreAPI*> g_CoreAPIs = {};

    static HWND g_hWindow = NULL;
    static WNDPROC oWndProc;

    constexpr float DEFAULT_FONT_SIZE = 18.0f;

    std::string ToLowerAscii(std::string value)
    {
        std::transform(value.begin(),
                       value.end(),
                       value.begin(),
                       [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return value;
    }

    std::optional<std::filesystem::path> FindFontFile(const std::filesystem::path& fontDirectory,
                                                      const std::initializer_list<const char*> candidateNames)
    {
        std::error_code error;
        if (!std::filesystem::is_directory(fontDirectory, error))
            return std::nullopt;

        for (const auto* candidate : candidateNames)
        {
            const auto path = fontDirectory / candidate;
            if (std::filesystem::is_regular_file(path, error))
                return path;
        }

        for (std::filesystem::recursive_directory_iterator
                 iterator(fontDirectory, std::filesystem::directory_options::skip_permission_denied, error),
             end;
             iterator != end;
             iterator.increment(error))
        {
            if (error)
            {
                error.clear();
                continue;
            }
            if (!iterator->is_regular_file(error))
                continue;

            const auto filename = ToLowerAscii(iterator->path().filename().string());
            for (const auto* candidate : candidateNames)
            {
                if (filename == ToLowerAscii(candidate))
                    return iterator->path();
            }
        }
        return std::nullopt;
    }

    ImFont* AddFont(ImGuiIO& io,
                    const std::filesystem::path& path,
                    const ImWchar* glyphRanges,
                    ImFont* destination = nullptr)
    {
        ImFontConfig config{};
        config.MergeMode = destination != nullptr;
        config.DstFont = destination;
        config.PixelSnapH = true;

        const auto filename = path.string();
        const auto fontSize = destination && destination->FontSize ? destination->FontSize : DEFAULT_FONT_SIZE;
        return io.Fonts->AddFontFromFileTTF(filename.c_str(), fontSize, &config, glyphRanges);
    }

    void InitializeFonts(ImGuiIO& io)
    {
        std::array<wchar_t, 256> executablePath{};
        const auto length =
            GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
        if (length == 0 || length >= executablePath.size())
        {
            io.FontDefault = io.Fonts->AddFontDefault();
            return;
        }

        const auto fontDirectory =
            std::filesystem::path(executablePath.data()).parent_path() / L"puremirror" / L"fonts";

        static ImVector<ImWchar> baseRanges;
        if (baseRanges.empty())
        {
            ImFontGlyphRangesBuilder builder;
            builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
            builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
            builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
            builder.AddRanges(io.Fonts->GetGlyphRangesVietnamese());
            builder.BuildRanges(&baseRanges);
        }

        ImFont* defaultFont = io.Fonts->AddFontDefault();

        const auto merge = [&](const std::initializer_list<const char*> names, const ImWchar* ranges)
        {
            if (const auto path = FindFontFile(fontDirectory, names))
                AddFont(io, *path, ranges, defaultFont);
        };

        merge({"NotoSans-Regular.ttf", "NotoSans-VariableFont_wdth,wght.ttf"}, baseRanges.Data);

        merge({"NotoSansSC-Regular.ttf",
               "NotoSansSC-VariableFont_wght.ttf",
               "NotoSansTC-Regular.ttf",
               "NotoSansTC-VariableFont_wght.ttf"},
              io.Fonts->GetGlyphRangesChineseFull());
        merge({"NotoSansJP-Regular.ttf", "NotoSansJP-VariableFont_wght.ttf"}, io.Fonts->GetGlyphRangesJapanese());
        merge({"NotoSansKR-Regular.ttf", "NotoSansKR-VariableFont_wght.ttf"}, io.Fonts->GetGlyphRangesKorean());
        merge({"NotoSansThai-Regular.ttf", "NotoSansThai-VariableFont_wdth,wght.ttf"}, io.Fonts->GetGlyphRangesThai());

        static constexpr ImWchar arabicRanges[] = {
            0x0600, 0x06FF, 0x0750, 0x077F, 0x08A0, 0x08FF, 0xFB50, 0xFDFF, 0xFE70, 0xFEFF, 0};
        merge({"NotoSansArabic-Regular.ttf", "NotoSansArabic-VariableFont_wdth,wght.ttf"}, arabicRanges);

        // Dear ImGui has no automatic fallback between separate ImFont objects,
        // therefore all available Noto glyphs are merged into the active font.
        io.FontDefault = defaultFont;
    }

    TextureManager g_TextureManager;

    const char* GetImguiVersion();
    HWND GetWindow();

    RendererAPI g_rendererAPI = {.Version = RENDERER_API_VERSION,
                                 .Size = sizeof(RendererAPI),
                                 .GetImguiVersion = GetImguiVersion,
                                 .GetWindow = GetWindow,
                                 .LoadTexture = LoadTexture};

    static LRESULT WINAPI WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (GetMessageExtraInfo() == DIRECT_GAME_INPUT_MARKER)
            return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);

        for (auto& coreApi : g_CoreAPIs)
        {
            auto result = coreApi->HandleInput(hWnd, uMsg, wParam, lParam);
            if (result != 0)
                return result;
        }

        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    const char* GetImguiVersion()
    {
        return ImGui::GetVersion();
    }

    HWND GetWindow()
    {
        return g_hWindow;
    }

    void InitializeLibs()
    {
        g_coreDLL = LoadLibraryA("PureMirror.Core.dll");
        if (g_coreDLL)
        {
            auto GetCoreAPI = reinterpret_cast<GetCoreAPI_t>(GetProcAddress(g_coreDLL, "GetCoreAPI"));

            if (GetCoreAPI)
            {
                auto coreAPI = GetCoreAPI();
                // TODO Version check imgui check whatever
                g_CoreAPIs.push_back(coreAPI);

                coreAPI->Initialize(&g_rendererAPI);
            }
        }
    }

    bool HasContext()
    {
        return !!ImGui::GetCurrentContext();
    }

    void InitializeContext(HWND hwnd)
    {
        if (ImGui::GetCurrentContext())
            return;

        g_hWindow = hwnd;

        auto imguiContext = ImGui::CreateContext();
        ImGui_ImplWin32_Init(hwnd);
        InitializeLibs();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        InitializeFonts(io);

        oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    }

    void Render(RenderContext& renderContext)
    {
        for (auto& coreApi : g_CoreAPIs)
        {
            coreApi->Render(&renderContext);
        }
    }

    void Shutdown()
    {
        if (oWndProc)
        {
            SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        }
    }

    Texture LoadTexture(const char* path)
    {
        auto textureAsset = g_TextureManager.ReloadIfChanged(path);

        if (!textureAsset)
            return {};

        if (BackendDetector::Instance().GetActiveRenderer())
        {
            return BackendDetector::Instance().GetActiveRenderer()->UploadAndRetrieveTexture(textureAsset);
        }

        return {};
    }
}  // namespace Core
