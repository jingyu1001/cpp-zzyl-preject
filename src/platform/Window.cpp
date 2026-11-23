#include "zzyl/platform/Window.h"

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

#include "glad/glad.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace zzyl::platform {
namespace {

void SetPerMonitorDpiAwareness() {
#ifdef _WIN32
    // 必须在 glfwInit 之前调用：声明为 Per-Monitor V2，避免 Windows 位图拉伸发虚。
    // 失败（例如被宿主已设置）不影响启动。
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
}

void GlfwErrorCallback(int code, const char* description) {
    std::fprintf(stderr, "[GLFW] error %d: %s\n", code, description ? description : "(null)");
}

}  // namespace

Window::~Window() { Destroy(); }

bool Window::Create(const WindowConfig& config) {
    SetPerMonitorDpiAwareness();
    glfwSetErrorCallback(GlfwErrorCallback);

    if (glfwInit() == GLFW_FALSE) {
        std::fprintf(stderr, "[Window] glfwInit 失败\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);  // 窗口固定 1280x720 物理像素；DPI 感知但不随缩放改变像素尺寸
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 0);

    m_handle = glfwCreateWindow(config.width, config.height, config.title, nullptr, nullptr);
    if (m_handle == nullptr) {
        std::fprintf(stderr, "[Window] 创建窗口失败（OpenGL 3.3 上下文不可用？）\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_handle);
    const int gladResult = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
    if (gladResult == 0) {
        std::fprintf(stderr, "[Window] glad 初始化 OpenGL 函数指针失败\n");
        Destroy();
        return false;
    }

    if (config.vsync) {
        glfwSwapInterval(1);
    }

    // 允许缩放但限制最小窗口，保证布局不崩（Web UI 规格建议最小 1024×640）。
    glfwSetWindowSizeLimits(m_handle, 1024, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);

    m_windowedWidth = config.width;
    m_windowedHeight = config.height;
    glfwGetWindowPos(m_handle, &m_windowedX, &m_windowedY);

    std::printf("[Window] OpenGL %s | GLSL %s | %dx%d\n",
                reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)),
                config.width,
                config.height);
    return true;
}

void Window::Destroy() {
    if (m_handle != nullptr) {
        glfwDestroyWindow(m_handle);
        m_handle = nullptr;
    }
    glfwTerminate();
}

bool Window::ShouldClose() const {
    return m_handle == nullptr || glfwWindowShouldClose(m_handle) == GLFW_TRUE;
}

bool Window::IsMinimized() const {
    return m_handle != nullptr && glfwGetWindowAttrib(m_handle, GLFW_ICONIFIED) == GLFW_TRUE;
}

void Window::PollEvents() {
    glfwPollEvents();
    if (m_handle == nullptr) {
        return;
    }

    // F11：切换全屏；ESC：退出全屏（与 Web 端全屏按钮语义一致）。
    const bool f11Down = glfwGetKey(m_handle, GLFW_KEY_F11) == GLFW_PRESS;
    if (f11Down && !m_f11Down) {
        ToggleFullscreen();
    }
    m_f11Down = f11Down;

    const bool escDown = glfwGetKey(m_handle, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escDown && !m_escDown && m_fullscreen) {
        ToggleFullscreen();
    }
    m_escDown = escDown;
}

void Window::SwapBuffers() {
    if (m_handle != nullptr) {
        glfwSwapBuffers(m_handle);
    }
}

void Window::ToggleFullscreen() {
    if (m_handle == nullptr) {
        return;
    }
    if (!m_fullscreen) {
        glfwGetWindowPos(m_handle, &m_windowedX, &m_windowedY);
        glfwGetWindowSize(m_handle, &m_windowedWidth, &m_windowedHeight);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (monitor != nullptr && mode != nullptr) {
            glfwSetWindowMonitor(m_handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            m_fullscreen = true;
        }
    } else {
        RestoreWindowed();
    }
    std::printf("[Window] 全屏=%s\n", m_fullscreen ? "开" : "关");
}

void Window::RestoreWindowed() {
    if (m_handle == nullptr) {
        return;
    }
    glfwSetWindowMonitor(m_handle, nullptr, m_windowedX, m_windowedY, m_windowedWidth, m_windowedHeight, 0);
    m_fullscreen = false;
}

int Window::Width() const {
    if (m_handle == nullptr) {
        return 0;
    }
    int width = 0;
    int height = 0;
    glfwGetWindowSize(m_handle, &width, &height);
    return width;
}

int Window::Height() const {
    if (m_handle == nullptr) {
        return 0;
    }
    int width = 0;
    int height = 0;
    glfwGetWindowSize(m_handle, &width, &height);
    return height;
}

int Window::FramebufferWidth() const {
    if (m_handle == nullptr) {
        return 0;
    }
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_handle, &width, &height);
    return width;
}

int Window::FramebufferHeight() const {
    if (m_handle == nullptr) {
        return 0;
    }
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_handle, &width, &height);
    return height;
}

float Window::ContentScale() const {
    if (m_handle == nullptr) {
        return 1.0f;
    }
    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(m_handle, &xscale, &yscale);
    return xscale > 0.0f ? xscale : 1.0f;
}

}  // namespace zzyl::platform
