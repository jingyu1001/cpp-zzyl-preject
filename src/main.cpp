#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "zzyl/app/App.h"

namespace {

void PrintUsage() {
    std::printf(
        "中州养老 - C++ 转译端 (OpenGL + GLFW + ImGui)\n"
        "用法：zzyl_desktop [选项]\n"
        "  --width <px>        窗口宽（默认 1280）\n"
        "  --height <px>       窗口高（默认 720）\n"
        "  --hidden            隐藏窗口启动（调试用）\n"
        "  --auto-login        启动后自动 Mock 登录并进入首页\n"
        "  --selftest [dir]    自测：隐藏窗口截图 1280x720 / 缩放 / F11 全屏，输出 BMP + metrics.json\n"
        "  --frames <n>        自测帧数上限（默认 16）\n"
        "  --help              显示本帮助\n"
        "\n"
        "运行时快捷键：F11 全屏切换 | ESC 退出全屏 | F1 调试浮层\n");
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);
#endif

    zzyl::App::Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            PrintUsage();
            return 0;
        }
        if (argument == "--width" && index + 1 < argc) {
            options.width = std::atoi(argv[++index]);
            continue;
        }
        if (argument == "--height" && index + 1 < argc) {
            options.height = std::atoi(argv[++index]);
            continue;
        }
        if (argument == "--hidden") {
            options.hidden = true;
            continue;
        }
        if (argument == "--auto-login") {
            options.autoLogin = true;
            continue;
        }
        if (argument == "--selftest") {
            options.selftest = true;
            options.hidden = true;
            if (index + 1 < argc && argv[index + 1][0] != '-') {
                options.selftestDir = argv[++index];
            }
            continue;
        }
        if (argument == "--frames" && index + 1 < argc) {
            options.selftestFrames = std::atoi(argv[++index]);
            continue;
        }
        std::fprintf(stderr, "未知参数：%s\n", argument.c_str());
        PrintUsage();
        return 0;
    }

    zzyl::App app;
    return app.Run(options);
}
