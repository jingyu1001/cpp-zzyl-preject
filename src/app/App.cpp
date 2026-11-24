#include "zzyl/app/App.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#include "glad/glad.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "imgui.h"

#include "zzyl/app/ModuleRegistry.h"
#include "zzyl/app/Routes.h"
#include "zzyl/data/Services.h"
#include "zzyl/platform/Screenshot.h"
#include "zzyl/render/Texture.h"
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"
#include "zzyl/version.h"

namespace zzyl {
namespace {

std::string FormatRect(const layout::Rect& rect) {
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "[%.1f,%.1f,%.1f,%.1f]", rect.x, rect.y, rect.w, rect.h);
    return std::string(buffer);
}

}  // namespace

void App::SetupResources() {
    if (auto* window = m_world.TryResource<WindowState>()) {
        window->width = m_window.Width();
        window->height = m_window.Height();
        window->framebufferWidth = m_window.FramebufferWidth();
        window->framebufferHeight = m_window.FramebufferHeight();
        window->contentScale = m_window.ContentScale();
        window->fullscreen = m_window.IsFullscreen();
        window->minimized = m_window.IsMinimized();
    }
}

int App::Run(const Options& options) {
    m_options = options;
    m_selftest = options.selftest;
    m_selftestDir = options.selftestDir.empty() ? std::string("artifacts/selftest") : options.selftestDir;
    // SQLite 路径：--db 优先；其次环境变量 ZZYL_DB_PATH；否则 ModuleRegistry 用默认 <exe>/data/zzyl.db
    if (m_options.dbPath.empty()) {
        if (const char* envDbPath = std::getenv("ZZYL_DB_PATH"); envDbPath != nullptr && envDbPath[0] != '\0') {
            m_options.dbPath = envDbPath;
        }
    }
    if (m_selftest && !EnsureSelfTestDirectory()) {
        return 2;
    }

    platform::WindowConfig config;
    config.width = options.width;
    config.height = options.height;
    config.title = kAppTitle;
    config.visible = !options.hidden && !options.selftest;
    if (!m_window.Create(config)) {
        return 1;
    }
    if (!m_imgui.Init(m_window)) {
        m_window.Destroy();
        return 1;
    }

    // Sprint 2：WIC 纹理自检（只读一张 assets/icons 素材 png；自测时加载）
    if (m_selftest) {
        const std::filesystem::path iconPath =
            render::ResolveAssetPath("assets/icons/修剪指甲@2x.png");
        m_selftestIcon = render::LoadTextureFromFile(iconPath);
        m_selftestIconReady = m_selftestIcon.Valid();
        std::printf("[selftest] WIC 素材图：%s | 路径=%ls | 尺寸=%dx%d\n",
                    m_selftestIconReady ? "OK" : "失败", iconPath.wstring().c_str(),
                    m_selftestIcon.width, m_selftestIcon.height);
    }

    RegisterAllModules(m_world, m_scheduler, m_options.emptyData, m_options.dbPath);
    m_scheduler.Start(m_world);
    UpdateWindowResource(FrameContext{});

    std::printf("[App] 启动完成：窗口 %dx%d，系统 %zu 个，实体 %zu 个，资源 %zu 个，空数据模式=%s\n",
                m_window.Width(), m_window.Height(), m_scheduler.Count(), m_world.EntityCount(),
                m_world.ResourceCount(), m_options.emptyData ? "开" : "关");

    const auto startTime = std::chrono::steady_clock::now();
    auto lastTime = startTime;
    std::uint64_t frameIndex = 0;
    bool autoLoginDone = false;

    while (!m_window.ShouldClose()) {
        m_window.PollEvents();

        const auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        deltaTime = std::min(deltaTime, 0.1f);

        FrameContext frame;
        frame.deltaTime = deltaTime;
        frame.totalTime = std::chrono::duration<double>(now - startTime).count();
        frame.frameIndex = frameIndex;

        UpdateWindowResource(frame);

        if (!m_window.IsMinimized()) {
            // ---- 命令队列生命周期（关键时序，勿改）----------------------------
            // 渲染层（Render 阶段）入队的命令是给"下一帧 Simulation"消费的。因此：
            //   帧首：记录上一帧遗留（=上一帧 Render 入队）的命令数量 pending；
            //   帧中：Simulation 系统读取并处理这些命令（消费方只读不删）；
            //   帧末：只删除前 pending 条（上一批），本帧 Render 新入队的命令保留给下一帧。
            // 若在帧末无条件 Clear()，会把本帧 Render 刚入队的 Login/Navigate/Logout
            // 立即清掉，导致下一帧 Simulation 永远收不到 → 所有交互失效（已实际踩坑）。
            std::size_t pendingCommandCount = 0;
            if (auto* queue = m_world.TryResource<CommandQueue>()) {
                pendingCommandCount = queue->commands.size();
            }

            m_imgui.BeginFrame();

            HandleGlobalHotkeys();

            m_scheduler.Update(m_world, frame);

            // 中文字形自检（当前帧字体已绑定）
            m_hasChineseGlyph = m_imgui.HasGlyph(0x4E2D) && m_imgui.HasGlyph(0x5DDE) &&
                                m_imgui.HasGlyph(0x517B) && m_imgui.HasGlyph(0x8001);
            m_chineseTextWidth = ImGui::CalcTextSize("中州养老").x;
            m_fontSizeMeasured = ImGui::GetFontSize();
            m_fontSizeBaseMeasured = ImGui::GetStyle().FontSizeBase;

            // Sprint 2：自测第 22 帧绘制 WIC 纹理自检（页面切换后，保证截图可见）
            if (m_selftest && frame.frameIndex == 22) {
                DrawSelfTestTexturePreview();
            }

            if (auto* ui = m_world.TryResource<UiState>(); ui != nullptr && ui->debugOverlay) {
                DrawDebugOverlay(frame);
            }

            m_imgui.RenderDrawData();

            if (m_selftest) {
                SelfTestStep(frame);
            } else if (options.autoLogin && !autoLoginDone && frameIndex >= 2) {
                ForceLoginForSelfTest();
                autoLoginDone = true;
            }

            m_window.SwapBuffers();

            // 帧末：只丢弃本轮 Simulation 已消费的"上一帧命令"；
            // 本帧 Render 新入队的命令（超出 pending 的部分）必须保留到下一帧。
            if (auto* queue = m_world.TryResource<CommandQueue>()) {
                const std::size_t drop =
                    std::min(pendingCommandCount, queue->commands.size());
                queue->commands.erase(
                    queue->commands.begin(),
                    queue->commands.begin() + static_cast<std::ptrdiff_t>(drop));
            }

            if (m_selftest && m_selftestDone) {
                break;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        ++frameIndex;
    }

    if (m_selftest) {
        WriteSelfTestReport();
    }

    m_scheduler.Clear();
    render::DestroyTexture(m_selftestIcon);
    m_imgui.Shutdown();
    m_window.Destroy();
    return 0;
}

void App::UpdateWindowResource(const FrameContext& frame) {
    WindowState state;
    state.width = m_window.Width();
    state.height = m_window.Height();
    state.framebufferWidth = m_window.FramebufferWidth();
    state.framebufferHeight = m_window.FramebufferHeight();
    state.contentScale = m_window.ContentScale();
    state.deltaTime = frame.deltaTime;
    state.totalTime = frame.totalTime;
    state.frameIndex = frame.frameIndex;
    state.fullscreen = m_window.IsFullscreen();
    state.minimized = m_window.IsMinimized();
    m_world.SetResource(state);
}

void App::HandleGlobalHotkeys() {
    auto* ui = m_world.TryResource<UiState>();
    if (ui == nullptr) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        ui->debugOverlay = !ui->debugOverlay;
    }
}

void App::DrawDebugOverlay(const FrameContext& /*frame*/) {
    const WindowState* window = m_world.TryResource<WindowState>();
    const RouteState* route = m_world.TryResource<RouteState>();
    if (window == nullptr) {
        return;
    }
    const char* routeName = "登录页";
    if (route != nullptr) {
        routeName = RouteTitle(route->current);
    }

    const float width = std::min(400.0f, static_cast<float>(window->width) - 40.0f);
    const float height = 190.0f;
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(window->width) - width - 16.0f, 72.0f));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##debug_overlay", nullptr, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const layout::Rect panel{ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, width, height};
        ui::Card(drawList, panel, 8.0f);
        ImGui::PushFont(theme::F().regular, 14.0f);

        char line[256];
        float y = panel.y + 12.0f;
        const float x = panel.x + 16.0f;
        const float lineHeight = 23.0f;

        ui::Text(drawList, ImVec2(x, y), theme::Colors().textPrimary, "调试浮层（F1 显示/隐藏）");
        y += lineHeight + 2.0f;

        std::snprintf(line, sizeof(line), "FPS %.1f | 窗口 %dx%d | 帧缓冲 %dx%d | DPI %.2f",
                      ImGui::GetIO().Framerate, window->width, window->height,
                      window->framebufferWidth, window->framebufferHeight, window->contentScale);
        ui::Text(drawList, ImVec2(x, y), theme::Colors().textRegular, line);
        y += lineHeight;

        std::snprintf(line, sizeof(line), "路由：%s | 全屏：%s", routeName, window->fullscreen ? "开" : "关");
        ui::Text(drawList, ImVec2(x, y), theme::Colors().textRegular, line);
        y += lineHeight;

        std::snprintf(line, sizeof(line), "实体 %zu | 组件类型 %zu | 资源 %zu | 系统 %zu",
                      m_world.EntityCount(), m_world.ComponentTypeCount(), m_world.ResourceCount(),
                      m_scheduler.Count());
        ui::Text(drawList, ImVec2(x, y), theme::Colors().textRegular, line);
        y += lineHeight;

        std::snprintf(line, sizeof(line), "中文字形：中州养老 %s（文本宽度 %.1f px）",
                      m_hasChineseGlyph ? "OK" : "缺失", m_chineseTextWidth);
        ui::Text(drawList, ImVec2(x, y),
                 m_hasChineseGlyph ? theme::Colors().success : theme::Colors().danger, line);
        y += lineHeight;

        ui::Text(drawList, ImVec2(x, y), theme::Colors().textMuted, "F11 全屏切换 | ESC 退出全屏 | F1 调试");

        ImGui::PopFont();
    }
    ImGui::End();
}

