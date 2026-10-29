#include <windows.h>
#include <commctrl.h>

#include "article_output.h"
#include "article_parser.h"
#include "http_client.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int page_edit_id = 101;
constexpr int file_edit_id = 102;
constexpr int email_edit_id = 103;
constexpr int scrape_button_id = 104;
constexpr UINT scrape_finished_message = WM_APP + 1;

struct WindowControls {
    HWND page = nullptr;
    HWND file = nullptr;
    HWND email = nullptr;
    HWND button = nullptr;
    HWND status = nullptr;
    std::filesystem::path application_folder;
};

struct ScrapeResult {
    std::wstring message;
    bool success = false;
};

std::wstring trim(const std::wstring& value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](wchar_t c) { return std::iswspace(c); });
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](wchar_t c) { return std::iswspace(c); }).base();
    return {first, last};
}

std::wstring field_text(HWND field) {
    const int length = GetWindowTextLengthW(field);
    std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(field, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return trim(text);
}

std::string utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (length == 0) {
        throw std::runtime_error("Could not encode the input text.");
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0);
    if (length == 0) {
        return L"An unknown error occurred.";
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length);
    return result;
}

std::filesystem::path executable_folder() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        throw std::runtime_error("Could not locate the program folder.");
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

HWND add_control(HWND parent, const wchar_t* type, const wchar_t* text,
                 DWORD style, int x, int y, int width, int height, int id = 0) {
    HWND control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
                                   x, y, width, height, parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
    if (control != nullptr) {
        SendMessageW(control, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    }
    return control;
}

void show_error(HWND window, const std::string& message) {
    MessageBoxW(window, wide(message).c_str(), L"Web Scraper", MB_OK | MB_ICONERROR);
}

void start_scrape(HWND window, WindowControls& controls) {
    const std::wstring page = field_text(controls.page);
    const std::wstring file = field_text(controls.file);
    const std::wstring email = field_text(controls.email);
    if (page.empty() || file.empty() || email.empty()) {
        show_error(window, "Enter a Wikipedia page, file name, and contact email or website.");
        return;
    }

    try {
        const std::filesystem::path output = article_output_path(controls.application_folder, file);
        const std::string page_utf8 = utf8(page);
        const std::string contact_utf8 = utf8(email);
        EnableWindow(controls.button, FALSE);
        SetWindowTextW(controls.status, L"Fetching and parsing the article...");

        std::thread([window, page_utf8, contact_utf8, output]() {
            auto result = std::make_unique<ScrapeResult>();
            try {
                const Article article = parse_article(fetch_article_html(page_utf8, contact_utf8));
                save_article_json(article, output);
                result->success = true;
                result->message = L"Saved to Scrapped_Pages\\" + output.filename().wstring();
            } catch (const std::exception& error) {
                result->message = wide(error.what());
            }
            if (!PostMessageW(window, scrape_finished_message, 0,
                              reinterpret_cast<LPARAM>(result.get()))) {
                return;
            }
            result.release();
        }).detach();
    } catch (const std::exception& error) {
        EnableWindow(controls.button, TRUE);
        show_error(window, error.what());
    }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* controls = reinterpret_cast<WindowControls*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        auto owned = std::make_unique<WindowControls>();
        try {
            owned->application_folder = executable_folder();
        } catch (const std::exception& error) {
            show_error(window, error.what());
            return -1;
        }
        add_control(window, L"STATIC", L"Wikipedia page (example: Ada Lovelace)", 0, 20, 18, 520, 20);
        owned->page = add_control(window, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                                  20, 40, 510, 26, page_edit_id);
        SendMessageW(owned->page, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Example: Ada Lovelace"));

        add_control(window, L"STATIC", L"File name (example: ada_lovelace; .json added)", 0, 20, 80, 520, 20);
        owned->file = add_control(window, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                                  20, 102, 510, 26, file_edit_id);
        SendMessageW(owned->file, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Example: ada_lovelace"));

        add_control(window, L"STATIC", L"Contact email or website (example: you@example.com)", 0, 20, 142, 520, 20);
        owned->email = add_control(window, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                                   20, 164, 510, 26, email_edit_id);
        SendMessageW(owned->email, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Example: you@example.com"));

        owned->button = add_control(window, L"BUTTON", L"Scrape page", WS_TABSTOP | BS_DEFPUSHBUTTON,
                                    20, 207, 135, 32, scrape_button_id);
        owned->status = add_control(window, L"STATIC", L"Ready", 0, 20, 252, 510, 40);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owned.release()));
        return 0;
    }
    case WM_COMMAND:
        if (controls != nullptr && LOWORD(wparam) == scrape_button_id && HIWORD(wparam) == BN_CLICKED) {
            start_scrape(window, *controls);
            return 0;
        }
        break;
    case scrape_finished_message: {
        std::unique_ptr<ScrapeResult> result(reinterpret_cast<ScrapeResult*>(lparam));
        if (controls != nullptr) {
            EnableWindow(controls->button, TRUE);
            SetWindowTextW(controls->status, result->success ? result->message.c_str() : L"Scrape failed.");
            if (!result->success) {
                MessageBoxW(window, result->message.c_str(), L"Web Scraper", MB_OK | MB_ICONERROR);
            }
        }
        return 0;
    }
    case WM_DESTROY:
        delete controls;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    const wchar_t* class_name = L"CSE381WikipediaScraper";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = class_name;
    if (!RegisterClassW(&window_class)) {
        return 1;
    }
    HWND window = CreateWindowExW(0, class_name, L"Wikipedia Web Scraper",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 570, 345,
                                  nullptr, nullptr, instance, nullptr);
    if (window == nullptr) {
        return 1;
    }
    ShowWindow(window, show_command);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
