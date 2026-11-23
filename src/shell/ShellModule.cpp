// =============================================================================
// 应用外壳（cpp-dev1 负责）：侧边栏（Logo + 菜单树）/ 顶栏 / 标签栏
//
// 设计要点（依据 docs/tasks/Sprint1_cpp-dev1_应用外壳.md 与架构设计 §3/§6）：
//  - 数据只读：ShellLayout（每帧布局）/ RouteState（当前页）/ AuthSession（用户）/
//    MenuCatalog（菜单树）；交互一律入队 CommandQueue（Navigate），不直接写路由/会话；
//  - 外壳私有表现状态 ShellUiState（菜单展开项、滚动偏移）在 RegisterShellModule 中
//    注册为 World 资源，不使用全局静态变量；
//  - 所有容器坐标一律从 ShellLayout / LayoutTokens / 锚定矩形推导；
//    spec 命名空间里只有 UI 复刻规格规定的"视觉尺寸"（图标/头像/字号等固定值）；
//  - 字体作用域一律用 ui::ScopedFontSize / ScopedBoldFont，并在 EndFixedWindow 前结束；
//  - 菜单总高超出 sidebarMenu 时：鼠标滚轮滚动 + DrawList 裁剪 + 自绘细滚动条；
//  - 折叠态只显示图标（居中）、hover 出中文 tooltip；点击含子项的一级菜单会恢复展开。
// =============================================================================

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"

#include "zzyl/app/AppState.h"
#include "zzyl/app/ModuleHooks.h"
#include "zzyl/core/System.h"
#include "zzyl/data/MenuCatalog.h"
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"

