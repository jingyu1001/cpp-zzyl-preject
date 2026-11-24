// =============================================================================
// M4 入退管理-入住办理（列表）· cpp-dev1 负责实现
//
// 依据：
//  - Sprint3_cpp-dev1_M4入住办理列表与外壳.md（页面结构 / 9 列 / 交互）
//  - Web《Sprint3_M4入住办理规格_v0.1.md》§1（列表页实测：搜索 2 项 / 9 列 / 状态标签 / 分页）
//  - Cpp架构设计_v0.3.md §3.1/§3.2（路由、ICheckInService、CheckInSelection、SQLite 数据层）
//
// 数据流（遵守渲染与数据分离）：
//  - 页面私有状态 CheckInListPageState 注册为 World 资源；
//  - 列表数据来自 DataServices.checkIn（ICheckInService，SQLite 实现），不 include 数据层实现头；
//  - 「查看」写 CheckInSelection.id 后入队 Navigate；「发起入住申请」直接入队 Navigate；
//  - 渲染只读页面状态，不在渲染期写库。
//
// 布局（禁止写死屏幕坐标）：
//  所有矩形由 ShellLayout::pageViewport 逐层推导：搜索栏 → 工具栏 → 表格（余高）→ 分页（贴底），
//  高度取自组件辅助函数（SearchBarHeight/ToolbarHeight/PaginationHeight）与本页规格常量。
// =============================================================================

#include <algorithm>
#include <cstdio>
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
#include "zzyl/render/Texture.h"
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"
#include "zzyl/ui/Widgets.h"