void App::ForceLoginForSelfTest() {
    auto* services = m_world.TryResource<DataServices>();
    auto* session = m_world.TryResource<AuthSession>();
    auto* route = m_world.TryResource<RouteState>();
    if (services == nullptr || session == nullptr || route == nullptr || !services->auth) {
        return;
    }
    data::LoginRequest request;
    request.username = "admin";
    request.password = "admin123";
    request.code = "1234";
    const data::LoginResult result = services->auth->Login(request);
    if (!result.ok) {
        std::fprintf(stderr, "[selftest] 模拟登录失败：%s\n", result.message.c_str());
        return;
    }
    session->loggedIn = true;
    session->user = result.session;
    route->current = Route::Dashboard;
    if (services->dashboard) {
        m_world.SetResource(services->dashboard->LoadDashboard(result.session));
    }
    std::printf("[selftest] 模拟登录成功：%s，已切换到首页看板\n", result.session.userName.c_str());
}

bool App::EnsureSelfTestDirectory() {
    std::error_code errorCode;
    std::filesystem::create_directories(m_selftestDir, errorCode);
    if (errorCode) {
        std::fprintf(stderr, "[selftest] 无法创建输出目录 %s：%s\n", m_selftestDir.c_str(),
                     errorCode.message().c_str());
        return false;
    }
    return true;
}

