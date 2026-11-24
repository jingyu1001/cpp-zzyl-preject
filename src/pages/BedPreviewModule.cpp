// =============================================================================
// M7 在住管理-床位预览 · cpp-dev1 负责实现（Sprint 4）
//
// 依据：
//  - Sprint4_cpp-dev1_M7床位预览.md（楼层 Tab / 图例 / 房间卡片 / 床位状态 / 只读详情）
//  - Web《Sprint4_功能规格_v0.1.md》§3（页面结构 / 交互 / 布局 1280·1024 / 缺陷 D-41~D-45 修正）
//  - Cpp架构设计_v0.4.md §2.3/§4（IFloorLayoutService、Floor/Room/BedRecord、路由 /liveIn/floor）
//
// 数据流：DataServices.floorLayout 懒加载（Floors → 当前楼层 Rooms → 逐房间 Beds），
//         页面私有状态 BedPreviewPageState 注册为 World 资源；只读，不做写操作。
//
// 布局：全部由 ShellLayout::pageViewport 推导（内容宽 → 列数/卡片宽 → 床位卡宽），
//       不写死屏幕坐标；页面内容区纵向滚动（1280×720 下约 1080 总高）。
// =============================================================================

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

#include "zzyl/app/AppState.h"
#include "zzyl/app/ModuleHooks.h"
#include "zzyl/app/Routes.h"
#include "zzyl/core/System.h"
#include "zzyl/data/BusinessServices.h"
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"
#include "zzyl/ui/Widgets.h"

namespace zzyl {
namespace {

// -----------------------------------------------------------------------------
// 视觉/布局常量（取 Web 规格 §3.1/§3.4 实测值；只作推导参数，不是屏幕坐标）
// -----------------------------------------------------------------------------
namespace spec {
constexpr float kPagePadding = 20.0f;         // 内容块左右留白（Web .bg-wt padding）
constexpr float kPageBottomPadding = 20.0f;   // 内容底部留白
constexpr float kScrollbarReserve = 10.0f;    // 页面纵向滚动条预留宽
constexpr float kFloorTabHeight = 57.0f;      // 楼层 Tab 行高
constexpr float kFloorTabPaddingH = 14.0f;    // 单个 Tab 左右内边距
constexpr float kFloorTabFontSize = 14.0f;    // Tab 字号
constexpr float kFloorIndicatorHeight = 2.0f; // 选中下划线高
constexpr float kFloorIndicatorInset = 8.0f;  // 选中下划线左右内缩
constexpr float kLegendRowHeight = 52.0f;     // 图例行高（含 padding-top 20）
constexpr float kLegendIconSize = 26.0f;      // 图例图标 26×26
constexpr float kLegendItemGap = 30.0f;       // 图例项右距
constexpr float kLegendFontSize = 14.0f;      // 图例文字
constexpr float kGridTopGap = 16.0f;          // 图例行 → 房间卡片
constexpr float kRoomCardGap = 20.0f;         // 房间卡片列间距/行间距
constexpr float kRoomCardPadding = 16.0f;     // 房间卡片内边距
constexpr float kRoomHeaderHeight = 28.0f;    // 房间标题行高
constexpr float kRoomHeaderGap = 10.0f;       // 标题行 → 床位网格
constexpr float kRoomCodeFontSize = 18.0f;    // 房间号 18px
constexpr float kRoomTagHeight = 22.0f;       // 房型标签高
constexpr float kRoomTagPaddingH = 8.0f;      // 房型标签左右内边距
constexpr float kRoomTagFontSize = 12.0f;     // 房型标签字号
constexpr float kRoomIconSize = 20.0f;        // 房间编辑/删除图标 20×20
constexpr float kAddBedButton = 24.0f;        // 房间标题右侧「+」24×24
constexpr float kBedHeight = 72.0f;           // 床位卡高 72
constexpr float kBedGap = 12.0f;              // 床位卡列间距/行间距
constexpr float kBedPadding = 14.0f;          // 床位卡内边距
constexpr float kBedIconSize = 26.0f;         // 床位状态图标 26×26
constexpr float kBedFontSize = 14.0f;         // 床位卡文字
constexpr float kBedHoverButton = 22.0f;      // 悬停圆形按钮 22×22
constexpr float kBedHoverGap = 10.0f;         // 两个圆形按钮间距
constexpr float kAddRoomWidth = 88.0f;        // 新增房间 88×32
constexpr float kAddRoomHeight = 32.0f;
constexpr float kButtonFontSize = 14.0f;
constexpr float kSectionTitleFontSize = 14.0f;
constexpr float kDetailWidth = 380.0f;        // 床位详情卡片
constexpr float kDetailHeight = 268.0f;
constexpr float kDetailFontSize = 14.0f;
constexpr float kDetailLabelFontSize = 13.0f;
constexpr float kMinRoomCardWidth = 300.0f;   // 窄屏时卡片最小宽度（1024 下 2 列）
constexpr float kPageWheelStep = 60.0f;    // 页面滚轮步长（像素/格）
}  // namespace spec

// -----------------------------------------------------------------------------
// 页面私有状态（World 资源；渲染系统只读 + 懒加载缓存）
// -----------------------------------------------------------------------------
struct BedPreviewPageState {
    bool loaded = false;
    bool needReload = true;
    std::vector<data::FloorRecord> floors;    // 11 楼层（一次加载）
    int currentFloorId = 0;                   // 当前楼层
    std::vector<data::RoomRecord> rooms;      // 当前楼层房间
    std::vector<std::vector<data::BedRecord>> roomBeds;  // 与 rooms 对齐的床位缓存

