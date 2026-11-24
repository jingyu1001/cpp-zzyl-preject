#include "zzyl/platform/FileDialog.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <system_error>
#include <vector>

#include <windows.h>
#include <shellapi.h>

namespace zzyl::platform {
namespace {

std::string ExecutableDirectory() {
    wchar_t buffer[MAX_PATH * 4] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0) return ".";
    return std::filesystem::path(buffer).parent_path().string();
}

std::string ToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(),
                          size, nullptr, nullptr);
    return result;
}

std::wstring ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int size = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(),
                          size);
    return result;
}

std::filesystem::path AssetsRootDirectoryPath() {
    const std::filesystem::path exeAssets = std::filesystem::path(ExecutableDirectory()) / "assets";
    if (std::filesystem::exists(exeAssets)) return exeAssets;
    const std::filesystem::path sourceAssets = std::filesystem::path(ZZYL_SOURCE_DIR) / "assets";
    if (std::filesystem::exists(sourceAssets)) return sourceAssets;
    return exeAssets;
}

std::string Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &time);
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d_%02d%02d%02d", local.tm_year + 1900,
                  local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
    return buffer;
}

bool HasExtension(const std::wstring& path, const wchar_t* extensions[], int count) {
    const std::wstring ext = std::filesystem::path(path).extension().wstring();
    for (int index = 0; index < count; ++index) {
        if (_wcsicmp(ext.c_str(), extensions[index]) == 0) return true;
    }
    return false;
}

FilePickResult ImportSelectedFile(const std::wstring& selectedPath, const std::string& subdir,
                                  std::size_t maxBytes) {
    FilePickResult result;
    const std::filesystem::path source(selectedPath);
    result.fileName = ToUtf8(source.filename().wstring());
    const std::wstring extension = source.extension().wstring();
    result.isPdf = (_wcsicmp(extension.c_str(), L".pdf") == 0);

    std::error_code sizeError;
    const auto fileSize = std::filesystem::file_size(source, sizeError);
    if (!sizeError && maxBytes > 0 && fileSize > maxBytes) {
        result.error = "文件超过大小限制（" + std::to_string(maxBytes / (1024 * 1024)) + "MB）";
        return result;
    }

    std::error_code ec;
    const std::filesystem::path targetDir =
        AssetsRootDirectoryPath() / std::filesystem::path(ToWide(subdir));
    std::filesystem::create_directories(targetDir, ec);
    if (ec) {
        result.error = "创建上传目录失败：" + ec.message();
        return result;
    }
    // 关键：文件名全程用宽字符构造，禁止 UTF-8 的 std::string 直接拼 path（Windows 会按 ACP 解释）
    const std::wstring targetName =
        ToWide(Timestamp()) + L"_" + source.stem().wstring() + extension;
    const std::filesystem::path target = targetDir / targetName;
    std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        result.error = "复制文件失败：" + ec.message();
        return result;
    }
    result.ok = true;
    result.absolutePath = ToUtf8(target.wstring());
    result.relativePath = "assets/" + subdir + "/" + ToUtf8(target.filename().wstring());
    return result;
}

// =============================================================================
// 应用内文件浏览器（Win32 自定义窗口，不依赖 Shell/COM/系统 #32770，避免本机
// GetOpenFileNameW 白屏挂起）。同步模态返回选择结果；页面 API 不变。
// =============================================================================
struct PickerState {
    HWND window = nullptr;
    HWND list = nullptr;
    HWND pathLabel = nullptr;
    WNDPROC listOriginalProc = nullptr;
    std::wstring directory;
    std::vector<std::wstring> itemPaths;
    std::vector<bool> itemIsDir;
    std::wstring selectedPath;
    bool done = false;
    bool accepted = false;
};

PickerState* g_picker = nullptr;