namespace zzyl {
namespace {

// -----------------------------------------------------------------------------
// 布局常量（来自 M4 规格 §1 与 M0 组件高度；只作推导参数，不是屏幕坐标）
// -----------------------------------------------------------------------------
constexpr float kSearchToToolbarGap = 16.0f;  // 搜索栏 → 工具栏
constexpr float kSectionGap = 12.0f;          // 工具栏/表格/分页之间
constexpr float kPageBottomPadding = 20.0f;   // 页面底部留白
constexpr float kScrollbarReserve = 10.0f;    // 页面纵向滚动条预留宽
constexpr float kTableHeaderHeight = 40.0f;   // 表头高（表格组件规范）
constexpr float kTableRowHeight = 60.0f;      // M4 行高（Web 实测 63，C++ 目标 60）
constexpr float kTableEmptyHeight = 120.0f;   // 空态占位高
constexpr float kApplyButtonWidth = 136.0f;   // 「发起入住申请」136×32（规格 §1.3）
constexpr float kApplyButtonHeight = 32.0f;

// 工具栏主按钮配色（规格 §1.3：底 #E5F7F5 / 字主色 / 边框 #7FDBD7）
constexpr ImU32 kApplyBackground = IM_COL32(0xE5, 0xF7, 0xF5, 0xFF);
constexpr ImU32 kApplyBorder = IM_COL32(0x7F, 0xDB, 0xD7, 0xFF);

// -----------------------------------------------------------------------------
// 页面私有状态（World 资源；渲染系统只改本结构 + 调数据服务）
// -----------------------------------------------------------------------------
struct CheckInListPageState {
    bool needRefresh = true;  // 首次进入 / 查询条件变化 / 从表单返回后置位
    std::vector<data::CheckInRecord> allRows;  // 当前查询结果全集
    std::vector<data::CheckInRecord> rows;     // 当前页切片（表格数据源）
    std::vector<ui::SearchField> searchFields; // 老人姓名 / 身份证号
    ui::PaginationState pagination;
};

// -----------------------------------------------------------------------------
// 小工具
// -----------------------------------------------------------------------------
void EnqueueNavigatePath(World& world, const char* path) {
    if (auto* queue = world.TryResource<CommandQueue>()) {
        CommandQueue::Command command;
        command.type = CommandQueue::Type::Navigate;
        command.arg0 = path;
        queue->commands.push_back(std::move(command));
    }
}

data::ICheckInService* CheckInService(World& world) {
    auto* services = world.TryResource<DataServices>();
    return services != nullptr ? services->checkIn.get() : nullptr;
}

void EnsureSearchFields(CheckInListPageState& state) {
    if (!state.searchFields.empty()) {
        return;
    }
    ui::SearchField elderName;
    elderName.key = "elderName";
    elderName.label = "老人姓名";
    elderName.controlWidth = 192.0f;
    ui::SearchField idCard;
    idCard.key = "idCard";
    idCard.label = "身份证号";
    idCard.controlWidth = 192.0f;
    state.searchFields.push_back(std::move(elderName));
    state.searchFields.push_back(std::move(idCard));
}

// 入住期限：startDate ~ endDate（缺一显示可用的一半）
std::string BuildStayPeriod(const data::CheckInRecord& record) {
    if (record.startDate.empty() && record.endDate.empty()) {
        return std::string();
    }
    if (record.startDate.empty()) {
        return record.endDate;
    }
    if (record.endDate.empty()) {
        return record.startDate;
    }
    return record.startDate + " ~ " + record.endDate;
}

const char* StatusLabel(int status) { return status == 0 ? "已入住" : "已退住"; }

void SlicePage(CheckInListPageState& state) {
    const int pageSize = std::max(1, state.pagination.pageSize);
    const int totalCount = static_cast<int>(state.allRows.size());
    const int totalPages = std::max(1, (totalCount + pageSize - 1) / pageSize);
    state.pagination.page = std::clamp(state.pagination.page, 1, totalPages);
    const int begin = (state.pagination.page - 1) * pageSize;
    const int end = std::min(totalCount, begin + pageSize);
    state.rows.clear();
    state.rows.reserve(static_cast<std::size_t>(std::max(0, end - begin)));
    for (int index = begin; index < end; ++index) {
        state.rows.push_back(state.allRows[static_cast<std::size_t>(index)]);
    }
}

void RefreshList(CheckInListPageState& state, data::ICheckInService& service) {
    data::CheckInQuery query;
    if (state.searchFields.size() > 0) {
        query.elderName = state.searchFields[0].value;
    }
    if (state.searchFields.size() > 1) {
        query.idCard = state.searchFields[1].value;
    }
    state.allRows = service.List(query);
    state.pagination.total = static_cast<int>(state.allRows.size());
    SlicePage(state);
}

// -----------------------------------------------------------------------------
// 表格列 / 行数据
// -----------------------------------------------------------------------------
std::vector<ui::TableColumn> BuildColumns() {
    std::vector<ui::TableColumn> columns;
    // 序号列由 TableDrawOptions.showIndex 提供（宽 50）；以下为剩余 8 列。
    // 列宽取 M4 规格相邻比例的紧凑值，保证 1280×720 下 9 列一屏可见（不出现横向滚动条）；
    // 1024×640 下超宽由表格组件横向滚动处理。
    columns.push_back(ui::TableColumn{"elderName", "老人姓名", 100.0f, ui::Align::Center,
                                      ui::CellKind::Text});
    columns.push_back(ui::TableColumn{"idCard", "身份证号", 155.0f, ui::Align::Center,
                                      ui::CellKind::Text});
    columns.push_back(ui::TableColumn{"bedNumber", "入住床位", 95.0f, ui::Align::Center,
                                      ui::CellKind::Text});
    columns.push_back(ui::TableColumn{"nursingLevelName", "护理等级名称", 130.0f,
                                      ui::Align::Center, ui::CellKind::Text});
    columns.push_back(ui::TableColumn{"stayPeriod", "入住期限", 175.0f, ui::Align::Center,
                                      ui::CellKind::Text});
    columns.push_back(ui::TableColumn{"status", "状态", 85.0f, ui::Align::Center,
                                      ui::CellKind::StatusTag});
    columns.push_back(ui::TableColumn{"createTime", "创建时间", 155.0f, ui::Align::Center,
                                      ui::CellKind::Text});
    columns.push_back(ui::TableColumn{"actions", "操作", 80.0f, ui::Align::Center,
                                      ui::CellKind::Actions});
    return columns;
}

std::vector<ui::TableRow> BuildTableRows(const std::vector<data::CheckInRecord>& records) {
    std::vector<ui::TableRow> rows;
    rows.reserve(records.size());
    for (const data::CheckInRecord& record : records) {
        ui::TableRow row;
        row.id = record.id;
        // StatusTag 颜色由 enabled 决定：已入住=绿、已退住=红
        row.enabled = (record.status == 0);
        row.cells.push_back(record.elderName);
        row.cells.push_back(record.idCard);
        row.cells.push_back(record.bedNumber);
        row.cells.push_back(record.nursingLevelName);
        row.cells.push_back(BuildStayPeriod(record));
        row.cells.push_back(StatusLabel(record.status));
        row.cells.push_back(record.createTime);
        row.cells.push_back("查看");
        rows.push_back(std::move(row));
    }
    return rows;
}

// -----------------------------------------------------------------------------
// M4 入住办理列表页渲染系统
// -----------------------------------------------------------------------------
class CheckInListPageSystem final : public ISystem {
public:
    const char* Name() const override { return "CheckInListPageSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& /*frame*/) override {
        const RouteState* route = world.TryResource<RouteState>();
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        if (route == nullptr || shell == nullptr || route->current != Route::CheckInList) {
            m_visible = false;
            return;
        }
        CheckInListPageState* state = world.TryResource<CheckInListPageState>();
        data::ICheckInService* service = CheckInService(world);
        if (state == nullptr || service == nullptr) {
            m_visible = false;
            return;
        }

        // 每次进入页面（含从表单/详情返回）重新拉取 SQLite 最新数据；搜索条件保留
        if (!m_visible) {
            m_visible = true;
            EnsureSearchFields(*state);
            state->needRefresh = true;
        }
        if (state->needRefresh) {
            RefreshList(*state, *service);
            state->needRefresh = false;
        }

        const layout::Rect pageViewport = shell->pageViewport;
        if (pageViewport.Empty()) {
            return;
        }
        DrawPage(world, pageViewport, *state);
    }

private:
    bool m_visible = false;