    float pageScroll = 0.0f;      // 页面纵向滚动偏移（自绘滚动条；NoDecoration 窗口无 ImGui 滚动条）
    float pageScrollMax = 0.0f;   // 页面可滚动最大值
    float floorTabScroll = 0.0f;  // 楼层 Tab 横向滚动（D-42 修正：Tab 多时可用）
    int hoverBedId = 0;           // 当前悬停床位（用于悬停遮罩保持）

    // 只读床位详情
    bool detailJustOpened = false;  // 防止"打开详情的同一次点击"立刻被遮罩点击关闭
    bool detailOpen = false;
    int detailFloorId = 0;
    std::string detailFloorName;
    int detailRoomIndex = -1;
    int detailBedIndex = -1;
};

// -----------------------------------------------------------------------------
// 小工具
// -----------------------------------------------------------------------------
data::IFloorLayoutService* FloorService(World& world) {
    auto* services = world.TryResource<DataServices>();
    return services != nullptr ? services->floorLayout.get() : nullptr;
}

const char* BedStatusText(int bedStatus) {
    switch (bedStatus) {
        case 1:
            return "已入住";
        case 2:
            return "请假";
        default:
            return "空闲";
    }
}

ImU32 BedStatusColor(int bedStatus) {
    const theme::Palette& colors = theme::Colors();
    switch (bedStatus) {
        case 1:
            return colors.success;
        case 2:
            return colors.warning;
        default:
            return colors.textSecondary;
    }
}

void PushSecondPhaseToast(World& world, const char* what) {
    if (auto* toasts = world.TryResource<ui::ToastState>()) {
        ui::PushToast(*toasts, std::string("「") + what + "」后续迭代接入",
                      ui::ToastType::Warning);
    }
}

// 当前楼层的房间 + 床位（懒加载；每房间一次 Beds 查询）
void LoadFloor(BedPreviewPageState& state, data::IFloorLayoutService& service, int floorId) {
    state.currentFloorId = floorId;
    state.rooms = service.Rooms(floorId);
    state.roomBeds.assign(state.rooms.size(), std::vector<data::BedRecord>{});
    for (std::size_t index = 0; index < state.rooms.size(); ++index) {
        state.roomBeds[index] = service.Beds(state.rooms[index].id);
    }
    state.floorTabScroll = 0.0f;
    state.hoverBedId = 0;
    state.pageScroll = 0.0f;  // 切楼层回到顶部
}

void EnsureLoaded(BedPreviewPageState& state, data::IFloorLayoutService& service) {
    if (state.floors.empty()) {
        state.floors = service.Floors();
        // Web 楼层 Tab 顺序为 1楼~12楼（floor.code 在 Web 库中顺序混乱，本地按 id 升序还原自然顺序）
        std::sort(state.floors.begin(), state.floors.end(),
                  [](const data::FloorRecord& left, const data::FloorRecord& right) {
                      return left.id < right.id;
                  });
    }
    if (state.floors.empty()) {
        state.rooms.clear();
        state.roomBeds.clear();
        state.currentFloorId = 0;
        state.loaded = true;
        return;
    }
    bool floorExists = false;
    for (const data::FloorRecord& floor : state.floors) {
        if (floor.id == state.currentFloorId) {
            floorExists = true;
            break;
        }
    }
    if (!floorExists) {
        state.currentFloorId = state.floors.front().id;
    }
    LoadFloor(state, service, state.currentFloorId);
    state.loaded = true;
}

std::string FloorNameOf(const BedPreviewPageState& state, int floorId) {
    for (const data::FloorRecord& floor : state.floors) {
        if (floor.id == floorId) {
            return floor.name;
        }
    }
    return std::string();
}

// -----------------------------------------------------------------------------
// 图标绘制（几何占位，与外壳/组件图标风格一致；真实 SVG 资源后续迭代接入）
// -----------------------------------------------------------------------------
void DrawBedGlyph(ImDrawList* drawList, const layout::Rect& rect, ImU32 color) {
    // 床侧视剪影：床头板 + 床垫 + 枕头 + 两条床腿（几何占位，26×26 内绘制）
    const float w = rect.w;
    const float h = rect.h;
    // 床头板（左竖板）
    ui::FillRounded(drawList,
                    layout::Rect{rect.x + w * 0.12f, rect.y + h * 0.34f, w * 0.10f, h * 0.38f},
                    color, h * 0.04f);
    // 床垫
    ui::FillRounded(drawList,
                    layout::Rect{rect.x + w * 0.12f, rect.y + h * 0.56f, w * 0.76f, h * 0.16f},
                    color, h * 0.04f);
    // 枕头
    ui::FillRounded(drawList,
                    layout::Rect{rect.x + w * 0.26f, rect.y + h * 0.44f, w * 0.24f, h * 0.10f},
                    color, h * 0.04f);
    // 床腿
    ui::Line(drawList, rect.x + w * 0.20f, rect.y + h * 0.72f, rect.x + w * 0.20f,
             rect.y + h * 0.86f, color, 1.4f);
    ui::Line(drawList, rect.x + w * 0.80f, rect.y + h * 0.72f, rect.x + w * 0.80f,
             rect.y + h * 0.86f, color, 1.4f);
}

void DrawPencilIcon(ImDrawList* drawList, const layout::Rect& rect, ImU32 color) {
    const float w = rect.w;
    const float h = rect.h;
    ui::Line(drawList, rect.x + w * 0.22f, rect.y + h * 0.78f, rect.x + w * 0.74f,
             rect.y + h * 0.26f, color, 1.5f);
    ui::Line(drawList, rect.x + w * 0.30f, rect.y + h * 0.70f, rect.x + w * 0.78f,
             rect.y + h * 0.22f, color, 1.5f);
    ui::Line(drawList, rect.x + w * 0.22f, rect.y + h * 0.78f, rect.x + w * 0.18f,
             rect.y + h * 0.86f, color, 1.5f);
}

void DrawTrashIcon(ImDrawList* drawList, const layout::Rect& rect, ImU32 color) {
    const float w = rect.w;
    const float h = rect.h;
    ui::Line(drawList, rect.x + w * 0.18f, rect.y + h * 0.26f, rect.x + w * 0.82f,
             rect.y + h * 0.26f, color, 1.4f);
    ui::Line(drawList, rect.x + w * 0.40f, rect.y + h * 0.16f, rect.x + w * 0.60f,
             rect.y + h * 0.16f, color, 1.4f);
    ui::Line(drawList, rect.x + w * 0.30f, rect.y + h * 0.32f, rect.x + w * 0.34f,
             rect.y + h * 0.82f, color, 1.2f);
    ui::Line(drawList, rect.x + w * 0.70f, rect.y + h * 0.32f, rect.x + w * 0.66f,
             rect.y + h * 0.82f, color, 1.2f);
    ui::Line(drawList, rect.x + w * 0.34f, rect.y + h * 0.82f, rect.x + w * 0.66f,
             rect.y + h * 0.82f, color, 1.2f);
}

void DrawPlusIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness) {
    const float half = size * 0.5f;
    ui::Line(drawList, center.x - half, center.y, center.x + half, center.y, color, thickness);
    ui::Line(drawList, center.x, center.y - half, center.x, center.y + half, color, thickness);
}

void DrawPlusCircleIcon(ImDrawList* drawList, const layout::Rect& rect, ImU32 color) {
    drawList->AddCircle(ImVec2(rect.CenterX(), rect.CenterY()), rect.w * 0.5f - 1.0f, color, 20,
                        1.4f);
    DrawPlusIcon(drawList, ImVec2(rect.CenterX(), rect.CenterY()), rect.w * 0.48f, color, 1.4f);
}

// -----------------------------------------------------------------------------
// 房间卡片布局
// -----------------------------------------------------------------------------
struct RoomCardLayout {
    layout::Rect rect;
    std::vector<layout::Rect> beds;
};

float RoomCardHeight(std::size_t bedCount) {
    const int columns = 2;
    const int rows = std::max(1, static_cast<int>((bedCount + columns - 1) / columns));
    return spec::kRoomCardPadding * 2.0f + spec::kRoomHeaderHeight + spec::kRoomHeaderGap +
           static_cast<float>(rows) * spec::kBedHeight +
           static_cast<float>(std::max(0, rows - 1)) * spec::kBedGap;
}

// 计算当前楼层所有房间卡片 / 床位卡矩形（按内容宽自适应列数：1280/1024 两列，宽屏三列）
std::vector<RoomCardLayout> BuildRoomLayout(const BedPreviewPageState& state, float contentWidth,
                                            float topY) {
    std::vector<RoomCardLayout> layouts;
    const std::size_t roomCount = state.rooms.size();
    if (roomCount == 0) {
        return layouts;
    }
    int columns = contentWidth >= 1300.0f ? 3 : 2;  // 1280/1024 两列；宽屏（≥1600 窗口）三列（修 D-43）
    columns = std::clamp(columns, 2, 3);
    if (static_cast<std::size_t>(columns) > roomCount) {
        columns = static_cast<int>(roomCount);
    }
    const float cardWidth =
        (contentWidth - static_cast<float>(columns - 1) * spec::kRoomCardGap) /
        static_cast<float>(columns);

    // 先按行分组计算行高，再逐行摆放
    std::vector<float> rowHeights;
    for (std::size_t index = 0; index < roomCount; index += static_cast<std::size_t>(columns)) {
        float rowHeight = 0.0f;
        for (std::size_t column = 0; column < static_cast<std::size_t>(columns); ++column) {
            const std::size_t roomIndex = index + column;
            if (roomIndex >= roomCount) {
                break;
            }
            rowHeight = std::max(rowHeight, RoomCardHeight(state.roomBeds[roomIndex].size()));
        }
        rowHeights.push_back(rowHeight);
    }

    float y = topY;
    std::size_t roomIndex = 0;
    for (std::size_t row = 0; row < rowHeights.size(); ++row) {
        for (int column = 0; column < columns && roomIndex < roomCount; ++column, ++roomIndex) {
            const float x = static_cast<float>(column) * (cardWidth + spec::kRoomCardGap);
            const float height = RoomCardHeight(state.roomBeds[roomIndex].size());
            RoomCardLayout layout;
            layout.rect = layout::Rect{x, y, cardWidth, height};

            const std::size_t bedCount = state.roomBeds[roomIndex].size();
            const int bedRows =
                std::max(1, static_cast<int>((bedCount + 1) / 2));
            const float bedWidth =
                (cardWidth - spec::kRoomCardPadding * 2.0f - spec::kBedGap) * 0.5f;
            const float bedsTop = y + spec::kRoomCardPadding + spec::kRoomHeaderHeight +
                                  spec::kRoomHeaderGap;
            for (std::size_t bedIndex = 0; bedIndex < bedCount; ++bedIndex) {
                const int bedRow = static_cast<int>(bedIndex) / 2;
                const int bedColumn = static_cast<int>(bedIndex) % 2;
                const float bedX = layout.rect.x + spec::kRoomCardPadding +
                                   static_cast<float>(bedColumn) * (bedWidth + spec::kBedGap);
                const float bedY = bedsTop + static_cast<float>(bedRow) *
                                                 (spec::kBedHeight + spec::kBedGap);
                layout.beds.push_back(layout::Rect{bedX, bedY, bedWidth, spec::kBedHeight});
            }
            (void)bedRows;
            layouts.push_back(std::move(layout));
        }
        y += rowHeights[row] + spec::kRoomCardGap;
    }
    return layouts;
}

// -----------------------------------------------------------------------------
// M7 床位预览页渲染系统
// -----------------------------------------------------------------------------
class BedPreviewPageSystem final : public ISystem {
public:
    const char* Name() const override { return "BedPreviewPageSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& /*frame*/) override {
        const RouteState* route = world.TryResource<RouteState>();
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        if (route == nullptr || shell == nullptr || route->current != Route::BedPreview) {
            m_visible = false;
            return;
        }
        BedPreviewPageState* state = world.TryResource<BedPreviewPageState>();
        data::IFloorLayoutService* service = FloorService(world);
        if (state == nullptr || service == nullptr) {
            m_visible = false;
            return;
        }
        if (!m_visible) {
            m_visible = true;
            state->needReload = true;  // 每次进入页面重新加载当前楼层（床位状态可能被入住业务修改）
        }
        if (state->needReload) {
            EnsureLoaded(*state, *service);
            state->needReload = false;
        }
        const layout::Rect pageViewport = shell->pageViewport;
        if (pageViewport.Empty()) {
            return;
        }
        DrawPage(world, pageViewport, *state, *service);
    }

private:
    bool m_visible = false;