bool App::CaptureScreenshot(const std::string& name) {
    const int framebufferWidth = m_window.FramebufferWidth();
    const int framebufferHeight = m_window.FramebufferHeight();
    const std::filesystem::path outputPath = std::filesystem::path(m_selftestDir) / (name + ".bmp");

    std::string error;
    if (!platform::CaptureFramebufferToBmp(framebufferWidth, framebufferHeight, outputPath.string(),
                                           &error)) {
        std::fprintf(stderr, "[selftest] 截图失败 %s：%s\n", name.c_str(), error.c_str());
        return false;
    }

    CaptureRecord record;
    record.name = name;
    record.windowWidth = m_window.Width();
    record.windowHeight = m_window.Height();
    record.framebufferWidth = framebufferWidth;
    record.framebufferHeight = framebufferHeight;
    record.fullscreen = m_window.IsFullscreen();
    if (const layout::ShellLayout* shell = m_world.TryResource<layout::ShellLayout>()) {
        record.layout = *shell;
    }
    m_captures.push_back(record);

    std::printf("[selftest] 截图 %s | 窗口 %dx%d | 帧缓冲 %dx%d | 全屏=%s\n", name.c_str(),
                record.windowWidth, record.windowHeight, record.framebufferWidth,
                record.framebufferHeight, record.fullscreen ? "是" : "否");
    return true;
}