namespace zzyl {
namespace {

// =============================================================================
// 外壳私有表现状态（纯 UI 状态，注册为 World 资源）
// =============================================================================
struct ShellUiState {
    // 同一时间只展开一个子菜单（Web 端 el-menu unique-opened 行为）
    int expandedRoot = -1;
    // 侧边栏菜单滚动偏移（像素，0 = 顶部）
    float menuScroll = 0.0f;
    // 下一帧把该一级菜单（连同子项）滚入可视区（展开时用）
    int scrollToRoot = -1;
    // 内容区滚动（外壳作为内容视口所有者维护；超出 pageViewport 高度才可滚）
    float contentScroll = 0.0f;
    float contentMaxScroll = 0.0f;
};

// 外壳视觉常量：均来自《UI复刻规格 §4/§5/§6》的固定视觉尺寸。
// 注意：这里只放"尺寸/内边距"级别常量，任何容器坐标都必须由 ShellLayout 推导。
namespace spec {
constexpr float kSidebarPadding = 16.0f;        // 侧边栏左右内边距（Logo/菜单图标左边距）
constexpr float kLogoSize = 32.0f;             // Logo 图 32×32
constexpr float kLogoTextGap = 12.0f;          // Logo 与标题间距
constexpr float kMenuIconSize = 14.0f;         // 菜单图标 14×14
constexpr float kMenuIconLeft = 20.0f;         // 一级菜单图标左边距
constexpr float kMenuTextLeft = 50.0f;         // 一级菜单文字起点 x=50
constexpr float kMenuChevronReserve = 44.0f;   // 一级项右侧给展开箭头留的宽度
constexpr float kChevronRightInset = 24.0f;    // 箭头中心距菜单右边缘
constexpr float kChevronSize = 9.0f;           // 展开箭头尺寸
constexpr float kSubMenuIconLeft = 40.0f;      // 二级菜单图标左缩进（规格 §4.1：左缩进 40）
constexpr float kSubMenuItemIndent = 70.0f;   // 二级菜单文字起点（图标 40 + 14 + 16 间距）
constexpr float kSubItemRightPadding = 20.0f;  // 二级菜单右内边距
constexpr float kMenuTopPadding = 6.0f;        // 菜单内容顶部留白
constexpr float kMenuWheelStep = 0.55f;        // 滚轮步长（一级菜单项高的倍数）
constexpr float kScrollbarWidth = 3.0f;        // 侧边栏菜单细滚动条宽
constexpr float kScrollbarInset = 3.0f;        // 滚动条与菜单边缘间距
constexpr float kScrollbarMinThumb = 24.0f;    // 滚动条滑块最小高度
constexpr float kContentScrollbarWidth = 6.0f;    // 内容区滚动条宽（规格 §3.3：6px）
constexpr float kContentScrollbarInset = 2.0f;    // 内容区滚动条右边距
constexpr float kContentScrollbarMinThumb = 30.0f;  // 内容区滑块最小高度
constexpr float kContentWheelStepRatio = 4.0f;    // 内容区滚轮步长 = rowGap * 4
constexpr float kBreadcrumbLeftGap = 8.0f;     // 汉堡键 -> 面包屑间距
constexpr float kBreadcrumbRightGap = 16.0f;   // 面包屑 -> 右侧功能区间距
constexpr float kAvatarRatio = 0.6f;           // 头像直径 / 顶栏高（50 * 0.6 = 30）
constexpr float kTopbarRightPadding = 15.0f;   // 顶栏右侧留白（与内容区右留白一致）
constexpr float kUserAreaGap = 8.0f;           // 头像与昵称间距
constexpr float kUserHitPadding = 6.0f;        // 用户区热区左右扩展
constexpr float kHintGap = 16.0f;              // 快捷键提示与用户区间距
constexpr float kPopupWidth = 240.0f;          // 顶栏用户区弹窗宽
constexpr float kPopupGap = 6.0f;              // 弹窗与顶栏底边间距
constexpr float kPopupPadding = 12.0f;         // 弹窗内边距
constexpr float kPopupMaxSize = 10000.0f;      // 弹窗高度不设上限（自动高度）
constexpr float kHamburgerLineRatio = 0.4f;    // 汉堡线宽 / 按钮边长（20/50）
constexpr float kHamburgerSpacingRatio = 0.1f; // 汉堡线间距 / 按钮边长（5/50）
constexpr float kHamburgerThickness = 1.6f;    // 汉堡线粗
constexpr float kTagVerticalInset = 4.0f;      // 标签上下间距（标签高 26 = 34 - 8）
constexpr float kTagLeftPadding = 8.0f;         // 标签左右内边距
constexpr float kTagDotSize = 8.0f;            // 标签白点 8×8
constexpr float kTagDotGap = 5.0f;             // 白点与文字间距
constexpr float kTagFirstLeft = 15.0f;         // 首个标签左边距
constexpr float kFontSmallOffset = 2.0f;       // 辅助字号 = 基准字号 - 2
}  // namespace spec

// =============================================================================
// 小工具
// =============================================================================

// 本迭代已实现的路由：首页看板（RouteSystem 同时接受 /index 与 /dashboard）
bool IsRoutableRoute(const std::string& route) {
    return route == "/index" || route == "/dashboard";
}

// 当前路由是否命中该菜单项（首页高亮）
bool IsActiveMenuRoute(const std::string& route, const RouteState* state) {
    if (state == nullptr || state->current != Route::Dashboard) {
        return false;
    }
    return route == "/index" || route == "/dashboard";
}

const char* PageTitleForRoute(const RouteState* state) {
    if (state != nullptr && state->current == Route::Dashboard) {
        return "首页";
    }
    return "登录";
}

void EnqueueNavigate(World& world, const std::string& route) {
    if (auto* queue = world.TryResource<CommandQueue>()) {
        CommandQueue::Command command;
        command.type = CommandQueue::Type::Navigate;
        command.arg0 = route;
        queue->commands.push_back(std::move(command));
    }
}

// UTF-8 安全截断：超出宽度时截断并追加省略号（ImDrawList::AddText 不会自动换行/省略）
std::string EllipsizeToWidth(const char* text, float maxWidth) {
    if (text == nullptr || maxWidth <= 0.0f) {
        return std::string();
    }
    const std::string full(text);
    if (ImGui::CalcTextSize(full.c_str()).x <= maxWidth) {
        return full;
    }
    const char* ellipsis = "…";
    const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;
    if (maxWidth <= ellipsisWidth) {
        return std::string(ellipsis);
    }
    std::size_t length = 0;
    while (text[length] != '\0') {
        std::size_t next = length + 1;
        // 跳过 UTF-8 续字节，保证不会把中文切成半个字符
        while (text[next] != '\0' && (static_cast<unsigned char>(text[next]) & 0xC0U) == 0x80U) {
            ++next;
        }
        const std::string candidate(text, next);
        if (ImGui::CalcTextSize(candidate.c_str()).x + ellipsisWidth > maxWidth) {
            break;
        }
        length = next;
    }
    return std::string(text, length) + ellipsis;
}

// =============================================================================
// 图标绘制（Sprint 1 用几何占位；真实 SVG 图标资源 Sprint 2 接入）
// =============================================================================
void DrawHamburger(ImDrawList* drawList, const layout::Rect& rect, ImU32 color) {
    const float lineWidth = rect.h * spec::kHamburgerLineRatio;
    const float spacing = rect.h * spec::kHamburgerSpacingRatio;
    const float centerX = rect.CenterX();
    const float centerY = rect.CenterY();
    for (int index = -1; index <= 1; ++index) {
        const float y = centerY + static_cast<float>(index) * spacing;
        ui::Line(drawList, centerX - lineWidth * 0.5f, y, centerX + lineWidth * 0.5f, y, color,
                 spec::kHamburgerThickness);
    }
}

// 展开箭头：收起时朝下、展开后旋转 180° 朝上（对齐 Web 截图 02/06 的 el-sub-menu 箭头）
void DrawChevron(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, bool expanded) {
    const float angle = expanded ? -1.5707963f : 1.5707963f;
    const float dirX = std::cos(angle);
    const float dirY = std::sin(angle);
    const float perpX = -dirY;
    const float perpY = dirX;
    const float half = size * 0.5f;
    const ImVec2 tip(center.x + dirX * half, center.y + dirY * half);
    const ImVec2 tailA(center.x - dirX * half * 0.55f + perpX * half * 0.62f,
                       center.y - dirY * half * 0.55f + perpY * half * 0.62f);
    const ImVec2 tailB(center.x - dirX * half * 0.55f - perpX * half * 0.62f,
                       center.y - dirY * half * 0.55f - perpY * half * 0.62f);
    const ImVec2 points[3] = {tailA, tip, tailB};
    drawList->AddPolyline(points, 3, color, 0, 1.4f);
}

// 菜单图标占位：14×14 圆角方块
void DrawMenuIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float half = size * 0.5f;
    const layout::Rect iconRect{center.x - half, center.y - half, size, size};
    ui::FillRounded(drawList, iconRect, color, size * 0.22f);
}

// =============================================================================
// 菜单行布局（根菜单 + 当前展开的子菜单，滚动/裁剪基于该结果）
// =============================================================================
enum class MenuRowKind {
    Root,
    Child,
};

struct MenuRow {
    MenuRowKind kind = MenuRowKind::Root;
    int rootIndex = -1;
    int childIndex = -1;
    const data::MenuEntry* entry = nullptr;
    float top = 0.0f;     // 相对菜单内容顶部的偏移（未含滚动）
    float height = 0.0f;
};

struct MenuLayout {
    std::vector<MenuRow> rows;
    float contentHeight = 0.0f;
};

MenuLayout BuildMenuLayout(const data::MenuCatalog& catalog, int expandedRoot, bool collapsed,
                           const layout::LayoutTokens& tokens) {
    MenuLayout menu;
    float top = 0.0f;
    for (std::size_t rootIndex = 0; rootIndex < catalog.roots.size(); ++rootIndex) {
        const data::MenuEntry& root = catalog.roots[rootIndex];
        MenuRow row;
        row.kind = MenuRowKind::Root;
        row.rootIndex = static_cast<int>(rootIndex);
        row.entry = &root;
        row.top = top;
        row.height = tokens.sidebarItemHeight;
        menu.rows.push_back(row);
        top += row.height;

        const bool expanded =
            !collapsed && root.HasChildren() && expandedRoot == static_cast<int>(rootIndex);
        if (!expanded) {
            continue;
        }
        for (std::size_t childIndex = 0; childIndex < root.children.size(); ++childIndex) {
            MenuRow child;
            child.kind = MenuRowKind::Child;
            child.rootIndex = static_cast<int>(rootIndex);
            child.childIndex = static_cast<int>(childIndex);
            child.entry = &root.children[childIndex];
            child.top = top;
            child.height = tokens.sidebarSubItemHeight;
            menu.rows.push_back(child);
            top += child.height;
        }
    }
    menu.contentHeight = top;
    return menu;
}

// -----------------------------------------------------------------------------
// ShellRenderSystem：应用外壳（顶栏 / 侧边栏 / 标签栏）
// 依据 UI复刻规格 §3/§4/§5/§6：
//  - 侧边栏 200（折叠 54）深色 #304156，Logo 区 50，一级菜单 56 / 二级 50
//  - 顶栏 50 白底：汉堡键 + 面包屑 + 快捷键提示 + 用户区（头像/昵称）
//  - 标签栏 34 白底：首页固定标签 #00B8A0 白字 + 8px 白点
//  - 内容区只画底色，具体页面由页面渲染系统负责（不遮挡页面）
// -----------------------------------------------------------------------------
class ShellRenderSystem final : public ISystem {
public:
    const char* Name() const override { return "ShellRenderSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& /*frame*/) override {
        const RouteState* route = world.TryResource<RouteState>();
        if (route == nullptr || route->current == Route::Login) {
            return;  // 登录页是独立全屏页面，不绘制外壳
        }
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        if (shell == nullptr || shell->viewport.Empty()) {
            return;
        }
        const theme::Palette& colors = theme::Colors();
        const UiState* ui = world.TryResource<UiState>();
        const bool collapsed = ui != nullptr && ui->sidebarCollapsed;

        // 背景层（先于所有固定窗口）：整窗 + 内容区底色
        ImDrawList* background = ImGui::GetBackgroundDrawList();
        ui::Fill(background, shell->viewport, colors.background);
        ui::Fill(background, shell->content, colors.background);

        // 内容区滚动容器：先按页面的锚定结果测量内容高度，再应用视口平移
        //（页面渲染系统在本系统之后运行，读到的 ScreenRect 已是滚动后的坐标）
        UpdateContentScroll(world, *shell);

        DrawSidebar(world, *shell, colors, collapsed, route);
        DrawTopbar(world, *shell, colors, route);
        DrawTagsView(world, *shell, colors);
        DrawContentScrollbar(world, *shell, colors);
    }

private:
    // --------------------------- 内容区滚动容器 ---------------------------
    // 外壳负责内容区容器（底色 + pageViewport + 细滚动条）；页面内容滚动有两条路径：
    //  1) 页面自建 ImGui 滚动子窗口（看板页当前实现：锚定满 pageViewport 后自己滚）；
    //  2) 页面用 AnchorLayout(space=PageViewport) 声明超出 pageViewport 的内容，由外壳
    //     统一测量并平移 ScreenRect（本文件的兜底路径）。
    // 两条路径互斥：路径 1 的页面锚定高度不会超过 pageViewport，因此本兜底不激活
    // （contentMaxScroll=0，不画滚动条、不消费滚轮），不会与页面滚动"双重滚动"。
    float MeasurePageContentHeight(World& world, const layout::ShellLayout& shell) const {
        float maxBottom = shell.pageViewport.y;
        world.Each<AnchorLayout, ScreenRect>(
            [&maxBottom](Entity /*entity*/, const AnchorLayout& anchor, const ScreenRect& rect) {
                if (anchor.space != AnchorSpace::PageViewport || rect.rect.Empty()) {
                    return;
                }
                maxBottom = std::max(maxBottom, rect.rect.Bottom());
            });
        return std::max(0.0f, maxBottom - shell.pageViewport.y);
    }