    // ------------------------------ 页面主体 ------------------------------
    void DrawPage(World& world, const layout::Rect& pageViewport, BedPreviewPageState& state,
                  data::IFloorLayoutService& service) {
        const theme::Palette& colors = theme::Colors();
        ImDrawList* background = ImGui::GetBackgroundDrawList();
        ui::Fill(background, pageViewport, colors.background);

        ImGui::SetNextWindowPos(ui::Min(pageViewport));
        ImGui::SetNextWindowSize(ImVec2(pageViewport.w, pageViewport.h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;
        const bool open = ImGui::Begin("##bed_preview_page", nullptr, flags);
        if (open) {
            const float contentWidth = std::max(
                1.0f, pageViewport.w - spec::kPagePadding - spec::kScrollbarReserve);
            const bool modalOpen = state.detailOpen;
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // ---- 先算内容总高（用于页面滚动范围），再处理滚轮 ----
            const float legendBottom = spec::kFloorTabHeight + spec::kLegendRowHeight;
            const float gridTop = legendBottom + spec::kGridTopGap;
            std::vector<RoomCardLayout> layouts;
            float contentHeight = legendBottom;
            if (state.rooms.empty()) {
                contentHeight = legendBottom + 200.0f + spec::kPageBottomPadding;
            } else {
                layouts = BuildRoomLayout(state, contentWidth, gridTop);
                float gridBottom = gridTop;
                for (const RoomCardLayout& item : layouts) {
                    gridBottom = std::max(gridBottom, item.rect.Bottom());
                }
                contentHeight = gridBottom + spec::kPageBottomPadding;
            }
            state.pageScrollMax = std::max(0.0f, contentHeight - pageViewport.h);

            // 手动纵向滚动：NoDecoration 窗口无 ImGui 滚动条，页面自绘细滚动条 + 滚轮
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool pageHovered = mouse.x >= pageViewport.x && mouse.x <= pageViewport.Right() &&
                                     mouse.y >= pageViewport.y && mouse.y <= pageViewport.Bottom();
            if (!modalOpen && pageHovered && state.pageScrollMax > 0.0f) {
                const float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0.0f) {
                    state.pageScroll -= wheel * spec::kPageWheelStep;
                }
            }
            state.pageScroll = std::clamp(state.pageScroll, 0.0f, state.pageScrollMax);

            const auto localToScreen = [&pageViewport, &state](const layout::Rect& local) {
                return layout::Rect{pageViewport.x + spec::kPagePadding + local.x,
                                    pageViewport.y + local.y - state.pageScroll, local.w,
                                    local.h};
            };

            // ---- ① 楼层 Tab 行（高 57，底部 1px 分隔线；选中主色 + 2px 下划线）----
            DrawFloorTabs(world, drawList, state, service, localToScreen,
                          layout::Rect{0.0f, 0.0f, contentWidth, spec::kFloorTabHeight},
                          !modalOpen);

            // ---- ② 图例 + 新增房间（高 52）----
            DrawLegend(world, drawList, colors,
                       localToScreen(layout::Rect{0.0f, spec::kFloorTabHeight, contentWidth,
                                                  spec::kLegendRowHeight}),
                       !modalOpen);

            // ---- ③ 房间卡片网格 ----
            if (state.rooms.empty()) {
                ui::TextCentered(drawList,
                                 localToScreen(layout::Rect{0.0f, legendBottom, contentWidth,
                                                            200.0f}),
                                 colors.textSecondary, "暂无数据");
            } else {
                for (std::size_t index = 0; index < layouts.size(); ++index) {
                    DrawRoomCard(world, drawList, state, service, colors, index,
                                 localToScreen(layouts[index].rect), localToScreen,
                                 layouts[index], !modalOpen);
                }
            }

            // ---- ④ 页面细滚动条（6px，贴内容区右侧；Web 一致）----
            if (state.pageScrollMax > 0.0f) {
                const float barWidth = 6.0f;
                const layout::Rect track{pageViewport.Right() - barWidth - 2.0f,
                                         pageViewport.y + 2.0f, barWidth,
                                         pageViewport.h - 4.0f};
                ui::FillRounded(drawList, track, colors.borderLight, barWidth * 0.5f);
                const float thumbHeight =
                    std::max(30.0f, track.h * track.h / (track.h + state.pageScrollMax));
                const float travel = std::max(0.0f, track.h - thumbHeight);
                const float ratio = state.pageScrollMax > 0.0f
                                        ? state.pageScroll / state.pageScrollMax
                                        : 0.0f;
                const layout::Rect thumb{track.x, track.y + travel * ratio, barWidth,
                                         thumbHeight};
                ui::FillRounded(drawList, thumb, theme::WithAlpha(colors.textPlaceholder, 0.95f),
                                barWidth * 0.5f);
            }

            // ---- ⑤ 只读床位详情（覆盖在页面上的模态卡片）----
            if (state.detailOpen) {
                DrawBedDetail(world, drawList, state, pageViewport);
                state.detailJustOpened = false;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    // 通用小按钮：主文字按钮（新增房间 88×32）
    bool DrawPrimarySmallButton(const char* id, ImDrawList* drawList, const layout::Rect& rect,
                                const char* label) const {
        if (rect.Empty()) {
            return false;
        }
        const bool pressed = ui::InvisibleButtonAt(id, rect);
        const bool hovered = ImGui::IsItemHovered();
        const theme::Palette& colors = theme::Colors();
        ui::FillRounded(drawList, rect, hovered ? colors.primaryHover : colors.primary, 4.0f);
        ui::ScopedFontSize font(spec::kButtonFontSize);
        ui::TextCentered(drawList, rect, colors.textInverse, label);
        return pressed;
    }

    // ① 楼层 Tab 行：左侧 11 个楼层（支持横向滚动），右侧「新增楼层」
    void DrawFloorTabs(World& world, ImDrawList* drawList, BedPreviewPageState& state,
                       data::IFloorLayoutService& service,
                       const std::function<layout::Rect(const layout::Rect&)>& toScreen,
                       const layout::Rect& local, bool interactive) {
        const theme::Palette& colors = theme::Colors();
        const layout::Rect screenRect = toScreen(local);
        ui::Line(drawList, screenRect.x, screenRect.Bottom() - 0.5f, screenRect.Right(),
                 screenRect.Bottom() - 0.5f, colors.border, 1.0f);

        ui::ScopedFontSize tabFont(spec::kFloorTabFontSize);
        // 「新增楼层」文字按钮（右对齐）
        const float addWidth = std::max(64.0f, ui::TextSize("新增楼层").x + 12.0f);
        const layout::Rect addLocal{local.w - addWidth, 0.0f, addWidth, spec::kFloorTabHeight};
        const layout::Rect addScreen = toScreen(addLocal);
        const bool addHovered = ImGui::IsMouseHoveringRect(ui::Min(addScreen), ui::Max(addScreen));
        {
            ui::ScopedBoldFont boldFont(spec::kFloorTabFontSize);
            ui::TextRightMiddle(drawList, addScreen, colors.link, "新增楼层");
        }
        if (addHovered && interactive) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                PushSecondPhaseToast(world, "新增楼层");
            }
        }

        // 楼层 Tab 布局：先算总宽，再按需横向滚动/裁剪
        const float tabsMaxWidth = local.w - addWidth - 16.0f;
        float totalWidth = 0.0f;
        std::vector<float> tabWidths;
        for (const data::FloorRecord& floor : state.floors) {
            const float width = ui::TextSize(floor.name.c_str()).x + spec::kFloorTabPaddingH * 2.0f;
            tabWidths.push_back(width);
            totalWidth += width;
        }
        const float maxScroll = std::max(0.0f, totalWidth - tabsMaxWidth);
        const layout::Rect tabsClip = toScreen(layout::Rect{0.0f, 0.0f, tabsMaxWidth, local.h});
        // 滚轮横向滚动（D-42 修正）
        if (interactive && maxScroll > 0.0f && ImGui::IsMouseHoveringRect(
                                   ImVec2(tabsClip.x, tabsClip.y),
                                   ImVec2(tabsClip.Right(), tabsClip.Bottom()))) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                state.floorTabScroll -= wheel * 60.0f;
            }
        }
        state.floorTabScroll = std::clamp(state.floorTabScroll, 0.0f, maxScroll);

        drawList->PushClipRect(ImVec2(tabsClip.x, tabsClip.y),
                               ImVec2(tabsClip.Right(), tabsClip.Bottom()), true);
        float tabX = -state.floorTabScroll;
        for (std::size_t index = 0; index < state.floors.size(); ++index) {
            const data::FloorRecord& floor = state.floors[index];
            const layout::Rect tabLocal{tabX, 0.0f, tabWidths[index], spec::kFloorTabHeight};
            const layout::Rect tabScreen = toScreen(tabLocal);
            const bool selected = floor.id == state.currentFloorId;
            const bool hovered =
                ImGui::IsMouseHoveringRect(ui::Min(tabScreen), ui::Max(tabScreen));
            {
                ui::ScopedBoldFont boldFont(spec::kFloorTabFontSize);
                ui::TextCentered(drawList, tabScreen,
                                 selected ? colors.link
                                          : (hovered ? colors.primary : colors.textRegular),
                                 floor.name.c_str());
            }
            if (selected) {
                const layout::Rect indicator{
                    tabScreen.x + spec::kFloorIndicatorInset,
                    tabScreen.Bottom() - spec::kFloorIndicatorHeight,
                    std::max(0.0f, tabScreen.w - spec::kFloorIndicatorInset * 2.0f),
                    spec::kFloorIndicatorHeight};
                ui::FillRounded(drawList, indicator, colors.link,
                                spec::kFloorIndicatorHeight * 0.5f);
            }
            if (interactive && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !selected) {
                LoadFloor(state, service, floor.id);
                drawList->PopClipRect();
                return;
            }
            tabX += tabWidths[index];
        }
        drawList->PopClipRect();
    }