void App::SelfTestStep(const FrameContext& frame) {
    if (m_selftestDone) {
        return;
    }
    switch (frame.frameIndex) {
        case 2:
            CaptureScreenshot("01_login_1280x720");
            // 命令队列时序回归测试：模拟"渲染层在帧末入队 Navigate"，
            // 验证下一帧 Simulation(RouteSystem) 能收到并切换路由。
            // 若帧末无条件 Clear，该命令会被清掉，第 3 帧检查失败 → 立即暴露回归。
            if (auto* queue = m_world.TryResource<CommandQueue>()) {
                CommandQueue::Command command;
                command.type = CommandQueue::Type::Navigate;
                command.arg0 = "/index";
                queue->commands.push_back(command);
                m_commandQueuedForTest = true;
            }
            break;
        case 3:
            if (m_commandQueuedForTest) {
                const RouteState* route = m_world.TryResource<RouteState>();
                m_commandQueueTimingOk =
                    route != nullptr && route->current == Route::Dashboard;
                std::printf("[selftest] 命令队列时序回归测试：%s（帧末入队 → 下一帧 Simulation 消费）\n",
                            m_commandQueueTimingOk ? "通过" : "失败");
            }
            ForceLoginForSelfTest();
            break;
        case 5:
            CaptureScreenshot("02_dashboard_1280x720");
            break;
        case 6:
            std::printf("[selftest] 缩放窗口 -> 1024x640（Web 规格最小窗口）\n");
            glfwSetWindowSize(m_window.Handle(), 1024, 640);
            break;
        case 9:
            CaptureScreenshot("03_dashboard_1024x640");
            break;
        case 10:
            std::printf("[selftest] 触发 F11 全屏逻辑\n");
            m_window.ToggleFullscreen();
            break;
        case 13:
            CaptureScreenshot("04_fullscreen");
            break;
        case 14:
            // 退出全屏 + 回到 1280×720，用于 Sprint 2 页面截图
            std::printf("[selftest] 退出全屏 -> 1280x720（页面截图基准）\n");
            m_window.ToggleFullscreen();
            glfwSetWindowSize(m_window.Handle(), 1280, 720);
            break;
        case 16:
            QueueNavigateForSelfTest("/system/user");
            break;
        case 17:
            CaptureScreenshot("05_system_user_1280x720");
            break;
        case 18:
            QueueNavigateForSelfTest("/liveIn/houseSet");
            break;
        case 19:
            CaptureScreenshot("06_room_type_1280x720");
            break;
        case 20:
            QueueNavigateForSelfTest("/nursing/project");
            break;
        case 21:
            CaptureScreenshot("07_nursing_project_1280x720");
            break;
        case 22:
            CaptureScreenshot("08_texture_wic_1280x720");
            break;
        case 23:
            QueueNavigateForSelfTest("/enterQuit/checkIn");
            break;
        case 24:
            CaptureScreenshot("09_checkin_list_1280x720");
            break;
        case 25:
            QueueNavigateForSelfTest("/enterQuit/checkInForm");
            break;
        case 26:
            CaptureScreenshot("10_checkin_form_1280x720");
            break;
        case 27:
            if (auto* selection = m_world.TryResource<CheckInSelection>(); selection != nullptr) {
                selection->id = 1;  // 自测详情帧使用确定性选中记录（列表首条）
            }
            QueueNavigateForSelfTest("/enterQuit/checkInInfo");
            break;
        case 28:
            CaptureScreenshot("11_checkin_detail_1280x720");
            break;
        // ---- Sprint 3 集成验证包：M4 三页 1024×640 / 1920×1080 ----
        case 29:
            QueueNavigateForSelfTest("/enterQuit/checkIn");
            break;
        case 30:
            std::printf("[selftest] M4 列表 -> 1024x640\n");
            glfwSetWindowSize(m_window.Handle(), 1024, 640);
            break;
        case 32:
            CaptureScreenshot("12_checkin_list_1024x640");
            break;
        case 33:
            glfwSetWindowSize(m_window.Handle(), 1280, 720);
            break;
        case 34:
            m_window.ToggleFullscreen();
            break;
        case 36:
            CaptureScreenshot("13_checkin_list_1920x1080");
            break;
        case 37:
            m_window.ToggleFullscreen();
            glfwSetWindowSize(m_window.Handle(), 1280, 720);
            break;
        case 38:
            QueueNavigateForSelfTest("/enterQuit/checkInForm");
            break;
        case 39:
            std::printf("[selftest] M4 表单 -> 1024x640\n");
            glfwSetWindowSize(m_window.Handle(), 1024, 640);
            break;
        case 41:
            CaptureScreenshot("14_checkin_form_1024x640");
            break;
        case 42:
            glfwSetWindowSize(m_window.Handle(), 1280, 720);
            break;
        case 43:
            m_window.ToggleFullscreen();
            break;
        case 45:
            CaptureScreenshot("15_checkin_form_1920x1080");
            break;
        case 46:
            m_window.ToggleFullscreen();
            glfwSetWindowSize(m_window.Handle(), 1280, 720);
            break;
        case 47:
            if (auto* selection = m_world.TryResource<CheckInSelection>(); selection != nullptr) {
                selection->id = 1;
            }
            QueueNavigateForSelfTest("/enterQuit/checkInInfo");
            break;
        case 48:
            std::printf("[selftest] M4 详情 -> 1024x640\n");
            glfwSetWindowSize(m_window.Handle(), 1024, 640);
            break;
        case 50:
            CaptureScreenshot("16_checkin_detail_1024x640");
            break;
        case 51:
            glfwSetWindowSize(m_window.Handle(), 1280, 720);
            break;
        case 52:
            m_window.ToggleFullscreen();
            break;
        case 54:
            CaptureScreenshot("17_checkin_detail_1920x1080");
            break;
        case 55:
            m_window.ToggleFullscreen();
            glfwSetWindowSize(m_window.Handle(), 1280, 720);
            break;
        case 56:
            m_selftestDone = true;
            break;
        default:
            break;
    }
    if (frame.frameIndex >= static_cast<std::uint64_t>(m_options.selftestFrames)) {
        std::fprintf(stderr, "[selftest] 达到帧数上限 %d，提前结束\n", m_options.selftestFrames);
        m_selftestDone = true;
    }
}

