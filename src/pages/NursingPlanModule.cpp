// =============================================================================
// M5 服务管理-护理计划 · cpp-dev2（Sprint 4）
//
// 依据：
//  - Web《Sprint4_功能规格_v0.1.md》§1（列表 5 列：序号/名称/状态/创建时间/操作；
//    新增/修改/查看弹窗 1000×614：顶部 名称*/状态*/排序* + 护理项目子表 5 列、表头 50、表体 300 可滚动）
//  - 架构 v0.4 §4 / 任务单 Sprint4 §1；种子 3 条（1号/2号/3号护理计划，2号禁用）
//
// 数据流（渲染与数据分离）：
//  - 列表：DataServices.nursingPlan->List(query)；被护理等级引用的计划禁用「修改/删除」
//    （通过 nursingLevel->List() 的 planId 集合判定，不直接读库）；
//  - 表单：项目下拉 = nursingProject->List()；保存 Add/Update（projects 随 record 传，服务内事务替换）；
//  - 删除确认 / 启停确认：DrawConfirmDialog + SetStatus/Remove + Toast + 刷新。
//
// 布局全部由 ShellLayout::pageViewport 推导；弹窗由 ui::BeginFormDialog/EndFormDialog 负责框架。
// =============================================================================

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
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
#include "zzyl/utils/CsvExport.h"

namespace zzyl {
namespace {

// -----------------------------------------------------------------------------
// 设计常量（Web 实测基准）
// -----------------------------------------------------------------------------
constexpr float kPagePadding = 16.0f;
constexpr float kLabelWidth = 140.0f;      // 弹窗 label-width: 140
constexpr float kControlHeight = 32.0f;
constexpr float kFormRowGap = 16.0f;       // 弹窗内行间距
constexpr float kProjectHeaderHeight = 50.0f;
constexpr float kProjectBodyHeight = 300.0f;  // 子表体高（可滚动）
constexpr float kProjectRowHeight = 50.0f;
constexpr int kPageSize = 10;
constexpr int kDialogWidth = 1000;  // 弹窗宽（高由内容自适应 ≈614）

const char* CycleText(int cycle) {
    switch (cycle) {
        case 0:
            return "天";
        case 1:
            return "周";
        case 2:
            return "月";
        default:
            return "天";
    }
}
constexpr int kCycleCount = 3;

// -----------------------------------------------------------------------------
// 页面私有状态
// -----------------------------------------------------------------------------
struct ProjectRowState {
    int projectId = -1;  // -1 = 未选择
    char executeTime[16] = "08:00:00";
    int cycle = 0;
    char frequency[8] = "1";
};

struct NursingPlanPageState {
    // 搜索
    char keyword[64] = {};
    int statusFilterIndex = 0;  // 0 全部 / 1 启用 / 2 禁用
    // 列表
    std::vector<data::NursingPlanRecord> allRows;
    int page = 1;
    bool reload = true;
    // 弹窗
    ui::DialogState dialog;
    bool readOnly = false;
    int editingId = 0;
    char planName[64] = {};
    int status = 1;  // 1 启用 / 0 禁用
    char sortNo[8] = "1";
    std::vector<ProjectRowState> projects;
    std::string dialogError;  // 服务层错误（保存失败等统一提示）
    // 逐项校验错误（就地红字 + 红框）
    std::string ePlanName;
    std::string eSort;
    std::string eProjects;
    // 数据字典
    std::vector<data::NursingProjectRecord> projectOptions;
    bool optionsLoaded = false;
    // 确认框
    bool confirmOpen = false;
    int confirmKind = 0;  // 0 删除 / 1 启停
    int confirmId = 0;
    int confirmTargetStatus = 1;
    std::string confirmTitle;
    std::string confirmMessage;
};

// -----------------------------------------------------------------------------
// 小工具
// -----------------------------------------------------------------------------
void PushToastMsg(World& world, const std::string& text, ui::ToastType type) {
    if (ui::ToastState* toasts = world.TryResource<ui::ToastState>()) {
        ui::PushToast(*toasts, text, type);
    }
}

std::string TextOf(const char* chars) {
    return chars != nullptr ? std::string(chars) : std::string();
}

int ParseInt(const std::string& text, int fallback) {
    if (text.empty()) {
        return fallback;
    }
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') {
        return fallback;
    }
    return static_cast<int>(value);
}

bool HasDuplicateProjects(const std::vector<ProjectRowState>& projects, std::string* error) {
    std::set<int> seen;
    for (const ProjectRowState& row : projects) {
        if (row.projectId < 0) {
            continue;
        }
        if (!seen.insert(row.projectId).second) {
            if (error != nullptr) {
                *error = "请勿选择重复的护理项目";
            }
            return true;
        }
    }
    return false;
}

data::INursingPlanService* PlanService(World& world) {
    DataServices* services = world.TryResource<DataServices>();
    return (services != nullptr && services->nursingPlan) ? services->nursingPlan.get() : nullptr;
}
data::INursingLevelService* LevelService(World& world) {
    DataServices* services = world.TryResource<DataServices>();
    return (services != nullptr && services->nursingLevel) ? services->nursingLevel.get() : nullptr;
}
data::INursingProjectService* ProjectService(World& world) {
    DataServices* services = world.TryResource<DataServices>();
    return (services != nullptr && services->nursingProject) ? services->nursingProject.get() : nullptr;
}

// -----------------------------------------------------------------------------
// 布局工具：在弹窗内以 label 对齐绘制一行
// -----------------------------------------------------------------------------
struct FormRow {
    ImVec2 origin;   // 行起点（屏幕坐标）
    float labelWidth = kLabelWidth;
    float fieldGap = 8.0f;