    // ② 图例（空闲中/已入住/请假中）+ 新增房间
    void DrawLegend(World& world, ImDrawList* drawList, const theme::Palette& colors,
                    const layout::Rect& screenRect, bool interactive) {
        float x = screenRect.x;
        const float iconTop = screenRect.y + (spec::kLegendRowHeight - 20.0f) * 0.5f -
                              spec::kLegendIconSize * 0.5f + 10.0f;
        const struct LegendItem {
            const char* label;
            int status;
        } items[3] = {{"空闲中", 0}, {"已入住", 1}, {"请假中", 2}};
        for (const LegendItem& item : items) {
            const layout::Rect icon{x, iconTop, spec::kLegendIconSize, spec::kLegendIconSize};
            DrawBedGlyph(drawList, icon, BedStatusColor(item.status));
            {
                ui::ScopedFontSize font(spec::kLegendFontSize);
                const float textWidth = ui::TextSize(item.label).x;
                ui::TextLeftMiddle(
                    drawList,
                    layout::Rect{icon.Right() + 8.0f, screenRect.y + 10.0f, textWidth,
                                 spec::kLegendRowHeight - 10.0f},
                    colors.textTitle, item.label);
                x = icon.Right() + 8.0f + textWidth + spec::kLegendItemGap;
            }
        }
        // 右侧「新增房间」88×32
        const layout::Rect addRect{screenRect.Right() - spec::kAddRoomWidth,
                                   screenRect.y + 10.0f, spec::kAddRoomWidth,
                                   spec::kAddRoomHeight};
        if (interactive && DrawPrimarySmallButton("##bedpreview_add_room", drawList, addRect,
                                                  "新增房间")) {
            PushSecondPhaseToast(world, "新增房间");
        }
    }