void App::QueueNavigateForSelfTest(const char* path) {
    auto* queue = m_world.TryResource<CommandQueue>();
    if (queue == nullptr || path == nullptr) {
        return;
    }
    CommandQueue::Command command;
    command.type = CommandQueue::Type::Navigate;
    command.arg0 = path;
    queue->commands.push_back(command);
    std::printf("[selftest] 入队 Navigate %s（下一帧 Simulation 消费）\n", path);
}

void App::DrawSelfTestTexturePreview() {
    if (!m_selftestIconReady) {
        return;
    }
    const layout::ShellLayout* shell = m_world.TryResource<layout::ShellLayout>();
    if (shell == nullptr) {
        return;
    }
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const layout::Rect pageViewport = shell->pageViewport;
    const layout::Rect imageRect{pageViewport.Right() - 190.0f, pageViewport.Bottom() - 180.0f,
                                 170.0f, 150.0f};
    render::DrawImageContain(drawList, m_selftestIcon, imageRect);
    ImGui::PushFont(theme::F().regular, 13.0f);
    drawList->AddText(ImVec2(imageRect.x, imageRect.Bottom() + 6.0f),
                      theme::Colors().textMuted, "WIC 纹理自检：修剪指甲@2x.png (340x300)");
    ImGui::PopFont();
}

