#pragma once

// =============================================================================
// 平台层：ImGui 上下文 + 字体 + 主题样式初始化
//
// 字体策略：优先加载微软雅黑（msyh.ttc，FontNo=0），中文由 ImGui 1.92 动态
// 字形加载按需光栅化（无需 glyph ranges）；失败时回退 msyhbd.ttc / simhei.ttf /
// ImGui 内置字体。
// =============================================================================

struct ImFont;

namespace zzyl {
class World;
}

namespace zzyl::platform {

class Window;

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    bool Init(Window& window);
    void Shutdown();

    void BeginFrame();        // backend NewFrame + ImGui::NewFrame
    void RenderDrawData();    // ImGui::Render + 后端渲染（调用前需 glViewport/glClear）

    ImFont* RegularFont() const { return m_regularFont; }
    ImFont* BoldFont() const { return m_boldFont; }

    // 在帧内调用：检查当前字体是否包含指定 Unicode 码点（用于自测报告中文是否可用）
    bool HasGlyph(unsigned int codepoint) const;

private:
    void LoadFonts();
    static bool FileExists(const char* path);

    ImFont* m_regularFont = nullptr;
    ImFont* m_boldFont = nullptr;
    bool m_initialized = false;
};

}  // namespace zzyl::platform