    // ③ 房间卡片：房间号 + 房型标签 + 编辑/删除 + 新增床位 + 2 列床位网格
    void DrawRoomCard(World& world, ImDrawList* drawList, BedPreviewPageState& state,
                      data::IFloorLayoutService& /*service*/, const theme::Palette& colors,
                      std::size_t roomIndex, const layout::Rect& cardScreen,
                      const std::function<layout::Rect(const layout::Rect&)>& toScreen,
                      const RoomCardLayout& layout, bool interactive) {
        const data::RoomRecord& room = state.rooms[roomIndex];
        ui::FillRounded(drawList, cardScreen, colors.panel, 2.0f);
        ui::StrokeRounded(drawList, cardScreen, colors.border, 2.0f, 1.0f);

        // ---- 标题行：房间号 18px + 房型标签 + 编辑/删除图标 + 右侧「+」----
        const float headerY = cardScreen.y + spec::kRoomCardPadding;
        const layout::Rect headerRect{cardScreen.x + spec::kRoomCardPadding, headerY,
                                      cardScreen.w - spec::kRoomCardPadding * 2.0f,
                                      spec::kRoomHeaderHeight};
        {
            ui::ScopedBoldFont codeFont(spec::kRoomCodeFontSize);
            const float codeWidth = ui::TextSize(room.code.c_str()).x;
            ui::TextLeftMiddle(drawList,
                               layout::Rect{headerRect.x, headerRect.y, codeWidth,
                                            spec::kRoomHeaderHeight},
                               colors.textTitle, room.code.c_str());

            float cursorX = headerRect.x + codeWidth + 12.0f;
            if (!room.typeName.empty()) {
                ui::ScopedFontSize tagFont(spec::kRoomTagFontSize);
                const float tagWidth =
                    std::min(headerRect.w - codeWidth - 120.0f,
                             ui::TextSize(room.typeName.c_str()).x + spec::kRoomTagPaddingH * 2.0f);
                if (tagWidth > 0.0f) {
                    const layout::Rect tag{cursorX,
                                           headerRect.y + (spec::kRoomHeaderHeight -
                                                           spec::kRoomTagHeight) * 0.5f,
                                           tagWidth, spec::kRoomTagHeight};
                    ui::FillRounded(drawList, tag, IM_COL32(0xE5, 0xF7, 0xF5, 0xFF), 2.0f);
                    ui::TextCentered(drawList, tag, colors.primary, room.typeName.c_str());
                    cursorX = tag.Right() + 12.0f;
                }
            }
            // 编辑/删除（房间级，常显；D-45 修正）
            const float iconY = headerRect.y + (spec::kRoomHeaderHeight - spec::kRoomIconSize) * 0.5f;
            const layout::Rect editRect{cursorX, iconY, spec::kRoomIconSize, spec::kRoomIconSize};
            const layout::Rect deleteRect{editRect.Right() + 8.0f, iconY, spec::kRoomIconSize,
                                          spec::kRoomIconSize};
            DrawPencilIcon(drawList, editRect, colors.iconRegular);
            DrawTrashIcon(drawList, deleteRect, colors.iconRegular);
            if (interactive) {
                if (ImGui::IsMouseHoveringRect(ui::Min(editRect), ui::Max(editRect)) &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    PushSecondPhaseToast(world, "编辑房间");
                }
                if (ImGui::IsMouseHoveringRect(ui::Min(deleteRect), ui::Max(deleteRect)) &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    PushSecondPhaseToast(world, "删除房间");
                }
            }
            // 右侧「+」（新增床位）
            const layout::Rect addBedRect{headerRect.Right() - spec::kAddBedButton,
                                          headerRect.y + (spec::kRoomHeaderHeight -
                                                          spec::kAddBedButton) * 0.5f,
                                          spec::kAddBedButton, spec::kAddBedButton};
            DrawPlusCircleIcon(drawList, addBedRect, colors.primary);
            if (interactive && ImGui::IsMouseHoveringRect(ui::Min(addBedRect), ui::Max(addBedRect)) &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                PushSecondPhaseToast(world, "新增床位");
            }
        }

        // ---- 床位网格 ----
        if (layout.beds.empty()) {
            const layout::Rect emptyRect{cardScreen.x + spec::kRoomCardPadding,
                                         headerRect.Bottom() + spec::kRoomHeaderGap,
                                         cardScreen.w - spec::kRoomCardPadding * 2.0f,
                                         spec::kBedHeight};
            ui::ScopedFontSize font(spec::kBedFontSize);
            ui::TextCentered(drawList, emptyRect, colors.textSecondary, "当前房间没有安排床位");
            return;
        }
        for (std::size_t bedIndex = 0; bedIndex < layout.beds.size(); ++bedIndex) {
            const layout::Rect bedRect = toScreen(layout.beds[bedIndex]);
            DrawBedCard(world, drawList, state, colors, room, state.roomBeds[roomIndex][bedIndex],
                        bedRect, interactive);
        }
    }