void App::WriteSelfTestReport() {
    const std::filesystem::path reportPath = std::filesystem::path(m_selftestDir) / "metrics.json";
    std::FILE* file = std::fopen(reportPath.string().c_str(), "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "[selftest] 无法写 metrics.json\n");
        return;
    }

    const WindowState* window = m_world.TryResource<WindowState>();
    std::fprintf(file, "{\n");
    std::fprintf(file, "  \"app\": \"%s\",\n", kAppTitle);
    std::fprintf(file, "  \"version\": \"%s\",\n", kAppVersion);
    std::fprintf(file, "  \"designResolution\": [%d, %d],\n", kDesignWidth, kDesignHeight);
    if (window != nullptr) {
        std::fprintf(file,
                     "  \"window\": {\"width\": %d, \"height\": %d, \"framebuffer\": [%d, %d], "
                     "\"contentScale\": %.3f, \"fullscreen\": %s},\n",
                     window->width, window->height, window->framebufferWidth,
                     window->framebufferHeight, window->contentScale,
                     window->fullscreen ? "true" : "false");
    }
    std::fprintf(file, "  \"chinese\": {\"glyphsOk\": %s, \"textWidthZhongzhouYanglao\": %.1f},\n",
                 m_hasChineseGlyph ? "true" : "false", m_chineseTextWidth);
    std::fprintf(file, "  \"commandQueueTimingOk\": %s,\n",
                 m_commandQueueTimingOk ? "true" : "false");
    std::fprintf(file, "  \"emptyData\": %s,\n", m_options.emptyData ? "true" : "false");
    std::fprintf(file, "  \"wicTextureOk\": %s,\n", m_selftestIconReady ? "true" : "false");
    std::fprintf(file,
                 "  \"font\": {\"regular\": \"%s\", \"bold\": \"%s\", \"fontSize\": %.1f, "
                 "\"fontSizeBase\": %.1f, \"fontsLoaded\": %d},\n",
                 theme::F().regular != nullptr ? theme::F().regular->GetDebugName() : "(null)",
                 theme::F().bold != nullptr ? theme::F().bold->GetDebugName() : "(null)",
                 m_fontSizeMeasured, m_fontSizeBaseMeasured,
                 ImGui::GetCurrentContext() != nullptr ? ImGui::GetIO().Fonts->Fonts.Size : 0);
    std::fprintf(file,
                 "  \"ecs\": {\"entities\": %zu, \"componentTypes\": %zu, \"resources\": %zu, "
                 "\"systems\": %zu},\n",
                 m_world.EntityCount(), m_world.ComponentTypeCount(), m_world.ResourceCount(),
                 m_scheduler.Count());
    std::fprintf(file, "  \"captures\": [\n");
    for (std::size_t index = 0; index < m_captures.size(); ++index) {
        const CaptureRecord& record = m_captures[index];
        std::fprintf(file,
                     "    {\"name\": \"%s\", \"window\": [%d, %d], \"framebuffer\": [%d, %d], "
                     "\"fullscreen\": %s, \"layout\": {\"sidebar\": %s, \"topbar\": %s, "
                     "\"content\": %s, \"pageViewport\": %s}}%s\n",
                     record.name.c_str(), record.windowWidth, record.windowHeight,
                     record.framebufferWidth, record.framebufferHeight,
                     record.fullscreen ? "true" : "false",
                     FormatRect(record.layout.sidebar).c_str(),
                     FormatRect(record.layout.topbar).c_str(),
                     FormatRect(record.layout.content).c_str(),
                     FormatRect(record.layout.pageViewport).c_str(),
                     index + 1 < m_captures.size() ? "," : "");
    }
    std::fprintf(file, "  ]\n}\n");
    std::fclose(file);

    std::printf("[selftest] metrics 已写入 %s\n", reportPath.string().c_str());
    std::printf("[selftest] 结论：中文=%s | 截图=%zu 张 | 实体=%zu | 系统=%zu\n",
                m_hasChineseGlyph ? "OK" : "失败", m_captures.size(), m_world.EntityCount(),
                m_scheduler.Count());
}

}  // namespace zzyl
