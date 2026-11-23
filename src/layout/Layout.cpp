#include "zzyl/layout/Layout.h"

namespace zzyl::layout {

ShellLayout ComputeShellLayout(float width, float height, float contentScale, bool sidebarCollapsed) {
    ShellLayout layout;
    layout.contentScale = contentScale;
    layout.sidebarCollapsed = sidebarCollapsed;
    layout.tokens = LayoutTokens::FromViewport(width, height);

    const LayoutTokens& tokens = layout.tokens;
    layout.viewport = Rect{0.0f, 0.0f, width, height};

    // 侧边栏：固定宽（展开 200 / 折叠 54），全高
    const float sidebarWidth = sidebarCollapsed ? tokens.sidebarCollapsedWidth : tokens.sidebarWidth;
    layout.sidebar = Rect{0.0f, 0.0f, sidebarWidth, height};
    layout.sidebarLogo = Rect{0.0f, 0.0f, sidebarWidth, tokens.sidebarLogoHeight};
    layout.sidebarMenu = Rect{0.0f, layout.sidebarLogo.Bottom(), sidebarWidth,
                              std::max(0.0f, height - layout.sidebarLogo.Bottom())};

    const float rightWidth = std::max(0.0f, width - sidebarWidth);
    // 顶栏（50）+ 标签栏（34）固定高度，内容区 = 剩余区域
    layout.topbar = Rect{sidebarWidth, 0.0f, rightWidth, tokens.topbarHeight};
    layout.tagsView = Rect{sidebarWidth, tokens.topbarHeight, rightWidth, tokens.tagsViewHeight};

    const float chromeHeight = tokens.topbarHeight + tokens.tagsViewHeight;
    layout.content = Rect{sidebarWidth, chromeHeight, rightWidth,
                          std::max(0.0f, height - chromeHeight)};

    const EdgeInsets padding{tokens.contentPaddingLeft, tokens.contentPaddingTop,
                             tokens.contentPaddingRight, tokens.contentPaddingBottom};
    layout.pageViewport = Inset(layout.content, padding);
    return layout;
}

}  // namespace zzyl::layout