void PickerPopulate(PickerState& state) {
    state.itemPaths.clear();
    state.itemIsDir.clear();
    SendMessageW(state.list, LB_RESETCONTENT, 0, 0);

    std::error_code ec;
    const std::filesystem::path directory(state.directory);
    std::vector<std::wstring> directories;
    std::vector<std::wstring> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        const std::wstring name = entry.path().filename().wstring();
        if (name.empty() || name[0] == L'.') continue;
        if (entry.is_directory()) {
            directories.push_back(name);
        } else if (entry.is_regular_file()) {
            files.push_back(name);
        }
    }
    std::sort(directories.begin(), directories.end());
    std::sort(files.begin(), files.end());

    SetWindowTextW(state.pathLabel, state.directory.c_str());
    for (const std::wstring& name : directories) {
        const std::wstring label = L"[" + name + L"]";
        const int index = static_cast<int>(SendMessageW(
            state.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
        SendMessageW(state.list, LB_SETITEMDATA, index, static_cast<LPARAM>(state.itemPaths.size()));
        state.itemPaths.push_back((directory / name).wstring());
        state.itemIsDir.push_back(true);
    }
    for (const std::wstring& name : files) {
        const int index = static_cast<int>(SendMessageW(
            state.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str())));
        SendMessageW(state.list, LB_SETITEMDATA, index, static_cast<LPARAM>(state.itemPaths.size()));
        state.itemPaths.push_back((directory / name).wstring());
        state.itemIsDir.push_back(false);
    }
    if (!state.itemPaths.empty()) {
        SendMessageW(state.list, LB_SETCURSEL, 0, 0);
    }
}

void PickerFinish(PickerState& state, bool accepted) {
    state.done = true;
    state.accepted = accepted;
    if (state.window != nullptr) DestroyWindow(state.window);
}

void PickerActivateSelected(PickerState& state) {
    const int index = static_cast<int>(SendMessageW(state.list, LB_GETCURSEL, 0, 0));
    if (index < 0) return;
    const auto data = static_cast<std::size_t>(SendMessageW(state.list, LB_GETITEMDATA, index, 0));
    if (data >= state.itemPaths.size()) return;
    if (state.itemIsDir[data]) {
        state.directory = state.itemPaths[data];
        PickerPopulate(state);
    } else {
        state.selectedPath = state.itemPaths[data];
        PickerFinish(state, true);
    }
}

LRESULT CALLBACK PickerListProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PickerState* state = reinterpret_cast<PickerState*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state != nullptr) {
        if (message == WM_KEYDOWN) {
            if (wParam == VK_RETURN) {
                PickerActivateSelected(*state);
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                PickerFinish(*state, false);
                return 0;
            }
        }
        if (message == WM_LBUTTONDBLCLK) {
            PickerActivateSelected(*state);
            return 0;
        }
    }
    return ::CallWindowProcW(state != nullptr ? state->listOriginalProc : DefWindowProcW, hwnd,
                             message, wParam, lParam);
}

void PickerGoUp(PickerState& state) {
    const std::filesystem::path parent = std::filesystem::path(state.directory).parent_path();
    if (!parent.empty() && parent != std::filesystem::path(state.directory)) {
        state.directory = parent.wstring();
        PickerPopulate(state);
    }
}