    float ControlX() const { return origin.x + labelWidth + fieldGap; }
    float ControlWidth(float contentWidth) const {
        return std::max(120.0f, contentWidth - labelWidth - fieldGap);
    }
};

void mapperTextError(float x, float y, const std::string& text) {
    if (text.empty()) {
        return;
    }
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ui::ScopedFontSize font(12.0f);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s", text.c_str());
}

void DrawRowLabel(const FormRow& row, const char* label, bool required) {
    const char* star = required ? "*" : "";
    const float starWidth = ImGui::CalcTextSize(star).x;
    const float labelLen = ImGui::CalcTextSize(label).x;
    ImGui::SetCursorScreenPos(ImVec2(row.origin.x + row.labelWidth - starWidth - labelLen,
                                     row.origin.y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f));
    if (required) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "*");
        ImGui::SameLine(0.0f, 0.0f);
    }
    ImGui::TextUnformatted(label);
}

void PushInputStyle(bool invalid) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 9.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(theme::Colors().panel));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ImGui::ColorConvertU32ToFloat4(invalid ? theme::Colors().danger
                                                                 : theme::Colors().border));
}

void PopInputStyle() {
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

bool DrawTextInput(const FormRow& row, const char* id, char* buffer, size_t size,
                   const char* hint, float width, bool invalid = false,
                   ImGuiInputTextFlags flags = 0) {
    PushInputStyle(invalid);
    ImGui::SetCursorScreenPos(ImVec2(row.ControlX(), row.origin.y));
    ImGui::SetNextItemWidth(width);
    bool edited = false;
    {
        ui::ScopedFontSize font(14.0f);
        if (hint != nullptr) {
            edited = ImGui::InputTextWithHint(id, hint, buffer, size, flags);
        } else {
            edited = ImGui::InputText(id, buffer, size, flags);
        }
    }
    PopInputStyle();
    return edited;
}

// -----------------------------------------------------------------------------
// 页面系统
// -----------------------------------------------------------------------------
class NursingPlanPageSystem final : public ISystem {
public:
    const char* Name() const override { return "NursingPlanPageSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& frame) override {
        const RouteState* route = world.TryResource<RouteState>();
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        if (route == nullptr || shell == nullptr || route->current != Route::NursingPlan) {
            m_visible = false;
            return;
        }
        NursingPlanPageState* state = world.TryResource<NursingPlanPageState>();
        data::INursingPlanService* service = PlanService(world);
        if (state == nullptr || service == nullptr) {
            return;
        }
        if (!m_visible) {
            m_visible = true;
            state->reload = true;
            state->dialog.open = false;
            m_resetScroll = true;
        }
        if (state->reload) {
            ReloadList(world, *state, *service);
        }
        DrawPage(world, *shell, *state, *service);

        if (ui::ToastState* toasts = world.TryResource<ui::ToastState>()) {
            ui::DrawToasts(shell->viewport, *toasts, frame.deltaTime);
        }
    }

private:
    void ReloadList(World& world, NursingPlanPageState& state, data::INursingPlanService& service) {
        data::NursingPlanQuery query;
        query.keyword = TextOf(state.keyword);
        query.status = state.statusFilterIndex == 1 ? 1 : (state.statusFilterIndex == 2 ? 0 : -1);
        state.allRows = service.List(query);
        state.reload = false;
        if (state.page < 1) {
            state.page = 1;
        }

        // 被引用次数由契约字段 levelRefCount 提供（>0 时修改/删除禁用）
        (void)world;
    }

    bool IsReferenced(const data::NursingPlanRecord& plan) const {
        return plan.levelRefCount > 0;  // 契约字段：被护理等级引用次数（>0 禁用修改/删除）
    }

    void DrawPage(World& world, const layout::ShellLayout& shell, NursingPlanPageState& state,
                  data::INursingPlanService& service) {
        const layout::Rect pageViewport = shell.pageViewport;
        ImGui::SetNextWindowPos(ui::Min(pageViewport));
        ImGui::SetNextWindowSize(ImVec2(pageViewport.w, pageViewport.h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;
        const bool open = ImGui::Begin("##nursing_plan_page", nullptr, flags);
        if (open) {
            if (m_resetScroll) {
                m_resetScroll = false;
                ImGui::SetScrollY(0.0f);
            }
            const ImVec2 pageOrigin = ImGui::GetCursorScreenPos();
            const float contentWidth = pageViewport.w - 2.0f * kPagePadding;
            const float pageX = pageOrigin.x + kPagePadding;
            float cursorY = pageOrigin.y + kPagePadding;

            // ---- 搜索栏 ----
            std::vector<ui::SearchField> fields;
            ui::SearchField keywordField;
            keywordField.key = "keyword";
            keywordField.label = "名称";
            keywordField.value = TextOf(state.keyword);
            keywordField.type = ui::SearchFieldType::Text;
            keywordField.controlWidth = 192.0f;
            fields.push_back(keywordField);
            ui::SearchField statusField;
            statusField.key = "status";
            statusField.label = "状态";
            statusField.value = state.statusFilterIndex == 1
                                    ? "启用"
                                    : (state.statusFilterIndex == 2 ? "禁用" : "全部");
            statusField.type = ui::SearchFieldType::Select;
            statusField.options = {"全部", "启用", "禁用"};
            statusField.controlWidth = 200.0f;
            fields.push_back(statusField);

            const float searchHeight = ui::SearchBarHeight(static_cast<int>(fields.size()));
            const ui::SearchBarResult searchResult =
                ui::DrawSearchBar(layout::Rect{pageX, cursorY, contentWidth, searchHeight}, fields);
            if (searchResult.search || searchResult.reset) {
                if (searchResult.reset) {
                    state.keyword[0] = '\0';
                    state.statusFilterIndex = 0;
                } else {
                    std::snprintf(state.keyword, sizeof(state.keyword), "%s",
                                  fields[0].value.c_str());
                    const std::string statusText = fields[1].value;
                    state.statusFilterIndex =
                        statusText == "启用" ? 1 : (statusText == "禁用" ? 2 : 0);
                }
                state.page = 1;
                state.reload = true;
            }
            cursorY += searchHeight + 8.0f;

            // ---- 工具栏 ----
            std::vector<ui::ToolbarButton> buttons;
            ui::ToolbarButton addButton;
            addButton.kind = ui::ToolbarKind::Add;
            addButton.label = "新增";
            buttons.push_back(addButton);
            ui::ToolbarButton exportButton;
            exportButton.kind = ui::ToolbarKind::Export;
            exportButton.label = "导出";
            buttons.push_back(exportButton);
            const ui::ToolbarResult toolbarResult =
                ui::DrawToolbar(layout::Rect{pageX, cursorY, contentWidth, ui::ToolbarHeight()},
                                buttons);
            if (toolbarResult.clicked == 0) {
                OpenCreateDialog(state);
            } else if (toolbarResult.clicked == 1) {
                ExportPlans(world, state);
            }
            cursorY += ui::ToolbarHeight() + 8.0f;

            // ---- 表格 + 分页（分页贴底） ----
            const float paginationHeight = ui::PaginationHeight();
            const float tableHeight =
                pageViewport.y + pageViewport.h - kPagePadding - paginationHeight - 8.0f - cursorY;
            const float tableH = std::max(120.0f, tableHeight);
            DrawTable(world, state, service, layout::Rect{pageX, cursorY, contentWidth, tableH});

            ui::PaginationState pagination;
            pagination.page = state.page;
            pagination.pageSize = kPageSize;
            pagination.total = static_cast<int>(state.allRows.size());
            const ui::PaginationResult paginationResult = ui::DrawPagination(
                layout::Rect{pageX, cursorY + tableH + 8.0f, contentWidth, paginationHeight},
                pagination);
            if (paginationResult.changed) {
                state.page = pagination.page;
            }

            ImGui::SetCursorScreenPos(ImVec2(pageX, cursorY + tableH + paginationHeight + 12.0f));
            ImGui::Dummy(ImVec2(1.0f, 1.0f));
        }
        ImGui::End();
        ImGui::PopStyleVar();

        // ---- 弹窗 / 确认框（页面之上） ----
        if (state.dialog.open) {
            DrawDialog(world, state, service);
        }
        if (state.confirmOpen) {
            const ui::ConfirmAction action = ui::DrawConfirmDialog(
                shell.viewport, state.confirmOpen, state.confirmTitle, state.confirmMessage);
            if (action == ui::ConfirmAction::Ok) {
                ApplyConfirm(world, state, service);
            }
        }
    }

    void DrawTable(World& world, NursingPlanPageState& state, data::INursingPlanService& service,
                   const layout::Rect& area) {
        const auto pageBegin = static_cast<std::size_t>((state.page - 1) * kPageSize);
        const auto pageEnd = std::min(state.allRows.size(), pageBegin + static_cast<std::size_t>(kPageSize));

        std::vector<ui::TableColumn> columns;
        ui::TableColumn nameColumn;
        nameColumn.key = "planName";
        nameColumn.title = "名称";
        nameColumn.width = 300.0f;
        nameColumn.align = ui::Align::Center;
        columns.push_back(nameColumn);
        ui::TableColumn statusColumn;
        statusColumn.key = "status";
        statusColumn.title = "状态";
        statusColumn.width = 120.0f;
        statusColumn.kind = ui::CellKind::StatusTag;
        columns.push_back(statusColumn);
        ui::TableColumn timeColumn;
        timeColumn.key = "createTime";
        timeColumn.title = "创建时间";
        timeColumn.width = 180.0f;
        columns.push_back(timeColumn);
        ui::TableColumn actionColumn;
        actionColumn.key = "actions";
        actionColumn.title = "操作";
        actionColumn.width = 280.0f;
        actionColumn.kind = ui::CellKind::Actions;
        columns.push_back(actionColumn);

        std::vector<ui::TableRow> rows;
        m_rowActions.clear();
        for (std::size_t index = pageBegin; index < pageEnd; ++index) {
            const data::NursingPlanRecord& plan = state.allRows[index];
            const bool referenced = IsReferenced(plan);
            std::vector<std::string> actions;
            if (!referenced) {
                actions.push_back("修改");
                actions.push_back("删除");
            }
            actions.push_back("查看");
            actions.push_back(plan.status == 1 ? "禁用" : "启用");

            std::string actionText;
            for (std::size_t actionIndex = 0; actionIndex < actions.size(); ++actionIndex) {
                if (actionIndex > 0) {
                    actionText += "|";
                }
                actionText += actions[actionIndex];
            }
            m_rowActions[plan.id] = actions;

            ui::TableRow row;
            row.id = plan.id;
            row.cells.push_back(plan.planName);
            row.cells.push_back(plan.status == 1 ? "启用" : "禁用");
            row.cells.push_back(plan.createTime);
            row.cells.push_back(actionText);
            row.enabled = plan.status == 1;
            rows.push_back(row);
        }

        ui::TableDrawOptions options;
        options.showCheckbox = false;
        options.showIndex = true;
        options.rowHeight = 49.0f;
        std::vector<int> checkedIds;
        const ui::TableResult result =
            ui::DrawDataTable(area, columns, rows, checkedIds, nullptr, options);
        if (result.clickedAction >= 0 && result.actionRowId >= 0) {
            const auto found = m_rowActions.find(result.actionRowId);
            if (found != m_rowActions.end() &&
                result.clickedAction < static_cast<int>(found->second.size())) {
                HandleRowAction(world, state, service, result.actionRowId,
                                found->second[static_cast<std::size_t>(result.clickedAction)]);
            }
        }
        for (const ui::TableResult::SwitchToggle& toggle : result.switchToggles) {
            std::string error;
            if (service.SetStatus(toggle.rowId, toggle.enabled, &error)) {
                PushToastMsg(world, toggle.enabled ? "启用成功" : "禁用成功", ui::ToastType::Success);
                state.reload = true;
            } else {
                PushToastMsg(world, error.empty() ? "操作失败" : error, ui::ToastType::Error);
            }
        }
    }

    void HandleRowAction(World& world, NursingPlanPageState& state,
                         data::INursingPlanService& service, int planId,
                         const std::string& action) {
        const auto found = std::find_if(state.allRows.begin(), state.allRows.end(),
                                        [planId](const data::NursingPlanRecord& plan) {
                                            return plan.id == planId;
                                        });
        if (found == state.allRows.end()) {
            return;
        }
        const data::NursingPlanRecord& plan = *found;
        if (action == "查看") {
            OpenEditDialog(world, state, service, plan, true);
        } else if (action == "修改") {
            if (IsReferenced(plan)) {
                PushToastMsg(world, "该计划已被护理等级引用，不可修改", ui::ToastType::Warning);
                return;
            }
            OpenEditDialog(world, state, service, plan, false);
        } else if (action == "删除") {
            if (IsReferenced(plan)) {
                PushToastMsg(world, "该计划已被护理等级引用，不可删除", ui::ToastType::Warning);
                return;
            }
            state.confirmOpen = true;
            state.confirmKind = 0;
            state.confirmId = plan.id;
            state.confirmTitle = "删除护理计划";
            state.confirmMessage = "是否确认删除该护理计划？";
        } else if (action == "启用" || action == "禁用") {
            state.confirmOpen = true;
            state.confirmKind = 1;
            state.confirmId = plan.id;
            state.confirmTargetStatus = action == "启用" ? 1 : 0;
            state.confirmTitle = action == "启用" ? "启用护理计划" : "禁用护理计划";
            state.confirmMessage = "是否确认" + action + "该护理计划？";
        }
    }

    void ApplyConfirm(World& world, NursingPlanPageState& state,
                      data::INursingPlanService& service) {
        std::string error;
        if (state.confirmKind == 0) {
            if (service.Remove({state.confirmId}, &error)) {
                PushToastMsg(world, "删除成功", ui::ToastType::Success);
            } else {
                PushToastMsg(world, error.empty() ? "删除失败" : error, ui::ToastType::Error);
            }
        } else {
            if (service.SetStatus(state.confirmId, state.confirmTargetStatus == 1, &error)) {
                PushToastMsg(world, state.confirmTargetStatus == 1 ? "启用成功" : "禁用成功",
                             ui::ToastType::Success);
            } else {
                PushToastMsg(world, error.empty() ? "操作失败" : error, ui::ToastType::Error);
            }
        }
        state.reload = true;
        state.confirmOpen = false;
    }

    // -------------------------------------------------------------------------
    // 弹窗
    // -------------------------------------------------------------------------
    void OpenCreateDialog(NursingPlanPageState& state) {
        state.dialog.open = true;
        state.dialog.width = static_cast<float>(kDialogWidth);
        state.dialog.title = "添加护理计划";
        state.readOnly = false;
        state.editingId = 0;
        state.planName[0] = '\0';
        state.status = 1;
        std::snprintf(state.sortNo, sizeof(state.sortNo), "1");
        state.projects.clear();
        state.projects.push_back(ProjectRowState{});
        state.dialogError.clear();
        state.ePlanName.clear();
        state.eSort.clear();
        state.eProjects.clear();
    }

    void OpenEditDialog(World& world, NursingPlanPageState& state,
                        data::INursingPlanService& service, const data::NursingPlanRecord& plan,
                        bool readOnly) {
        data::NursingPlanRecord detail = plan;
        std::string error;
        if (service.Get(plan.id, &detail, &error)) {
            // 详情含 projects
        }
        state.dialog.open = true;
        state.dialog.width = static_cast<float>(kDialogWidth);
        state.dialog.title = readOnly ? "查看护理计划" : "修改护理计划";
        state.readOnly = readOnly;
        state.editingId = detail.id;
        std::snprintf(state.planName, sizeof(state.planName), "%s", detail.planName.c_str());
        state.status = detail.status;
        std::snprintf(state.sortNo, sizeof(state.sortNo), "%d", detail.sortNo);
        state.projects.clear();
        for (const data::NursingPlanProjectRecord& project : detail.projects) {
            ProjectRowState row;
            row.projectId = project.projectId;
            std::snprintf(row.executeTime, sizeof(row.executeTime), "%s", project.executeTime.c_str());
            row.cycle = project.executeCycle;
            std::snprintf(row.frequency, sizeof(row.frequency), "%d", project.executeFrequency);
            state.projects.push_back(row);
        }
        if (state.projects.empty()) {
            state.projects.push_back(ProjectRowState{});
        }
        state.dialogError.clear();
        state.ePlanName.clear();
        state.eSort.clear();
        state.eProjects.clear();
        (void)world;
    }

    void EnsureProjectOptions(World& world, NursingPlanPageState& state) {
        if (state.optionsLoaded) {
            return;
        }
        state.projectOptions.clear();
        if (data::INursingProjectService* projectService = ProjectService(world)) {
            state.projectOptions = projectService->List(data::NursingProjectQuery{});
        }
        state.optionsLoaded = true;
    }

    void DrawDialog(World& world, NursingPlanPageState& state, data::INursingPlanService& service) {
        EnsureProjectOptions(world, state);
        if (!ui::BeginFormDialog(layout::Rect{}, state.dialog)) {
            // 未打开：确保配对调用
            return;
        }
        const ImVec2 contentOrigin = ImGui::GetCursorScreenPos();
        const float contentWidth = ImGui::GetContentRegionAvail().x;

        // 错误提示
        if (!state.dialogError.empty()) {
            ui::ScopedFontSize font(13.0f);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                               state.dialogError.c_str());
        }

        float cursorY = ImGui::GetCursorScreenPos().y;
        // ---- 行1：名称 + 状态 ----
        FormRow row1;
        row1.origin = ImVec2(contentOrigin.x, cursorY);
        DrawRowLabel(row1, "护理计划名称", true);
        if (state.readOnly) {
            ImGui::SetCursorScreenPos(ImVec2(row1.ControlX(), row1.origin.y));
            ui::ScopedFontSize font(14.0f);
            ImGui::TextUnformatted(state.planName);
        } else {
            const bool nameEdited = DrawTextInput(row1, "##plan_name", state.planName,
                                                  sizeof(state.planName), "请输入护理计划名称",
                                                  350.0f, !state.ePlanName.empty());
            if (nameEdited) {
                state.ePlanName.clear();
            }
        }
        if (!state.readOnly && !state.ePlanName.empty()) {
            mapperTextError(row1.ControlX(), row1.origin.y + kControlHeight + 2.0f,
                            state.ePlanName);
        }
        // 状态（单选：启用/禁用）
        const float statusX = contentOrigin.x + contentWidth * 0.55f;
        ImGui::SetCursorScreenPos(ImVec2(statusX, row1.origin.y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f));
        {
            ui::ScopedFontSize font14(14.0f);
            ImGui::TextUnformatted("状态：");
            ImGui::SameLine(0.0f, 8.0f);
            if (state.readOnly) {
                ImGui::TextUnformatted(state.status == 1 ? "启用" : "禁用");
            } else {
                ImGui::RadioButton("启用##plan_status_on", &state.status, 1);
                ImGui::SameLine(0.0f, 24.0f);
                ImGui::RadioButton("禁用##plan_status_off", &state.status, 0);
            }
        }
        cursorY += kControlHeight + kFormRowGap +
                   (state.readOnly || state.ePlanName.empty() ? 0.0f : 16.0f);

        // ---- 行2：排序（步进器） ----
        FormRow row2;
        row2.origin = ImVec2(contentOrigin.x, cursorY);
        DrawRowLabel(row2, "排序", true);
        if (state.readOnly) {
            ImGui::SetCursorScreenPos(ImVec2(row2.ControlX(), row2.origin.y));
            ui::ScopedFontSize fontSort(14.0f);
            ImGui::TextUnformatted(state.sortNo);
        } else {
            // - / 输入 / +
            PushInputStyle(false);    ImGui::PushStyleColor(ImGuiCol_Button, theme::Colors().panel);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::Colors().panelHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::Colors().panelHover);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::Colors().textPrimary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);            ImGui::SetCursorScreenPos(ImVec2(row2.ControlX(), row2.origin.y));
            if (ImGui::Button("-##sort_minus")) {
                const int value = std::max(1, ParseInt(TextOf(state.sortNo), 1) - 1);
                std::snprintf(state.sortNo, sizeof(state.sortNo), "%d", value);
            }
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::SetNextItemWidth(90.0f);
            {
                ui::ScopedFontSize font(14.0f);
                ImGui::InputText("##sort_no", state.sortNo, sizeof(state.sortNo),
                                 ImGuiInputTextFlags_CharsDecimal);
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ImGui::Button("+##sort_plus")) {
                const int value = std::min(999999, ParseInt(TextOf(state.sortNo), 1) + 1);
                std::snprintf(state.sortNo, sizeof(state.sortNo), "%d", value);
            }
            PopInputStyle();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
        if (!state.readOnly && !state.eSort.empty()) {
            ImGui::SetCursorScreenPos(ImVec2(row2.ControlX(), row2.origin.y + kControlHeight + 2.0f));
            ui::ScopedFontSize font(12.0f);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                               state.eSort.c_str());
        }
        cursorY += kControlHeight + kFormRowGap +
                   (state.readOnly || state.eSort.empty() ? 0.0f : 16.0f);

        // ---- 行3：护理项目子表 ----
        DrawRowLabel(FormRow{ImVec2(contentOrigin.x, cursorY)}, "护理项目", false);
        const float tableX = contentOrigin.x + kLabelWidth + 8.0f;
        const float tableWidth = std::max(200.0f, contentWidth - kLabelWidth - 8.0f);
        DrawProjectTable(world, state, ImVec2(tableX, cursorY), tableWidth);
        float tableExtra = 0.0f;
        if (!state.readOnly && !state.eProjects.empty()) {
            ImGui::SetCursorScreenPos(
                ImVec2(tableX, cursorY + kProjectHeaderHeight + kProjectBodyHeight + 2.0f));
            ui::ScopedFontSize font(12.0f);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                               state.eProjects.c_str());
            tableExtra = 18.0f;
        }

        // 让内容高度撑开（子表体在下一次光标位置后由 EndFormDialog 计算）
        ImGui::SetCursorScreenPos(
            ImVec2(contentOrigin.x, cursorY + kProjectHeaderHeight + kProjectBodyHeight + 8.0f +
                                         tableExtra));
        ImGui::Dummy(ImVec2(1.0f, 1.0f));

        const ui::DialogResult result = ui::EndFormDialog();
        if (result == ui::DialogResult::Ok) {
            if (state.readOnly) {
                state.dialog.open = false;
                return;
            }
            SubmitDialog(world, state, service);
        } else if (result == ui::DialogResult::Cancel) {
            state.dialog.open = false;
        }
    }

    void DrawProjectTable(World& world, NursingPlanPageState& state, const ImVec2& origin,
                          float width) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // 表头（50 高、#F3F8F9）
        const ImVec2 headerMin = origin;
        const ImVec2 headerMax(origin.x + width, origin.y + kProjectHeaderHeight);
        drawList->AddRectFilled(headerMin, headerMax, theme::Colors().lightBlock, 4.0f);
        const float flexTotal = 2.0f + 1.8f + 1.2f + 1.0f + 0.6f;
        const float unit = width / flexTotal;
        const float colX[5] = {origin.x + 10.0f, origin.x + 2.0f * unit + 10.0f,
                               origin.x + 3.8f * unit + 10.0f, origin.x + 5.0f * unit + 10.0f,
                               origin.x + 6.0f * unit + 10.0f};
        const float colWidth[5] = {2.0f * unit - 20.0f, 1.8f * unit - 20.0f,
                                   1.2f * unit - 20.0f, 1.0f * unit - 20.0f,
                                   0.6f * unit - 12.0f};
        const char* titles[5] = {"护理项目名称", "期望服务时间", "执行周期", "执行频次", "操作"};
        {
            ui::ScopedFontSize font(14.0f);
            for (int index = 0; index < 5; ++index) {
                const ImVec2 textSize = ImGui::CalcTextSize(titles[index]);
                drawList->AddText(
                    ImVec2(colX[index] + (colWidth[index] - textSize.x) * 0.5f,
                           origin.y + (kProjectHeaderHeight - textSize.y) * 0.5f),
                    theme::Colors().textPrimary, titles[index]);
            }
        }

        // 表体（300 高，可滚动；child 内使用局部坐标，行数 > 6 时可滚动）
        const float childWidth = width;
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + kProjectHeaderHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImGui::ColorConvertU32ToFloat4(theme::Colors().panel));
        ImGui::BeginChild("##plan_project_body", ImVec2(childWidth, kProjectBodyHeight), false);
        int removeAt = -1;
        int addAt = -1;
        {
            const float childUnit = childWidth / flexTotal;
            const float localColX[5] = {10.0f, 2.0f * childUnit + 10.0f, 3.8f * childUnit + 10.0f,
                                        5.0f * childUnit + 10.0f, 6.0f * childUnit + 10.0f};
            const float localColWidth[5] = {2.0f * childUnit - 20.0f, 1.8f * childUnit - 20.0f,
                                            1.2f * childUnit - 20.0f, 1.0f * childUnit - 20.0f,
                                            0.6f * childUnit - 12.0f};
            for (std::size_t rowIndex = 0; rowIndex < state.projects.size(); ++rowIndex) {
                ProjectRowState& row = state.projects[rowIndex];
                const float rowTop = static_cast<float>(rowIndex) * kProjectRowHeight;
                const float fieldY = rowTop + (kProjectRowHeight - kControlHeight) * 0.5f;

                // 项目下拉
                PushInputStyle(false);
                ImGui::SetCursorPos(ImVec2(localColX[0], fieldY));
                ImGui::SetNextItemWidth(localColWidth[0]);
                {
                    ui::ScopedFontSize font(14.0f);
                    std::string preview = "请选择";
                    for (const data::NursingProjectRecord& project : state.projectOptions) {
                        if (project.id == row.projectId) {
                            preview = project.name;
                            break;
                        }
                    }
                    if (ImGui::BeginCombo(
                            (std::string("##plan_proj_") + std::to_string(rowIndex)).c_str(),
                            preview.c_str())) {
                        for (const data::NursingProjectRecord& project : state.projectOptions) {
                            if (ImGui::Selectable(project.name.c_str(),
                                                  project.id == row.projectId)) {
                                row.projectId = project.id;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                // 期望服务时间（HH:mm:ss）
                ImGui::SetCursorPos(ImVec2(localColX[1], fieldY));
                ImGui::SetNextItemWidth(localColWidth[1]);
                {
                    ui::ScopedFontSize font(14.0f);
                    ImGui::InputTextWithHint(
                        (std::string("##plan_time_") + std::to_string(rowIndex)).c_str(),
                        "请选择时间", row.executeTime, sizeof(row.executeTime));
                }

                // 执行周期
                ImGui::SetCursorPos(ImVec2(localColX[2], fieldY));
                ImGui::SetNextItemWidth(localColWidth[2]);
                {
                    ui::ScopedFontSize font(14.0f);
                    if (ImGui::BeginCombo(
                            (std::string("##plan_cycle_") + std::to_string(rowIndex)).c_str(),
                            CycleText(row.cycle))) {
                        for (int cycle = 0; cycle < kCycleCount; ++cycle) {
                            if (ImGui::Selectable(CycleText(cycle), row.cycle == cycle)) {
                                row.cycle = cycle;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }

                // 执行频次
                ImGui::SetCursorPos(ImVec2(localColX[3], fieldY));
                ImGui::SetNextItemWidth(localColWidth[3]);
                {
                    ui::ScopedFontSize font(14.0f);
                    ImGui::InputText(
                        (std::string("##plan_freq_") + std::to_string(rowIndex)).c_str(),
                        row.frequency, sizeof(row.frequency), ImGuiInputTextFlags_CharsDecimal);
                }
                PopInputStyle();

                // 操作：+ 新增行 / - 删除行（仅 >1 行时显示 -）
                const float buttonY = fieldY + 4.0f;
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, theme::Colors().panel);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::Colors().panelHover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::Colors().panelHover);
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::Colors().textPrimary);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                    ui::ScopedFontSize font(16.0f);
                    ImGui::SetCursorPos(ImVec2(localColX[4], buttonY));
                    if (ImGui::Button(
                            (std::string("+##plan_add_") + std::to_string(rowIndex)).c_str(),
                            ImVec2(28.0f, 24.0f))) {
                        addAt = static_cast<int>(rowIndex);
                    }
                    if (state.projects.size() > 1) {
                        ImGui::SameLine(0.0f, 6.0f);
                        if (ImGui::Button(
                                (std::string("-##plan_del_") + std::to_string(rowIndex)).c_str(),
                                ImVec2(28.0f, 24.0f))) {
                            removeAt = static_cast<int>(rowIndex);
                        }
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(4);
                }
            }
            // 撑开 child 内容高度（保证滚动范围）
            ImGui::SetCursorPos(
                ImVec2(0.0f, static_cast<float>(state.projects.size()) * kProjectRowHeight));
            ImGui::Dummy(ImVec2(1.0f, 1.0f));
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (addAt >= 0) {
            state.projects.push_back(ProjectRowState{});
        } else if (removeAt >= 0 && state.projects.size() > 1) {
            state.projects.erase(state.projects.begin() + removeAt);
        }
        (void)world;
    }

    void SubmitDialog(World& world, NursingPlanPageState& state,
                      data::INursingPlanService& service) {
        state.dialogError.clear();
        state.ePlanName.clear();
        state.eSort.clear();
        state.eProjects.clear();
        const std::string planName = TextOf(state.planName);
        if (planName.empty()) {
            state.ePlanName = "护理计划名称为空，请输入护理计划名称";
        }
        const int sortNo = ParseInt(TextOf(state.sortNo), 0);
        if (sortNo < 1) {
            state.eSort = "排序必须为 ≥1 的整数";
        }
        std::string duplicateError;
        if (HasDuplicateProjects(state.projects, &duplicateError)) {
            state.eProjects = duplicateError;
        }
        if (!state.ePlanName.empty() || !state.eSort.empty() || !state.eProjects.empty()) {
            PushToastMsg(world, "请检查表单必填项", ui::ToastType::Warning);
            return;
        }

        data::NursingPlanRecord record;
        record.id = state.editingId;
        record.planName = planName;
        record.status = state.status;
        record.sortNo = sortNo;
        for (const ProjectRowState& row : state.projects) {
            if (row.projectId < 0) {
                continue;
            }
            data::NursingPlanProjectRecord project;
            project.planId = state.editingId;
            project.projectId = row.projectId;
            project.executeTime = TextOf(row.executeTime);
            project.executeCycle = row.cycle;
            project.executeFrequency = std::max(1, ParseInt(TextOf(row.frequency), 1));
            for (const data::NursingProjectRecord& option : state.projectOptions) {
                if (option.id == row.projectId) {
                    project.projectName = option.name;
                    break;
                }
            }
            record.projects.push_back(project);
        }

        std::string error;
        const bool isEdit = state.editingId > 0;
        const bool ok = isEdit ? service.Update(record, &error) : service.Add(record, &error);
        if (!ok) {
            state.dialogError = error.empty() ? "保存失败，请稍后重试" : error;
            PushToastMsg(world, state.dialogError, ui::ToastType::Error);
            return;
        }
        PushToastMsg(world, isEdit ? "修改成功" : "新增成功", ui::ToastType::Success);
        state.dialog.open = false;
        state.reload = true;
    }

    // 导出当前查询结果为 CSV（可选能力，dev3 提供的 CsvExport 工具契约）
    void ExportPlans(World& world, const NursingPlanPageState& state) {
        std::vector<std::string> headers = {"序号", "名称", "状态", "创建时间"};
        std::vector<std::vector<std::string>> rows;
        for (std::size_t index = 0; index < state.allRows.size(); ++index) {
            const data::NursingPlanRecord& plan = state.allRows[index];
            rows.push_back({std::to_string(index + 1), plan.planName,
                            plan.status == 1 ? "启用" : "禁用", plan.createTime});
        }
        const utils::CsvExportResult result = utils::ExportCsv("nursing_plans", headers, rows);
        if (result.ok) {
            const std::size_t slash = result.path.find_last_of("/\\");
            const std::string fileName =
                slash == std::string::npos ? result.path : result.path.substr(slash + 1);
            PushToastMsg(world, "导出成功：" + fileName + "（共 " + std::to_string(rows.size()) +
                                    " 条）",
                         ui::ToastType::Success);
        } else {
            PushToastMsg(world, "导出失败：" + result.error, ui::ToastType::Error);
        }
    }

    bool m_visible = false;
    bool m_resetScroll = false;
    std::map<int, std::vector<std::string>> m_rowActions;
};

}  // namespace

void RegisterNursingPlanModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<NursingPlanPageState>();
    scheduler.Add(std::make_unique<NursingPlanPageSystem>(), 200);
}

}  // namespace zzyl