    // 页面实体的 ScreenRect 由 LayoutSystem 在 Layout 阶段按锚定重新解算（每帧基准值），
    // 因此这里只需要在当前基准上减去滚动偏移；不需要保存/恢复上一次的坐标。
    static void ApplyContentScroll(World& world, float offsetY) {
        if (offsetY <= 0.0f) {
            return;
        }
        world.Each<AnchorLayout, ScreenRect>(
            [offsetY](Entity /*entity*/, const AnchorLayout& anchor, ScreenRect& rect) {
                if (anchor.space != AnchorSpace::PageViewport || rect.rect.Empty()) {
                    return;
                }
                rect.rect.y -= offsetY;
            });
    }

    void UpdateContentScroll(World& world, const layout::ShellLayout& shell) {
        ShellUiState* shellUi = world.TryResource<ShellUiState>();
        if (shellUi == nullptr) {
            return;
        }
        const float contentHeight = MeasurePageContentHeight(world, shell);
        shellUi->contentMaxScroll = std::max(0.0f, contentHeight - shell.pageViewport.h);

        // 滚轮：仅鼠标位于内容区时消费（侧边栏区域由菜单自己处理）
        const ImVec2 contentMin(shell.content.x, shell.content.y);
        const ImVec2 contentMax(shell.content.Right(), shell.content.Bottom());
        if (shellUi->contentMaxScroll > 0.0f &&
            ImGui::IsMouseHoveringRect(contentMin, contentMax)) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                shellUi->contentScroll -=
                    wheel * shell.tokens.rowGap * spec::kContentWheelStepRatio;
            }
        }
        shellUi->contentScroll =
            layout::Clamp(shellUi->contentScroll, 0.0f, shellUi->contentMaxScroll);