LRESULT CALLBACK PickerWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PickerState* state = g_picker;
    switch (message) {
        case WM_COMMAND:
            if (state == nullptr) break;
            switch (LOWORD(wParam)) {
                case 1:
                    PickerActivateSelected(*state);
                    return 0;
                case 3:
                    PickerGoUp(*state);
                    return 0;
                case IDCANCEL:
                    PickerFinish(*state, false);
                    return 0;
                default:
                    if (HIWORD(wParam) == LBN_DBLCLK) {
                        PickerActivateSelected(*state);
                        return 0;
                    }
                    break;
            }
            break;
        case WM_CLOSE:
            if (state != nullptr) PickerFinish(*state, false);
            return 0;
        case WM_TIMER:
            if (state != nullptr && wParam == 1 &&
                (::GetEnvironmentVariableW(L"ZZYL_PICKER_AUTOTEST", nullptr, 0) > 0 ||
                 ::GetEnvironmentVariableW(L"ZZYL_PICKER_SHOWTEST", nullptr, 0) > 0)) {
                ::KillTimer(hwnd, 1);
                for (std::size_t index = 0; index < state->itemIsDir.size(); ++index) {
                    if (!state->itemIsDir[index]) {
                        SendMessageW(state->list, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
                        PickerActivateSelected(*state);
                        return 0;
                    }
                }
                PickerFinish(*state, false);
                return 0;
            }
            break;
        case WM_DESTROY:
            return 0;
        default:
            break;
    }
    return ::DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RunInAppPicker(const std::string& subdir, std::wstring* outPath) {
    // 隐藏自动化钩子：不创建窗口，直接返回起始目录首个文件
    if (::GetEnvironmentVariableW(L"ZZYL_PICKER_AUTOTEST", nullptr, 0) > 0) {
        std::filesystem::path start = std::filesystem::path(AssetsRootDirectory());
        if (subdir.find("roomtype") != std::string::npos) {
            start /= "roomtypes";
        } else if (!subdir.empty()) {
            start /= subdir;
        }
        if (!std::filesystem::exists(start)) {
            start = std::filesystem::path(AssetsRootDirectory()) / "roomtypes";
        }
        std::error_code ec;
        std::filesystem::path fallback = std::filesystem::path(AssetsRootDirectory()) / "roomtypes";
        std::wstring fallbackFirst;
        for (const auto& entry : std::filesystem::directory_iterator(start, ec)) {
            if (!entry.is_regular_file()) continue;
            const std::wstring name = entry.path().filename().wstring();
            // 跳过本测试自己生成的 `<yyyyMMdd_HHmmss>_` 文件，避免反馈循环/双前缀
            bool generated = name.size() > 16 && name[8] == L'_' && name[15] == L'_';
            if (generated) {
                if (fallbackFirst.empty()) fallbackFirst = entry.path().wstring();
                continue;
            }
            *outPath = entry.path().wstring();
            return true;
        }
        if (fallbackFirst.empty()) {
            std::error_code fbError;
            for (const auto& entry : std::filesystem::directory_iterator(fallback, fbError)) {
                if (entry.is_regular_file()) {
                    *outPath = entry.path().wstring();
                    return true;
                }
            }
        } else {
            *outPath = fallbackFirst;  // 只有生成文件时仍可测试（避免空目录直接失败）
            return true;
        }
        return false;
    }

    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = PickerWindowProc;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"ZZYL_InAppFilePicker";
        ::RegisterClassExW(&wc);
        classRegistered = true;
    }

    PickerState state;
    const bool silentAutotest = ::GetEnvironmentVariableW(L"ZZYL_PICKER_AUTOTEST", nullptr, 0) > 0;
    const bool showTest = ::GetEnvironmentVariableW(L"ZZYL_PICKER_SHOWTEST", nullptr, 0) > 0;
    const bool autoSelect = silentAutotest || showTest;

    std::filesystem::path start = std::filesystem::path(AssetsRootDirectory());
    bool hasEnvStart = false;
    {
        wchar_t envBuffer[MAX_PATH * 4] = {};
        const DWORD envLength = ::GetEnvironmentVariableW(
            L"ZZYL_PICKER_START_DIR", envBuffer, static_cast<DWORD>(std::size(envBuffer)));
        if (envLength > 0 && envLength < std::size(envBuffer)) {
            const std::filesystem::path envPath(envBuffer);
            std::error_code envError;
            if (std::filesystem::is_directory(envPath, envError)) {
                start = envPath;
                hasEnvStart = true;
            }
        }
    }
    if (!hasEnvStart) {
        if (subdir.find("roomtype") != std::string::npos) {
            start /= "roomtypes";
        } else if (!subdir.empty()) {
            start /= subdir;
        }
    }
    if (!std::filesystem::exists(start)) {
        start = std::filesystem::path(AssetsRootDirectory());
    }
    state.directory = start.wstring();

    const int width = 760;
    const int height = 520;
    RECT workArea{};
    ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
    const int y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
    HWND owner = ::GetForegroundWindow();
    if (owner == nullptr) owner = ::GetActiveWindow();

    const DWORD windowStyle =
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
        (silentAutotest ? 0u : static_cast<DWORD>(WS_VISIBLE));
    state.window = ::CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"ZZYL_InAppFilePicker", L"选择文件（应用内）",
        windowStyle, x, y, width, height, owner, nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (state.window == nullptr) return false;
    g_picker = &state;
    ::SetWindowLongPtrW(state.window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));

    state.pathLabel = ::CreateWindowExW(0, L"STATIC", state.directory.c_str(),
                                        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS, 12, 10,
                                        width - 24, 22, state.window, nullptr,
                                        ::GetModuleHandleW(nullptr), nullptr);
    state.list = ::CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 12, 38,
        width - 24, height - 100, state.window, reinterpret_cast<HMENU>(100),
        ::GetModuleHandleW(nullptr), nullptr);
    state.listOriginalProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(
        state.list, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PickerListProc)));
    ::SetWindowLongPtrW(state.list, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));

    ::CreateWindowExW(0, L"BUTTON", L"选择", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                      width - 260, height - 52, 80, 30, state.window,
                      reinterpret_cast<HMENU>(1), ::GetModuleHandleW(nullptr), nullptr);
    ::CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP, width - 170,
                      height - 52, 80, 30, state.window, reinterpret_cast<HMENU>(IDCANCEL),
                      ::GetModuleHandleW(nullptr), nullptr);
    ::CreateWindowExW(0, L"BUTTON", L"上一级", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 12,
                      height - 52, 90, 30, state.window, reinterpret_cast<HMENU>(3),
                      ::GetModuleHandleW(nullptr), nullptr);

    PickerPopulate(state);
    if (!silentAutotest) {
        ::ShowWindow(state.window, SW_SHOW);
        ::SetForegroundWindow(state.window);
        ::SetFocus(state.list);
    }
    if (autoSelect) {
        ::SetTimer(state.window, 1, showTest ? 1200u : 250u, nullptr);
    }

    MSG message{};
    while (!state.done && ::GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!::IsDialogMessageW(state.window, &message)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }
    g_picker = nullptr;
    if (state.accepted && !state.selectedPath.empty()) {
        *outPath = state.selectedPath;
        return true;
    }
    return false;
}

}  // namespace

