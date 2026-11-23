#pragma once

// =============================================================================
// 应用主体：窗口 + ImGui + ECS World + 系统调度 + 主循环
//
// 主循环每帧顺序：
//   1. 轮询事件（F11 全屏 / ESC 退出全屏）
//   2. 写入 WindowState 资源
//   3. ImGui BeginFrame
//   4. ECS 系统调度（Input -> Simulation -> Layout -> Render -> Overlay）
//   5. ImGui 绘制 + 交换缓冲
//   6. 自测模式下按帧序号截图 / 缩放 / 全屏，并输出 metrics.json
// =============================================================================

#include <cstdint>
#include <string>
#include <vector>

#include "zzyl/app/AppState.h"
#include "zzyl/core/SystemScheduler.h"
#include "zzyl/core/World.h"
#include "zzyl/layout/Layout.h"
#include "zzyl/platform/ImGuiLayer.h"
#include "zzyl/platform/Window.h"

namespace zzyl {

class App {
public:
    struct Options {
        int width = 1280;
        int height = 720;
        bool hidden = false;         // 隐藏窗口（自测时使用）
        bool selftest = false;       // 自测模式：自动截图/缩放/全屏并退出
        bool autoLogin = false;      // 启动后自动用 Mock 账号登录（演示/自测）
        std::string selftestDir;     // 截图与 metrics.json 输出目录
        int selftestFrames = 16;     // 自测运行帧数上限（保护）
    };

    int Run(const Options& options);

private:
    void SetupResources();
    void UpdateWindowResource(const FrameContext& frame);
    void HandleGlobalHotkeys();
    void DrawDebugOverlay(const FrameContext& frame);
    void SelfTestStep(const FrameContext& frame);
    bool CaptureScreenshot(const std::string& name);
    void WriteSelfTestReport();
    void ForceLoginForSelfTest();
    bool EnsureSelfTestDirectory();

    struct CaptureRecord {
        std::string name;
        int windowWidth = 0;
        int windowHeight = 0;
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        bool fullscreen = false;
        layout::ShellLayout layout;
    };

    platform::Window m_window;
    platform::ImGuiLayer m_imgui;
    World m_world;
    SystemScheduler m_scheduler;
    Options m_options;

    std::vector<CaptureRecord> m_captures;
    bool m_selftest = false;
    bool m_selftestDone = false;
    std::string m_selftestDir;
    bool m_hasChineseGlyph = false;
    float m_chineseTextWidth = 0.0f;
    float m_fontSizeMeasured = 0.0f;
    float m_fontSizeBaseMeasured = 0.0f;
    // 命令队列时序回归测试（自测用：帧末入队 → 下一帧 Simulation 消费）
    bool m_commandQueuedForTest = false;
    bool m_commandQueueTimingOk = false;
    int m_loginRouteSeen = 0;
    int m_dashboardRouteSeen = 0;
};

}  // namespace zzyl
