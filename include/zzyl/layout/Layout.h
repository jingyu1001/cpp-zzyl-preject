#pragma once

// =============================================================================
// 布局系统（响应式；外壳尺寸与留白来自 Web 端 UI 复刻规格，页面内元素按比例锚定）
//
// 依据：docs/architecture/UI复刻规格_Cpp_1280x720_v0.1.md（Web 端架构师产出）
//  - 外壳：侧边栏固定 200（折叠 54）、顶栏固定 50、标签栏固定 34——这是 Web 规格
//    明确规定的"布局约束"，不随窗口拉伸；窗口缩放时由内容区与卡片按公式重排。
//  - 页面：内容区 padding 上 24 / 右 15 / 下 52 / 左 24（固定留白）。
//  - 卡片：尺寸/位置按内容区宽度公式与固定行高重排（见 §8.1 公式）。
//
// 硬约束：页面内任何元素都不允许出现"绝对像素位置"；要么用 AnchorLayout 归一化
// 锚定，要么用 layout 工具函数从父矩形推导。
// =============================================================================

#include <algorithm>
#include <cmath>

namespace zzyl::layout {

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    float CenterX() const { return x + w * 0.5f; }
    float CenterY() const { return y + h * 0.5f; }
    bool Empty() const { return w <= 0.0f || h <= 0.0f; }

    bool Contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct EdgeInsets {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

inline Rect Inset(Rect rect, const EdgeInsets& edge) {
    rect.x += edge.left;
    rect.y += edge.top;
    rect.w = std::max(0.0f, rect.w - edge.left - edge.right);
    rect.h = std::max(0.0f, rect.h - edge.top - edge.bottom);
    return rect;
}

inline Rect CutTop(Rect& rect, float amount) {
    const float height = std::min(amount, rect.h);
    const Rect out{rect.x, rect.y, rect.w, height};
    rect.y += height;
    rect.h -= height;
    return out;
}

inline Rect CutBottom(Rect& rect, float amount) {
    const float height = std::min(amount, rect.h);
    rect.h -= height;
    return Rect{rect.x, rect.y + rect.h, rect.w, height};
}

inline Rect CutLeft(Rect& rect, float amount) {
    const float width = std::min(amount, rect.w);
    const Rect out{rect.x, rect.y, width, rect.h};
    rect.x += width;
    rect.w -= width;
    return out;
}

inline Rect CutRight(Rect& rect, float amount) {
    const float width = std::min(amount, rect.w);
    rect.w -= width;
    return Rect{rect.x + rect.w, rect.y, width, rect.h};
}

inline float Clamp(float value, float low, float high) {
    return std::max(low, std::min(high, value));
}

// 网格切分：在 area 内按 cols x rows 均匀切分，格间留 gap。
inline Rect GridCell(const Rect& area, int col, int row, int cols, int rows, float gapX, float gapY) {
    if (cols <= 0 || rows <= 0) {
        return area;
    }
    const float cellW = (area.w - gapX * static_cast<float>(cols - 1)) / static_cast<float>(cols);
    const float cellH = (area.h - gapY * static_cast<float>(rows - 1)) / static_cast<float>(rows);
    const float x = area.x + static_cast<float>(col) * (cellW + gapX);
    const float y = area.y + static_cast<float>(row) * (cellH + gapY);
    return Rect{x, y, std::max(0.0f, cellW), std::max(0.0f, cellH)};
}

// 布局令牌（来源：Web UI 复刻规格 §2/§3/§4/§8；固定项为规格规定的布局约束）
struct LayoutTokens {
    // 外壳
    float topbarHeight = 50.0f;
    float tagsViewHeight = 34.0f;
    float sidebarWidth = 200.0f;
    float sidebarCollapsedWidth = 54.0f;
    float sidebarLogoHeight = 50.0f;
    float sidebarItemHeight = 56.0f;     // 一级菜单
    float sidebarSubItemHeight = 50.0f;  // 二级菜单

    // 内容留白（固定，来自规格 §3.1：上 24 / 右 15 / 下 52 / 左 24）
    float contentPaddingTop = 24.0f;
    float contentPaddingRight = 15.0f;
    float contentPaddingBottom = 52.0f;
    float contentPaddingLeft = 24.0f;

    // 视觉
    float cardRadius = 4.0f;
    float loginCardRadius = 6.0f;
    float cardHeaderHeight = 41.0f;
    float baseFontSize = 14.0f;

    // 间距（规格 §8.1）
    float rowGap = 16.0f;           // 卡片行间
    float twoColumnGap = 24.0f;     // 两栏 gutter
    float threeColumnGap = 12.0f;   // 三栏 gutter

    static LayoutTokens FromViewport(float /*width*/, float /*height*/) {
        // 外壳尺寸为规格固定值，不随窗口缩放；窗口缩放由内容区/卡片公式重排。
        return LayoutTokens{};
    }
};

// 应用外壳布局（每帧由 LayoutSystem 从窗口尺寸计算）
struct ShellLayout {
    Rect viewport;
    Rect sidebar;        // 整个侧边栏（含 Logo 区）
    Rect sidebarLogo;    // 侧边栏顶部 Logo 区（高 50）
    Rect sidebarMenu;    // 侧边栏菜单滚动区
    Rect topbar;         // 顶栏（侧边栏右侧，高 50）
    Rect tagsView;       // 标签栏（顶栏下方，高 34）
    Rect content;        // 内容区（顶栏+标签栏下方、侧边栏右侧）
    Rect pageViewport;   // 内容区去掉固定 padding 后，页面可用区域

    LayoutTokens tokens;
    float contentScale = 1.0f;
    bool sidebarCollapsed = false;
};

// 计算应用外壳布局。
// width/height 为窗口逻辑尺寸（GLFW window size），contentScale 为 DPI 缩放。
ShellLayout ComputeShellLayout(float width, float height, float contentScale, bool sidebarCollapsed);

}  // namespace zzyl::layout
