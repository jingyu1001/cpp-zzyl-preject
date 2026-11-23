#include "zzyl/platform/ImGuiLayer.h"

#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "zzyl/platform/Window.h"
#include "zzyl/render/Theme.h"

namespace zzyl::platform {
namespace {

struct FontCandidate {
    const char* path;
    const char* label;
};

// 中文字体候选：msyh.ttc（微软雅黑，FontNo=0）/ msyhbd.ttc（雅黑粗体）/ simhei.ttf（黑体）
constexpr FontCandidate kRegularFontCandidates[] = {
    {"C:\\Windows\\Fonts\\msyh.ttc", "Microsoft YaHei"},
    {"C:\\Windows\\Fonts\\msyhbd.ttc", "Microsoft YaHei Bold"},
    {"C:\\Windows\\Fonts\\simhei.ttf", "SimHei"},
    {"C:\\Windows\\Fonts\\simsun.ttc", "SimSun"},
};

constexpr FontCandidate kBoldFontCandidates[] = {
    {"C:\\Windows\\Fonts\\msyhbd.ttc", "Microsoft YaHei Bold"},
    {"C:\\Windows\\Fonts\\msyh.ttc", "Microsoft YaHei"},
};

constexpr float kBaseFontSize = 14.0f;  // Web UI 规格：正文 14px

}  // namespace

ImGuiLayer::~ImGuiLayer() { Shutdown(); }

bool ImGuiLayer::FileExists(const char* path) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    std::fclose(file);
    return true;
}

bool ImGuiLayer::Init(Window& window) {
    if (m_initialized) {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // 禁用 imgui.ini，窗口布局全部由代码控制（不写盘）
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    zzyl::theme::ApplyStyle();
    ImGui::GetStyle().FontSizeBase = kBaseFontSize;

    if (!ImGui_ImplGlfw_InitForOpenGL(window.Handle(), true)) {
        std::fprintf(stderr, "[ImGui] ImGui_ImplGlfw_InitForOpenGL 失败\n");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::fprintf(stderr, "[ImGui] ImGui_ImplOpenGL3_Init 失败\n");
        return false;
    }

    LoadFonts();
    m_initialized = true;
    std::printf("[ImGui] 字体就绪：regular=%s bold=%s\n",
                m_regularFont ? m_regularFont->GetDebugName() : "(null)",
                m_boldFont ? m_boldFont->GetDebugName() : "(null)");
    return true;
}

void ImGuiLayer::LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig regularConfig;
    regularConfig.FontNo = 0;  // .ttc 内第 0 个字体（微软雅黑）
    regularConfig.OversampleH = 2;
    regularConfig.OversampleV = 1;
    regularConfig.PixelSnapH = true;

    for (const FontCandidate& candidate : kRegularFontCandidates) {
        if (!FileExists(candidate.path)) {
            continue;
        }
        m_regularFont = io.Fonts->AddFontFromFileTTF(candidate.path, kBaseFontSize, &regularConfig);
        if (m_regularFont != nullptr) {
            std::printf("[ImGui] 中文字体加载成功：%s (%s)\n", candidate.label, candidate.path);
            break;
        }
    }

    // 粗体（标题用）：msyhbd.ttc 优先，失败则复用常规字体。
    ImFontConfig boldConfig = regularConfig;
    for (const FontCandidate& candidate : kBoldFontCandidates) {
        if (!FileExists(candidate.path)) {
            continue;
        }
        m_boldFont = io.Fonts->AddFontFromFileTTF(candidate.path, kBaseFontSize, &boldConfig);
        if (m_boldFont != nullptr) {
            break;
        }
    }

    if (m_regularFont == nullptr) {
        std::fprintf(stderr, "[ImGui] 警告：未能加载任何中文字体，回退 ImGui 内置字体（中文将显示为方块）\n");
        m_regularFont = io.Fonts->AddFontDefault();
    }
    if (m_boldFont == nullptr) {
        m_boldFont = m_regularFont;
    }

    io.FontDefault = m_regularFont;
    zzyl::theme::SetFonts(m_regularFont, m_boldFont);
    ImGui::GetStyle().FontSizeBase = kBaseFontSize;
}

void ImGuiLayer::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::RenderDrawData() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::Shutdown() {
    if (m_initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_initialized = false;
    }
}

bool ImGuiLayer::HasGlyph(unsigned int codepoint) const {
    ImFontBaked* baked = ImGui::GetFontBaked();
    if (baked == nullptr) {
        return false;
    }
    return baked->FindGlyphNoFallback(static_cast<ImWchar>(codepoint)) != nullptr;
}

}  // namespace zzyl::platform
