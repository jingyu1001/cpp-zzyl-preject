#pragma once

// =============================================================================
// 平台层：GLFW 窗口（OpenGL 3.3 Core）
// 职责：进程 DPI 感知、窗口创建/销毁、事件轮询、F11 全屏切换、尺寸查询。
// 不包含任何 UI / 业务逻辑。
// =============================================================================

// GLFWwindow 为全局类型，不能声明在命名空间内
struct GLFWwindow;

namespace zzyl::platform {

struct WindowConfig {
    int width = 1280;
    int height = 720;
    const char* title = "中州养老 - C++ 转译端";
    bool visible = true;
    bool vsync = true;
};

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(const WindowConfig& config);
    void Destroy();

    bool ShouldClose() const;
    bool IsMinimized() const;

    // 轮询事件；内部处理 F11（全屏切换）+ ESC（退出全屏）。
    void PollEvents();
    void SwapBuffers();

    void ToggleFullscreen();
    bool IsFullscreen() const { return m_fullscreen; }

    // 逻辑尺寸（DPI 感知后为屏幕坐标；本项目 1:1 像素）
    int Width() const;
    int Height() const;
    // 实际帧缓冲像素尺寸（高 DPI 下可能大于 Width/Height）
    int FramebufferWidth() const;
    int FramebufferHeight() const;
    float ContentScale() const;

    GLFWwindow* Handle() const { return m_handle; }

private:
    void RestoreWindowed();

    GLFWwindow* m_handle = nullptr;
    bool m_fullscreen = false;
    bool m_f11Down = false;
    bool m_escDown = false;
    int m_windowedX = 0;
    int m_windowedY = 0;
    int m_windowedWidth = 1280;
    int m_windowedHeight = 720;
};

}  // namespace zzyl::platform