    // 单个床位卡：灰底 + 状态图标 + 两行文字；悬停显示遮罩 + 编辑/删除圆钮；点击开详情
    void DrawBedCard(World& world, ImDrawList* drawList, BedPreviewPageState& state,
                     const theme::Palette& colors, const data::RoomRecord& room,
                     const data::BedRecord& bed, layout::Rect rect, bool interactive) {
        (void)room;
        // 由 DrawRoomCard 传入的 rect 已由调用方换算为屏幕坐标
        const layout::Rect& bedRect = rect;
        if (bedRect.Empty()) {
            return;
        }
        const bool occupied = bed.bedStatus == 1;
        bool cardHovered = false;
        if (interactive) {
            ui::InvisibleButtonAt(("##bed_" + std::to_string(bed.id)).c_str(), bedRect);
            cardHovered = ImGui::IsItemHovered();
            if (cardHovered) {
                state.hoverBedId = bed.id;
            }
        }
        ui::FillRounded(drawList, bedRect, theme::Colors().panelHover, 2.0f);

        // 状态图标（26×26）+ 两行文字
        const layout::Rect iconRect{bedRect.x + spec::kBedPadding,
                                    bedRect.y + (bedRect.h - spec::kBedIconSize) * 0.5f,
                                    spec::kBedIconSize, spec::kBedIconSize};
        DrawBedGlyph(drawList, iconRect, BedStatusColor(bed.bedStatus));
        {
            ui::ScopedFontSize font(spec::kBedFontSize);
            const float textLeft = iconRect.Right() + 12.0f;
            const float textWidth = std::max(0.0f, bedRect.Right() - spec::kBedPadding - textLeft);
            const std::string line1 = "床位号：" + bed.bedNumber;
            const std::string line2 =
                occupied ? (bed.elderName.empty() ? std::string("已入住") : bed.elderName)
                         : std::string(BedStatusText(bed.bedStatus));
            ui::TextLeftMiddle(drawList,
                               layout::Rect{textLeft, bedRect.y + 8.0f, textWidth, 24.0f},
                               theme::Colors().textRegular, line1.c_str());
            ui::TextLeftMiddle(drawList,
                               layout::Rect{textLeft, bedRect.y + 34.0f, textWidth, 24.0f},
                               occupied && !bed.elderName.empty()
                                   ? theme::Colors().textTitle
                                   : theme::Colors().textSecondary,
                               line2.c_str());
        }

        // 悬停遮罩 + 编辑/删除圆形按钮（已入住床位禁用，透明度 0.4）
        const bool showHover = interactive && (cardHovered || state.hoverBedId == bed.id);
        if (!showHover) {
            if (state.hoverBedId == bed.id) {
                state.hoverBedId = 0;
            }
            if (interactive && cardHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                OpenBedDetail(state, room, bed);
            }
            return;
        }
        drawList->AddRectFilled(ui::Min(bedRect), ui::Max(bedRect),
                                IM_COL32(0, 0, 0, 76), 2.0f);
        const float buttonSize = spec::kBedHoverButton;
        const float groupWidth = buttonSize * 2.0f + spec::kBedHoverGap;
        const float startX = bedRect.CenterX() - groupWidth * 0.5f;
        const layout::Rect editRect{startX, bedRect.CenterY() - buttonSize * 0.5f, buttonSize,
                                    buttonSize};
        const layout::Rect deleteRect{editRect.Right() + spec::kBedHoverGap,
                                      bedRect.CenterY() - buttonSize * 0.5f, buttonSize,
                                      buttonSize};
        const ImU32 buttonAlpha = occupied ? 102U : 255U;
        const ImU32 buttonBg = IM_COL32(255, 255, 255, buttonAlpha);
        drawList->AddCircleFilled(ImVec2(editRect.CenterX(), editRect.CenterY()),
                                  buttonSize * 0.5f, buttonBg);
        drawList->AddCircleFilled(ImVec2(deleteRect.CenterX(), deleteRect.CenterY()),
                                  buttonSize * 0.5f, buttonBg);
        const ImU32 editColor = occupied ? IM_COL32(0x90, 0x93, 0x99, 102) : theme::Colors().link;
        const ImU32 deleteColor =
            occupied ? IM_COL32(0x90, 0x93, 0x99, 102) : theme::Colors().danger;
        DrawPencilIcon(drawList, editRect, editColor);
        DrawTrashIcon(drawList, deleteRect, deleteColor);

        const bool clicked = interactive && cardHovered &&
                             ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool overEdit = editRect.Contains(mouse.x, mouse.y);
        const bool overDelete = deleteRect.Contains(mouse.x, mouse.y);
        if (!interactive) {
            return;
        }
        if (occupied) {
            // 已入住床位不可编辑/删除，点击卡片仍可查看只读详情
            if ((overEdit || overDelete) && ImGui::IsMouseHoveringRect(ui::Min(editRect),
                                                                      ui::Max(deleteRect))) {
                ImGui::SetTooltip("已入住床位不可编辑/删除");
            }
            if (clicked) {
                OpenBedDetail(state, room, bed);
            }
            return;
        }
        if (clicked && overEdit) {
            PushSecondPhaseToast(world, "编辑床位");
        } else if (clicked && overDelete) {
            PushSecondPhaseToast(world, "删除床位");
        } else if (clicked) {
            OpenBedDetail(state, room, bed);
        }
    }