        ApplyContentScroll(world, shellUi->contentScroll);
    }

    void DrawContentScrollbar(World& world, const layout::ShellLayout& shell,
                              const theme::Palette& colors) const {
        const ShellUiState* shellUi = world.TryResource<ShellUiState>();
        if (shellUi == nullptr || shellUi->contentMaxScroll <= 0.0f) {
            return;  // 内容不溢出：不画轨道，避免"死白条"
        }
        // RuoYi 风格：6px 细滚动条贴在内容区右缘，轨道浅灰、滑块中灰圆角
        const layout::Rect bar{shell.content.Right() - spec::kContentScrollbarWidth -
                                   spec::kContentScrollbarInset,
                               shell.content.y, spec::kContentScrollbarWidth, shell.content.h};
        if (bar.Empty()) {
            return;
        }
        if (!ui::BeginFixedWindow("##shell_content_scrollbar", bar)) {
            ui::EndFixedWindow();
            return;
        }
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ui::FillRounded(drawList, bar, colors.borderLight, spec::kContentScrollbarWidth * 0.5f);

        const float thumbHeight =
            std::max(spec::kContentScrollbarMinThumb,
                     bar.h * bar.h / (bar.h + shellUi->contentMaxScroll));
        const float travel = std::max(0.0f, bar.h - thumbHeight);
        const float thumbTop =
            bar.y + travel * layout::Clamp(shellUi->contentScroll / shellUi->contentMaxScroll,
                                           0.0f, 1.0f);
        const layout::Rect thumb{bar.x, thumbTop, bar.w, thumbHeight};
        ui::FillRounded(drawList, thumb, theme::WithAlpha(colors.textPlaceholder, 0.95f),
                        spec::kContentScrollbarWidth * 0.5f);
        ui::EndFixedWindow();
    }
    // ------------------------------ 侧边栏 ------------------------------
    void DrawSidebar(World& world, const layout::ShellLayout& shell, const theme::Palette& colors,
                     bool collapsed, const RouteState* route) {
        if (!ui::BeginFixedWindow("##shell_sidebar", shell.sidebar)) {
            ui::EndFixedWindow();
            return;
        }
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ui::Fill(drawList, shell.sidebar, colors.sidebar);

        DrawSidebarLogo(drawList, shell, colors, collapsed);
        DrawSidebarMenu(world, shell, colors, collapsed, route);

        ui::EndFixedWindow();
    }

    void DrawSidebarLogo(ImDrawList* drawList, const layout::ShellLayout& shell,
                         const theme::Palette& colors, bool collapsed) {
        const layout::Rect& logoArea = shell.sidebarLogo;
        if (logoArea.Empty()) {
            return;
        }
        ui::Fill(drawList, logoArea, colors.sidebarLogo);

        const float logoSize = spec::kLogoSize;
        const float logoX =
            collapsed ? logoArea.CenterX() - logoSize * 0.5f : logoArea.x + spec::kSidebarPadding;
        const layout::Rect logoRect{logoX, logoArea.CenterY() - logoSize * 0.5f, logoSize, logoSize};
        ui::FillRounded(drawList, logoRect, colors.primary, shell.tokens.cardRadius);
        {
            ui::ScopedBoldFont logoGlyphFont(shell.tokens.baseFontSize + spec::kFontSmallOffset);
            ui::TextCentered(drawList, logoRect, colors.textInverse, "中");
        }

        if (collapsed) {
            return;
        }
        const float textLeft = logoRect.Right() + spec::kLogoTextGap;
        const float textWidth = std::max(0.0f, logoArea.Right() - spec::kSidebarPadding - textLeft);
        if (textWidth <= 0.0f) {
            return;
        }
        const std::string title = EllipsizeToWidth("中州养老", textWidth);
        ui::ScopedBoldFont logoTitleFont(shell.tokens.baseFontSize);
        ui::TextLeftMiddle(drawList,
                           layout::Rect{textLeft, logoArea.y, textWidth, logoArea.h},
                           colors.textInverse, title.c_str());
    }

    void DrawSidebarMenu(World& world, const layout::ShellLayout& shell,
                         const theme::Palette& colors, bool collapsed, const RouteState* route) {
        const data::MenuCatalog* catalog = world.TryResource<data::MenuCatalog>();
        ShellUiState* shellUi = world.TryResource<ShellUiState>();
        if (catalog == nullptr || shellUi == nullptr || shell.sidebarMenu.Empty()) {
            return;
        }

        const layout::LayoutTokens& tokens = shell.tokens;
        const layout::Rect& menuArea = shell.sidebarMenu;
        const float topPadding = std::min(spec::kMenuTopPadding, tokens.sidebarItemHeight * 0.25f);

        // 折叠态不展开子菜单（展开状态保留在 ShellUiState，展开侧边栏后恢复）
        const int expandedRoot = collapsed ? -1 : shellUi->expandedRoot;
        const MenuLayout menu = BuildMenuLayout(*catalog, expandedRoot, collapsed, tokens);

        const float viewportContentHeight = std::max(0.0f, menuArea.h - topPadding * 2.0f);
        const float maxScroll = std::max(0.0f, menu.contentHeight - viewportContentHeight);

        // ---- 滚轮：仅鼠标位于菜单区域时消费；无滚动空间时不响应 ----
        const ImVec2 menuMin(menuArea.x, menuArea.y);
        const ImVec2 menuMax(menuArea.Right(), menuArea.Bottom());
        if (maxScroll > 0.0f && ImGui::IsMouseHoveringRect(menuMin, menuMax)) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                shellUi->menuScroll -= wheel * tokens.sidebarItemHeight * spec::kMenuWheelStep;
            }
        }

        // ---- 展开的菜单块自动滚入可视区（点击展开后生效）----
        if (shellUi->scrollToRoot >= 0) {
            float blockTop = 0.0f;
            float blockBottom = 0.0f;
            bool found = false;
            for (const MenuRow& row : menu.rows) {
                if (row.rootIndex != shellUi->scrollToRoot) {
                    continue;
                }
                if (!found) {
                    blockTop = row.top;
                    found = true;
                }
                blockBottom = row.top + row.height;
            }
            if (found) {
                if (blockTop < shellUi->menuScroll) {
                    shellUi->menuScroll = blockTop;
                } else if (blockBottom > shellUi->menuScroll + viewportContentHeight) {
                    shellUi->menuScroll = blockBottom - viewportContentHeight;
                }
            }
            shellUi->scrollToRoot = -1;
        }

        shellUi->menuScroll = layout::Clamp(shellUi->menuScroll, 0.0f, maxScroll);

        ImGui::SetCursorScreenPos(menuMin);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        // 可视裁剪：滚出菜单区的行不绘制，也不可点击（防止压到 Logo/顶栏上）
        drawList->PushClipRect(menuMin, menuMax, true);

        const float contentTop = menuArea.y + topPadding - shellUi->menuScroll;
        for (const MenuRow& row : menu.rows) {
            const layout::Rect rowRect{menuArea.x, contentTop + row.top, menuArea.w, row.height};
            if (rowRect.Bottom() <= menuArea.y || rowRect.y >= menuArea.Bottom()) {
                continue;
            }
            DrawMenuRow(world, shell, colors, collapsed, route, row, rowRect);
        }

        drawList->PopClipRect();

        DrawMenuScrollbar(drawList, shell, colors, viewportContentHeight, maxScroll,
                          shellUi->menuScroll);
    }

    void DrawMenuRow(World& world, const layout::ShellLayout& shell, const theme::Palette& colors,
                     bool collapsed, const RouteState* route, const MenuRow& row,
                     const layout::Rect& rowRect) {
        if (row.entry == nullptr) {
            return;
        }
        ShellUiState* shellUi = world.TryResource<ShellUiState>();
        UiState* ui = world.TryResource<UiState>();
        if (shellUi == nullptr) {
            return;
        }
        const layout::LayoutTokens& tokens = shell.tokens;
        const layout::Rect& menuArea = shell.sidebarMenu;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const bool isRoot = row.kind == MenuRowKind::Root;
        const bool expanded = isRoot && shellUi->expandedRoot == row.rootIndex && !collapsed;

        // 二级菜单底色（一级菜单保持侧边栏底色，选中态不加色块，符合规格 §4.1）
        if (!isRoot) {
            ui::Fill(drawList, rowRect, colors.sidebarSub);
        }

        // 可点击热区裁剪到菜单可视区，避免滚出的行仍可被点击
        char buttonId[64];
        std::snprintf(buttonId, sizeof(buttonId), "##nav_%s_%d_%d", isRoot ? "root" : "sub",
                      row.rootIndex, row.childIndex);
        const float hitTop = std::max(rowRect.y, menuArea.y);
        const float hitBottom = std::min(rowRect.Bottom(), menuArea.Bottom());
        const layout::Rect hitRect{rowRect.x, hitTop, rowRect.w, std::max(0.0f, hitBottom - hitTop)};
        if (hitRect.Empty()) {
            return;
        }
        const bool pressed = ui::InvisibleButtonAt(buttonId, hitRect);
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) {
            ui::Fill(drawList, hitRect, isRoot ? colors.sidebarHover : colors.sidebarHover);
        }

        const bool active = isRoot && IsActiveMenuRoute(row.entry->route, route);
        const ImU32 textColor = active ? colors.sidebarTextActive : colors.sidebarText;

        if (isRoot) {
            const float iconCenterX = collapsed ? rowRect.CenterX()
                                                : rowRect.x + spec::kMenuIconLeft +
                                                      spec::kMenuIconSize * 0.5f;
            DrawMenuIcon(drawList, ImVec2(iconCenterX, rowRect.CenterY()), spec::kMenuIconSize,
                         textColor);

            if (!collapsed) {
                const float rightReserve =
                    row.entry->HasChildren() ? spec::kMenuChevronReserve : spec::kSidebarPadding;
                const float textLeft = rowRect.x + spec::kMenuTextLeft;
                const float textWidth =
                    std::max(0.0f, rowRect.Right() - rightReserve - textLeft);
                if (textWidth > 0.0f) {
                    const std::string title = EllipsizeToWidth(row.entry->title.c_str(), textWidth);
                    ui::ScopedFontSize menuFont(tokens.baseFontSize);
                    ui::TextLeftMiddle(drawList,
                                       layout::Rect{textLeft, rowRect.y, textWidth, rowRect.h},
                                       textColor, title.c_str());
                }
                if (row.entry->HasChildren()) {
                    DrawChevron(drawList,
                                ImVec2(rowRect.Right() - spec::kChevronRightInset, rowRect.CenterY()),
                                spec::kChevronSize, theme::WithAlpha(colors.sidebarText, 0.75f),
                                expanded);
                }
            }
        } else {
            const float iconCenterX =
                rowRect.x + spec::kSubMenuIconLeft + spec::kMenuIconSize * 0.5f;
            DrawMenuIcon(drawList, ImVec2(iconCenterX, rowRect.CenterY()), spec::kMenuIconSize,
                         textColor);
            const float textLeft = rowRect.x + spec::kSubMenuItemIndent;
            const float textWidth = std::max(
                0.0f, rowRect.Right() - spec::kSubItemRightPadding - textLeft);
            if (textWidth > 0.0f) {
                const std::string title = EllipsizeToWidth(row.entry->title.c_str(), textWidth);
                ui::ScopedFontSize subMenuFont(tokens.baseFontSize);
                ui::TextLeftMiddle(drawList,
                                   layout::Rect{textLeft, rowRect.y, textWidth, rowRect.h},
                                   textColor, title.c_str());
            }
        }

        // ---- 悬停提示 ----
        if (hovered) {
            if (isRoot && collapsed) {
                ImGui::SetTooltip("%s", row.entry->title.c_str());
            } else if (!isRoot && !IsRoutableRoute(row.entry->route)) {
                ImGui::SetTooltip("「%s」模块规划中，Sprint 2 接入", row.entry->title.c_str());
            }
        }

        // ---- 点击交互 ----
        if (!pressed) {
            return;
        }
        if (isRoot) {
            if (row.entry->HasChildren()) {
                if (collapsed) {
                    // 折叠态点击一级目录：恢复侧边栏并展开该目录（等价 Web 折叠态弹层）
                    if (ui != nullptr) {
                        ui->sidebarCollapsed = false;
                    }
                    shellUi->expandedRoot = row.rootIndex;
                    shellUi->scrollToRoot = row.rootIndex;
                } else if (shellUi->expandedRoot == row.rootIndex) {
                    shellUi->expandedRoot = -1;
                } else {
                    shellUi->expandedRoot = row.rootIndex;
                    shellUi->scrollToRoot = row.rootIndex;
                }
            } else if (IsRoutableRoute(row.entry->route)) {
                EnqueueNavigate(world, row.entry->route);
            }
        } else if (IsRoutableRoute(row.entry->route)) {
            EnqueueNavigate(world, row.entry->route);
        }
    }

    void DrawMenuScrollbar(ImDrawList* drawList, const layout::ShellLayout& shell,
                           const theme::Palette& colors, float viewportContentHeight,
                           float maxScroll, float scroll) const {
        if (maxScroll <= 0.0f || viewportContentHeight <= 0.0f) {
            return;  // 菜单内容未溢出：不画滚动条
        }
        const layout::Rect& menuArea = shell.sidebarMenu;
        const float width = spec::kScrollbarWidth;
        const float x = menuArea.Right() - spec::kScrollbarInset - width;
        const float trackTop = menuArea.y + spec::kScrollbarInset;
        const float trackHeight = std::max(0.0f, menuArea.h - spec::kScrollbarInset * 2.0f);
        if (trackHeight <= 0.0f) {
            return;
        }
        ui::FillRounded(drawList, layout::Rect{x, trackTop, width, trackHeight},
                        theme::WithAlpha(colors.textInverse, 0.10f), width * 0.5f);

        const float thumbHeight =
            std::max(spec::kScrollbarMinThumb,
                     trackHeight * viewportContentHeight / (viewportContentHeight + maxScroll));
        const float travel = std::max(0.0f, trackHeight - thumbHeight);
        const float thumbTop =
            trackTop + travel * layout::Clamp(scroll / maxScroll, 0.0f, 1.0f);
        ui::FillRounded(drawList, layout::Rect{x, thumbTop, width, thumbHeight},
                        theme::WithAlpha(colors.textInverse, 0.38f), width * 0.5f);
    }

    // ------------------------------ 顶栏 ------------------------------
    void DrawTopbar(World& world, const layout::ShellLayout& shell, const theme::Palette& colors,
                    const RouteState* route) {
        UiState* ui = world.TryResource<UiState>();
        const AuthSession* session = world.TryResource<AuthSession>();

        if (!ui::BeginFixedWindow("##shell_topbar", shell.topbar)) {
            ui::EndFixedWindow();
            return;
        }
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const layout::LayoutTokens& tokens = shell.tokens;
        const float smallFont = std::max(10.0f, tokens.baseFontSize - spec::kFontSmallOffset);

        ui::Fill(drawList, shell.topbar, colors.topbar);
        // 顶栏底部分割线（Web 为 0 1px 4px 阴影，这里用 1px 浅线近似）
        ui::Line(drawList, shell.topbar.x, shell.topbar.Bottom() - 0.5f, shell.topbar.Right(),
                 shell.topbar.Bottom() - 0.5f, theme::WithAlpha(colors.textPrimary, 0.08f), 1.0f);

        // ---- 汉堡键：与顶栏等高的正方形热区 ----
        const float hamburgerSize = std::min(shell.topbar.h, shell.topbar.w);
        const layout::Rect hamburger{shell.topbar.x, shell.topbar.y, hamburgerSize, hamburgerSize};
        const bool hamburgerPressed = ui::InvisibleButtonAt("##shell_hamburger", hamburger);
        if (ImGui::IsItemHovered()) {
            ui::Fill(drawList, hamburger, theme::WithAlpha(colors.textPrimary, 0.04f));
        }
        if (hamburgerPressed && ui != nullptr) {
            ui->sidebarCollapsed = !ui->sidebarCollapsed;
        }
        DrawHamburger(drawList, hamburger, colors.iconRegular);

        // ---- 右侧：昵称（最右） / 头像 / 快捷键提示 ----
        const char* nickname =
            (session != nullptr && session->loggedIn && !session->user.nickName.empty())
                ? session->user.nickName.c_str()
                : "未登录";
        float nicknameWidth = 0.0f;
        {
            ui::ScopedBoldFont nicknameFont(tokens.baseFontSize);
            nicknameWidth = ui::TextSize(nickname).x;
        }
        const float avatarDiameter =
            std::min(shell.topbar.h * spec::kAvatarRatio,
                     tokens.sidebarLogoHeight * spec::kAvatarRatio);
        const float rightEdge = shell.topbar.Right() - spec::kTopbarRightPadding;
        const float centerY = shell.topbar.CenterY();

        const layout::Rect nicknameRect{rightEdge - nicknameWidth, shell.topbar.y, nicknameWidth,
                                        shell.topbar.h};
        const layout::Rect avatarRect{rightEdge - nicknameWidth - spec::kUserAreaGap - avatarDiameter,
                                      centerY - avatarDiameter * 0.5f, avatarDiameter,
                                      avatarDiameter};

        const char* hint = "F11 全屏 · F1 调试";
        float hintWidth = 0.0f;
        {
            ui::ScopedFontSize hintFont(smallFont);
            hintWidth = ui::TextSize(hint).x;
        }
        const layout::Rect hintRect{avatarRect.x - spec::kHintGap - hintWidth, shell.topbar.y,
                                    hintWidth, shell.topbar.h};

        // 用户区（头像+昵称）热区：hover 高亮，点击弹出「个人中心（规划中）」
        const float userLeft = avatarRect.x - spec::kUserHitPadding;
        const layout::Rect userArea{userLeft, shell.topbar.y,
                                    (nicknameRect.Right() + spec::kUserHitPadding) - userLeft,
                                    shell.topbar.h};
        const bool userPressed = ui::InvisibleButtonAt("##shell_user", userArea);
        const bool userHovered = ImGui::IsItemHovered();
        if (userHovered) {
            ui::FillRounded(drawList, userArea, theme::WithAlpha(colors.textPrimary, 0.04f),
                            tokens.cardRadius);
        }

        // 快捷键提示
        {
            ui::ScopedFontSize hintFont(smallFont);
            ui::TextLeftMiddle(drawList, hintRect, colors.textMuted, hint);
        }

        // 头像（圆形 + 首字）与昵称
        drawList->AddCircleFilled(ImVec2(avatarRect.CenterX(), avatarRect.CenterY()),
                                  avatarDiameter * 0.5f, colors.primary);
        {
            const char* initial =
                (session != nullptr && !session->user.avatarText.empty())
                    ? session->user.avatarText.c_str()
                    : "客";
            ui::ScopedBoldFont avatarFont(std::max(10.0f, tokens.baseFontSize - 1.0f));
            ui::TextCentered(drawList, avatarRect, colors.textInverse, initial);
        }
        {
            ui::ScopedBoldFont nicknameFont(tokens.baseFontSize);
            ui::TextRightMiddle(drawList, layout::Rect{0.0f, shell.topbar.y, nicknameRect.Right(),
                                                       shell.topbar.h},
                                colors.textPrimary, nickname);
        }

        // 面包屑：当前页面名（登录页不画外壳；看板页为「首页」）
        const float breadcrumbLeft = hamburger.Right() + spec::kBreadcrumbLeftGap;
        const float breadcrumbRight = hintRect.x - spec::kBreadcrumbRightGap;
        const layout::Rect breadcrumb{breadcrumbLeft, shell.topbar.y,
                                      std::max(0.0f, breadcrumbRight - breadcrumbLeft),
                                      shell.topbar.h};
        if (!breadcrumb.Empty()) {
            const std::string title = EllipsizeToWidth(PageTitleForRoute(route), breadcrumb.w);
            ui::ScopedFontSize breadcrumbFont(tokens.baseFontSize);
            ui::TextLeftMiddle(drawList, breadcrumb, colors.breadcrumbCurrent, title.c_str());
        }

        // 用户区点击 → 在用户区下方弹出「个人中心」占位提示（Sprint 2 接入）
        if (userPressed) {
            ImGui::OpenPopup("##shell_user_popup");
        }
        // 固定宽度、锚定在用户区右下方（避免默认跟随鼠标时被窗口上缘裁掉）
        const float popupWidth = std::min(shell.topbar.w, spec::kPopupWidth);
        ImGui::SetNextWindowPos(
            ImVec2(std::max(shell.topbar.x, nicknameRect.Right() - popupWidth),
                   shell.topbar.Bottom() + spec::kPopupGap),
            ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(popupWidth, 0.0f),
                                            ImVec2(popupWidth, spec::kPopupMaxSize));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, colors.panel);
        ImGui::PushStyleColor(ImGuiCol_Border, colors.border);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, tokens.cardRadius);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            ImVec2(spec::kPopupPadding, spec::kPopupPadding));
        if (ImGui::BeginPopup("##shell_user_popup")) {
            {
                ui::ScopedBoldFont popupTitleFont(tokens.baseFontSize);
                ImGui::TextUnformatted("个人中心（规划中）");
            }
            ImGui::Separator();
            {
                ui::ScopedFontSize popupFont(std::max(10.0f, tokens.baseFontSize - 1.0f));
                ImGui::TextUnformatted("账号资料 / 退出登录将在 Sprint 2 接入");
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ui::EndFixedWindow();
    }

    // ------------------------------ 标签栏 ------------------------------
    void DrawTagsView(World& world, const layout::ShellLayout& shell,
                      const theme::Palette& colors) {
        if (!ui::BeginFixedWindow("##shell_tags", shell.tagsView)) {
            ui::EndFixedWindow();
            return;
        }
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const layout::LayoutTokens& tokens = shell.tokens;

        ui::Fill(drawList, shell.tagsView, colors.tagsView);
        ui::Line(drawList, shell.tagsView.x, shell.tagsView.Bottom() - 0.5f,
                 shell.tagsView.Right(), shell.tagsView.Bottom() - 0.5f,
                 theme::WithAlpha(colors.tagsViewBorder, 1.0f), 1.0f);

        const float tagHeight =
            std::max(0.0f, shell.tagsView.h - spec::kTagVerticalInset * 2.0f);
        if (tagHeight <= 0.0f) {
            ui::EndFixedWindow();
            return;
        }
        const float smallFont = std::max(10.0f, tokens.baseFontSize - spec::kFontSmallOffset);
        const char* title = "首页";
        float titleWidth = 0.0f;
        {
            ui::ScopedFontSize tagFont(smallFont);
            titleWidth = ui::TextSize(title).x;
        }
        const float tagWidth = spec::kTagLeftPadding * 2.0f + spec::kTagDotSize +
                               spec::kTagDotGap + titleWidth;
        const layout::Rect tag{shell.tagsView.x + spec::kTagFirstLeft,
                               shell.tagsView.y + (shell.tagsView.h - tagHeight) * 0.5f, tagWidth,
                               tagHeight};

        const bool tagPressed = ui::InvisibleButtonAt("##tag_home", tag);
        ui::Fill(drawList, tag, colors.primary);

        // 选中标签：8px 白点 + 白色文字（首页为固定标签，不画关闭按钮）
        drawList->AddCircleFilled(
            ImVec2(tag.x + spec::kTagLeftPadding + spec::kTagDotSize * 0.5f, tag.CenterY()),
            spec::kTagDotSize * 0.5f, colors.textInverse);
        {
            const float textLeft =
                tag.x + spec::kTagLeftPadding + spec::kTagDotSize + spec::kTagDotGap;
            ui::ScopedFontSize tagFont(smallFont);
            ui::TextLeftMiddle(drawList,
                                layout::Rect{textLeft, tag.y,
                                             std::max(0.0f, tag.Right() - textLeft), tag.h},
                                colors.textInverse, title);
        }
        if (tagPressed) {
            EnqueueNavigate(world, "/index");
        }

        ui::EndFixedWindow();
    }
};

}  // namespace

void RegisterShellModule(World& world, SystemScheduler& scheduler) {
    // 外壳私有表现状态（菜单展开/滚动）作为 World 资源注册，测试与多实例友好
    world.EmplaceResource<ShellUiState>();

    // 一级菜单实体（数据来自 MenuCatalog；渲染层只读）
    if (const auto* catalog = world.TryResource<data::MenuCatalog>()) {
        int index = 0;
        for (const data::MenuEntry& entry : catalog->roots) {
            const Entity entity = world.CreateEntity();
            NavItem item;
            item.title = entry.title;
            item.route = entry.route;
            item.icon = entry.icon;
            item.index = index++;
            item.depth = 0;
            item.hasChildren = entry.HasChildren();
            world.Add<NavItem>(entity, item);
        }
    }
    scheduler.Add(std::make_unique<ShellRenderSystem>(), 100);
}

}  // namespace zzyl