    // 页面主区域：搜索栏 → 工具栏 → 表格（余高）→ 分页（贴底）；整体可纵向滚动
    void DrawPage(World& world, const layout::Rect& pageViewport, CheckInListPageState& state) {
        ImGui::SetNextWindowPos(ui::Min(pageViewport));
        ImGui::SetNextWindowSize(ImVec2(pageViewport.w, pageViewport.h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;
        const bool open = ImGui::Begin("##checkin_list_page", nullptr, flags);
        if (open) {
            const float scrollY = ImGui::GetScrollY();
            const float contentWidth = std::max(1.0f, pageViewport.w - kScrollbarReserve);
            const auto localToScreen = [&pageViewport, scrollY](const layout::Rect& local) {
                return layout::Rect{pageViewport.x + local.x, pageViewport.y + local.y - scrollY,
                                    local.w, local.h};
            };

            // ---- 布局推导（全部由 pageViewport + 组件高度函数得到）----
            const float searchHeight = std::max(
                0.0f, ui::SearchBarHeight(static_cast<int>(state.searchFields.size())));
            const layout::Rect searchLocal{0.0f, 0.0f, contentWidth, searchHeight};
            float cursorY = searchLocal.Bottom() + kSearchToToolbarGap;

            const layout::Rect toolbarLocal{0.0f, cursorY,
                                            std::min(kApplyButtonWidth, contentWidth),
                                            kApplyButtonHeight};
            cursorY = toolbarLocal.Bottom() + kSectionGap;

            const std::size_t rowCount = state.rows.size();
            const float tableHeight =
                kTableHeaderHeight +
                (rowCount > 0 ? static_cast<float>(rowCount) * kTableRowHeight : kTableEmptyHeight);
            const layout::Rect tableLocal{0.0f, cursorY, contentWidth, tableHeight};
            cursorY = tableLocal.Bottom() + kSectionGap;

            const float paginationHeight = std::max(0.0f, ui::PaginationHeight());
            // 分页优先贴内容区底部；内容不足时跟随表格下方
            const float paginationBottomY =
                pageViewport.h - paginationHeight - kPageBottomPadding;
            const float paginationY = std::max(cursorY, paginationBottomY);
            const layout::Rect paginationLocal{0.0f, paginationY, contentWidth, paginationHeight};
            const float contentHeight = paginationLocal.Bottom() + kPageBottomPadding;

            // ---- 搜索栏 ----
            const ui::SearchBarResult search =
                ui::DrawSearchBar(localToScreen(searchLocal), state.searchFields);
            if (search.search || search.reset) {
                state.pagination.page = 1;
                state.needRefresh = true;
            }

            // ---- 工具栏：发起入住申请（136×32 主按钮，跳办理表单）----
            if (DrawApplyButton(ImGui::GetWindowDrawList(), localToScreen(toolbarLocal))) {
                EnqueueNavigatePath(world, "/enterQuit/checkInForm");
            }

            // ---- 数据表格（9 列：序号 + 8 列；行高 60）----
            const std::vector<ui::TableColumn> columns = BuildColumns();
            std::vector<ui::TableRow> tableRows = BuildTableRows(state.rows);
            ui::TableDrawOptions options;
            options.showCheckbox = false;
            options.showIndex = true;  // 序号列（宽 50）
            options.rowHeight = kTableRowHeight;
            options.actionLabels = {"查看"};
            std::vector<int> checkedIds;  // 本页无勾选，组件仍接收引用
            const render::TextureCache* textures = world.TryResource<render::TextureCache>();
            const ui::TableResult table = ui::DrawDataTable(
                localToScreen(tableLocal), columns, tableRows, checkedIds, textures, options);
            if (table.clickedAction >= 0 && table.actionRowId > 0) {
                // 操作列仅「查看」：写跨页选中记录后跳详情
                if (auto* selection = world.TryResource<CheckInSelection>()) {
                    selection->id = table.actionRowId;
                }
                EnqueueNavigatePath(world, "/enterQuit/checkInInfo");
            }

            // ---- 分页（贴底）----
            const ui::PaginationResult pagination =
                ui::DrawPagination(localToScreen(paginationLocal), state.pagination);
            if (pagination.changed) {
                SlicePage(state);
            }

            // 撑起页面滚动范围（组件用绝对坐标绘制，不参与 ImGui 内容尺寸计算）
            ImGui::SetCursorPos(ImVec2(0.0f, contentHeight));
            ImGui::Dummy(ImVec2(1.0f, 1.0f));
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    // 「发起入住申请」主按钮（136×32，浅青底/主色字/浅青边框 + 加号图标）
    bool DrawApplyButton(ImDrawList* drawList, const layout::Rect& rect) const {
        if (drawList == nullptr || rect.Empty()) {
            return false;
        }
        const bool pressed = ui::InvisibleButtonAt("##checkin_apply_button", rect);
        const bool hovered = ImGui::IsItemHovered();
        const theme::Palette& colors = theme::Colors();
        ui::FillRounded(drawList, rect, kApplyBackground, 4.0f);
        ui::StrokeRounded(drawList, rect, kApplyBorder, 4.0f, 1.0f);
        if (hovered) {
            ui::FillRounded(drawList, rect, theme::WithAlpha(IM_COL32(0, 0, 0, 255), 0.04f), 4.0f);
        }
        ui::ScopedFontSize font(14.0f);
        const char* label = "发起入住申请";
        const float textWidth = ui::TextSize(label).x;
        const float iconSize = 13.0f;
        const float gap = 4.0f;
        const float groupWidth = iconSize + gap + textWidth;
        const float startX = rect.CenterX() - groupWidth * 0.5f;
        const ImVec2 iconCenter(startX + iconSize * 0.5f, rect.CenterY());
        const float half = iconSize * 0.32f;
        const ImU32 color = colors.primary;
        drawList->AddLine(ImVec2(iconCenter.x - half, iconCenter.y),
                          ImVec2(iconCenter.x + half, iconCenter.y), color, 1.5f);
        drawList->AddLine(ImVec2(iconCenter.x, iconCenter.y - half),
                          ImVec2(iconCenter.x, iconCenter.y + half), color, 1.5f);
        ui::Text(drawList,
                 ImVec2(startX + iconSize + gap,
                        rect.CenterY() - ImGui::GetFontSize() * 0.5f),
                 color, label);
        return pressed;
    }
};

}  // namespace

void RegisterCheckInListModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<CheckInListPageState>();
    scheduler.Add(std::make_unique<CheckInListPageSystem>(), 200);
}

}  // namespace zzyl