    void OpenBedDetail(BedPreviewPageState& state, const data::RoomRecord& room,
                       const data::BedRecord& bed) {
        state.detailOpen = true;
        state.detailJustOpened = true;
        state.detailFloorId = state.currentFloorId;
        state.detailFloorName = FloorNameOf(state, state.currentFloorId);
        state.detailRoomIndex = -1;
        state.detailBedIndex = -1;
        for (std::size_t roomIndex = 0; roomIndex < state.rooms.size(); ++roomIndex) {
            if (state.rooms[roomIndex].id != room.id) {
                continue;
            }
            state.detailRoomIndex = static_cast<int>(roomIndex);
            for (std::size_t bedIndex = 0; bedIndex < state.roomBeds[roomIndex].size(); ++bedIndex) {
                if (state.roomBeds[roomIndex][bedIndex].id == bed.id) {
                    state.detailBedIndex = static_cast<int>(bedIndex);
                    break;
                }
            }
            break;
        }
    }

    // 只读床位详情（页面内模态卡片，不写库）
    void DrawBedDetail(World& world, ImDrawList* drawList, BedPreviewPageState& state,
                       const layout::Rect& pageViewport) {
        const theme::Palette& colors = theme::Colors();
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        // 半透明遮罩
        ui::Fill(drawList, pageViewport, IM_COL32(0, 0, 0, 76));

        const float cardWidth = std::min(spec::kDetailWidth, pageViewport.w - 40.0f);
        const float cardHeight = std::min(spec::kDetailHeight, pageViewport.h - 40.0f);
        const layout::Rect card{pageViewport.CenterX() - cardWidth * 0.5f,
                                pageViewport.CenterY() - cardHeight * 0.5f, cardWidth,
                                cardHeight};
        ui::FillRounded(drawList, card, colors.panel, 8.0f);
        ui::StrokeRounded(drawList, card, colors.border, 8.0f, 1.0f);

        // 标题
        {
            ui::ScopedBoldFont titleFont(16.0f);
            ui::TextLeftMiddle(drawList,
                               layout::Rect{card.x + 20.0f, card.y + 12.0f, card.w - 60.0f, 28.0f},
                               colors.textTitle, "床位详情");
        }
        // 关闭 ×
        const ImVec2 closeCenter(card.Right() - 22.0f, card.y + 26.0f);
        drawList->AddLine(ImVec2(closeCenter.x - 6.0f, closeCenter.y - 6.0f),
                          ImVec2(closeCenter.x + 6.0f, closeCenter.y + 6.0f),
                          colors.textSecondary, 1.6f);
        drawList->AddLine(ImVec2(closeCenter.x - 6.0f, closeCenter.y + 6.0f),
                          ImVec2(closeCenter.x + 6.0f, closeCenter.y - 6.0f),
                          colors.textSecondary, 1.6f);
        const layout::Rect closeRect{closeCenter.x - 12.0f, closeCenter.y - 12.0f, 24.0f, 24.0f};

        // 内容行
        const data::BedRecord* bed = nullptr;
        const data::RoomRecord* room = nullptr;
        if (state.detailRoomIndex >= 0 &&
            state.detailRoomIndex < static_cast<int>(state.rooms.size())) {
            room = &state.rooms[static_cast<std::size_t>(state.detailRoomIndex)];
            const auto& beds = state.roomBeds[static_cast<std::size_t>(state.detailRoomIndex)];
            if (state.detailBedIndex >= 0 &&
                state.detailBedIndex < static_cast<int>(beds.size())) {
                bed = &beds[static_cast<std::size_t>(state.detailBedIndex)];
            }
        }
        if (bed != nullptr) {
            const std::pair<const char*, std::string> rows[] = {
                {"床位号", bed->bedNumber},
                {"楼层", state.detailFloorName},
                {"房间号", room != nullptr ? room->code : std::string()},
                {"房型", room != nullptr ? room->typeName : std::string()},
                {"状态", BedStatusText(bed->bedStatus)},
                {"老人姓名", bed->elderName.empty() ? std::string("—") : bed->elderName},
            };
            float rowY = card.y + 56.0f;
            for (const auto& row : rows) {
                {
                    ui::ScopedFontSize labelFont(spec::kDetailLabelFontSize);
                    ui::TextLeftMiddle(drawList,
                                       layout::Rect{card.x + 24.0f, rowY, 80.0f, 26.0f},
                                       colors.textSecondary, row.first);
                }
                {
                    ui::ScopedFontSize valueFont(spec::kDetailFontSize);
                    ui::TextLeftMiddle(drawList,
                                       layout::Rect{card.x + 108.0f, rowY,
                                                    card.w - 132.0f, 26.0f},
                                       colors.textRegular, row.second.c_str());
                }
                rowY += 30.0f;
            }
        }

        // 点击遮罩空白处 / × 关闭
        if (!state.detailJustOpened && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const bool inClose = closeRect.Contains(mouse.x, mouse.y);
            const bool inCard = card.Contains(mouse.x, mouse.y);
            if (inClose || !inCard) {
                state.detailOpen = false;
                state.hoverBedId = 0;
            }
        }
        (void)world;
    }
};

}  // namespace

void RegisterBedPreviewModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<BedPreviewPageState>();
    scheduler.Add(std::make_unique<BedPreviewPageSystem>(), 200);
}

}  // namespace zzyl