std::string AssetsRootDirectory() {
    return ToUtf8(AssetsRootDirectoryPath().wstring());
}

FilePickResult PickAndImportFile(const wchar_t* /*filter*/, const std::string& subdir,
                                 std::size_t maxBytes) {
    FilePickResult result;
    std::wstring selectedPath;
    if (!RunInAppPicker(subdir, &selectedPath)) {
        result.canceled = true;
        return result;
    }
    return ImportSelectedFile(selectedPath, subdir, maxBytes);
}

FilePickResult PickImageFile(const std::string& subdir, std::size_t maxBytes) {
    FilePickResult result = PickAndImportFile(nullptr, subdir, maxBytes);
    if (result.ok) {
        static const wchar_t* kExtensions[] = {L".png", L".jpg", L".jpeg", L".bmp", L".webp"};
        if (!HasExtension(std::filesystem::path(result.absolutePath).wstring(), kExtensions, 5)) {
            result.ok = false;
            result.error = "请选择图片文件（png/jpg/jpeg/bmp/webp）";
        }
    }
    return result;
}

FilePickResult PickPdfFile(const std::string& subdir, std::size_t maxBytes) {
    FilePickResult result = PickAndImportFile(nullptr, subdir, maxBytes);
    if (result.ok && !result.isPdf) {
        result.ok = false;
        result.error = "请选择 PDF 文件";
    }
    return result;
}

std::string ResolveAssetAbsolutePath(const std::string& relativePath) {
    if (relativePath.empty()) return {};
    const std::filesystem::path path(ToWide(relativePath));
    if (path.is_absolute()) return ToUtf8(path.wstring());
    const std::filesystem::path absolute = AssetsRootDirectoryPath().parent_path() / path;
    return ToUtf8(absolute.wstring());
}

bool OpenFileWithShell(const std::string& relativePath) {
    const std::string absolutePath = ResolveAssetAbsolutePath(relativePath);
    if (absolutePath.empty()) return false;
    const std::wstring widePath = ToWide(absolutePath);
    const HINSTANCE result =
        ::ShellExecuteW(nullptr, L"open", widePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(result) > 32;
}

}  // namespace zzyl::platform
