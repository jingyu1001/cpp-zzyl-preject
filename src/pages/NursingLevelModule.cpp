// =============================================================================
// M6 服务管理-护理等级 · cpp-dev2（Sprint 4）
//
// 依据：
//  - Web《Sprint4_功能规格_v0.1.md》§2（列表 8 列：序号/护理等级名称/执行护理计划/护理费用(元/月)/
//    状态/等级说明/创建时间/操作；新增/修改弹窗 500×432、label-width 80：名称*/护理计划*/费用*/状态*/说明*）
//  - 架构 v0.4 §4 / 任务单 Sprint4 §2；种子 3 条（76/77/78 → 1500/2000/3000，plan 133/134/135）
//
// 数据流（渲染与数据分离）：
//  - 列表：DataServices.nursingLevel->List(query)；护理计划下拉 = nursingPlan->List()；
//  - 保存 Add/Update；删除确认 + SetStatus + Toast + 刷新；布局由 ShellLayout::pageViewport 推导。
// =============================================================================

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
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

constexpr float kPagePadding = 16.0f;
constexpr float kLabelWidth = 80.0f;       // 弹窗 label-width: 80
constexpr float kControlHeight = 32.0f;
constexpr float kFormRowGap = 16.0f;
constexpr int kPageSize = 10;
constexpr float kDialogWidth = 500.0f;     // 弹窗宽（高由内容自适应 ≈432）

// -----------------------------------------------------------------------------
// 页面私有状态
// -----------------------------------------------------------------------------
struct NursingLevelPageState {
    // 搜索
    char keyword[64] = {};
    int statusFilterIndex = 0;  // 0 全部 / 1 启用 / 2 禁用
    // 列表
    std::vector<data::NursingLevelRecord> allRows;
    int page = 1;
    bool reload = true;
    // 弹窗
    ui::DialogState dialog;
    int editingId = 0;
    char name[64] = {};
    int planIndex = -1;
    char price[16] = {};
    int status = 1;
    char description[128] = {};
    std::string dialogError;  // 服务层错误（保存失败等统一提示）
    // 逐项校验错误（就地红字 + 红框）
    std::string eName;
    std::string ePlan;
    std::string ePrice;
    std::string eDescription;
    // 护理计划下拉
    std::vector<data::NursingPlanRecord> planOptions;
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

double ParseDouble(const std::string& text, bool* ok) {
    if (ok != nullptr) {
        *ok = false;
    }
    if (text.empty()) {
        return 0.0;
    }
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == nullptr || *end != '\0') {
        return 0.0;
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return value;
}

std::string FormatPrice(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return std::string(buffer);
}

std::string DateOnly(const std::string& createTime) {
    return createTime.size() >= 10 ? createTime.substr(0, 10) : createTime;
}

data::INursingLevelService* LevelService(World& world) {
    DataServices* services = world.TryResource<DataServices>();
    return (services != nullptr && services->nursingLevel) ? services->nursingLevel.get() : nullptr;
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

// 弹窗内 label（宽 80，右对齐）+ 控件；返回本帧是否编辑
bool DrawFormRow(const char* label, bool required, const char* id, char* buffer, size_t size,
                 float controlWidth, const char* hint, const std::string& error,
                 ImGuiInputTextFlags flags = 0) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const char* star = required ? "*" : "";
    const float starWidth = ImGui::CalcTextSize(star).x;
    const float labelLen = ImGui::CalcTextSize(label).x;
    ImGui::SetCursorScreenPos(
        ImVec2(origin.x + kLabelWidth - starWidth - labelLen,
               origin.y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f));
    if (required) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "*");
        ImGui::SameLine(0.0f, 0.0f);
    }
    ImGui::TextUnformatted(label);
    const float controlX = origin.x + kLabelWidth + 8.0f;
    PushInputStyle(!error.empty());
    ImGui::SetCursorScreenPos(ImVec2(controlX, origin.y));
    ImGui::SetNextItemWidth(controlWidth);
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
    if (!error.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(controlX, origin.y + kControlHeight + 2.0f));
        {
            ui::ScopedFontSize font(12.0f);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                               error.c_str());
        }
        ImGui::SetCursorScreenPos(
            ImVec2(origin.x, origin.y + kControlHeight + kFormRowGap + 16.0f));
    } else {
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + kControlHeight + kFormRowGap));
    }
    return edited;
}

// -----------------------------------------------------------------------------
// 页面系统
// -----------------------------------------------------------------------------
class NursingLevelPageSystem final : public ISystem {
public:
    const char* Name() const override { return "NursingLevelPageSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& frame) override {
        const RouteState* route = world.TryResource<RouteState>();
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        if (route == nullptr || shell == nullptr || route->current != Route::NursingLevel) {
            m_visible = false;
            return;
        }
        NursingLevelPageState* state = world.TryResource<NursingLevelPageState>();
        data::INursingLevelService* service = LevelService(world);
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
    void ReloadList(World& world, NursingLevelPageState& state, data::INursingLevelService& service) {
        data::NursingLevelQuery query;
        query.keyword = TextOf(state.keyword);
        query.status = state.statusFilterIndex == 1 ? 1 : (state.statusFilterIndex == 2 ? 0 : -1);
        state.allRows = service.List(query);
        state.reload = false;
        if (state.page < 1) {
            state.page = 1;
        }
        (void)world;
    }

    void EnsurePlanOptions(World& world, NursingLevelPageState& state) {
        if (state.optionsLoaded) {
            return;
        }
        state.planOptions.clear();
        if (DataServices* services = world.TryResource<DataServices>()) {
            if (services->nursingPlan) {
                state.planOptions = services->nursingPlan->List(data::NursingPlanQuery{});
            }
        }
        state.optionsLoaded = true;
    }

    void DrawPage(World& world, const layout::ShellLayout& shell, NursingLevelPageState& state,
                  data::INursingLevelService& service) {
        const layout::Rect pageViewport = shell.pageViewport;
        ImGui::SetNextWindowPos(ui::Min(pageViewport));
        ImGui::SetNextWindowSize(ImVec2(pageViewport.w, pageViewport.h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;
        const bool open = ImGui::Begin("##nursing_level_page", nullptr, flags);
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
            keywordField.label = "等级名称";
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
            statusField.controlWidth = 240.0f;
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
                ExportLevels(world, state);
            }
            cursorY += ui::ToolbarHeight() + 8.0f;

            // ---- 表格 + 分页 ----
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

    void DrawTable(World& world, NursingLevelPageState& state,
                   data::INursingLevelService& service, const layout::Rect& area) {
        const auto pageBegin = static_cast<std::size_t>((state.page - 1) * kPageSize);
        const auto pageEnd =
            std::min(state.allRows.size(), pageBegin + static_cast<std::size_t>(kPageSize));

        std::vector<ui::TableColumn> columns;
        const auto addColumn = [&columns](const char* key, const char* title, float width,
                                          ui::CellKind kind, ui::Align align) {
            ui::TableColumn column;
            column.key = key;
            column.title = title;
            column.width = width;
            column.kind = kind;
            column.align = align;
            columns.push_back(column);
        };
        addColumn("name", "护理等级名称", 120.0f, ui::CellKind::Text, ui::Align::Center);
        addColumn("planName", "执行护理计划", 180.0f, ui::CellKind::Text, ui::Align::Center);
        addColumn("price", "护理费用(元/月)", 110.0f, ui::CellKind::Text, ui::Align::Center);
        addColumn("status", "状态", 90.0f, ui::CellKind::StatusTag, ui::Align::Center);
        addColumn("description", "等级说明", 160.0f, ui::CellKind::Text, ui::Align::Center);
        addColumn("createTime", "创建时间", 110.0f, ui::CellKind::Text, ui::Align::Center);
        addColumn("actions", "操作", 170.0f, ui::CellKind::Actions, ui::Align::Center);

        std::vector<ui::TableRow> rows;
        for (std::size_t index = pageBegin; index < pageEnd; ++index) {
            const data::NursingLevelRecord& level = state.allRows[index];
            ui::TableRow row;
            row.id = level.id;
            row.cells.push_back(level.name);
            row.cells.push_back(level.planName);
            row.cells.push_back(FormatPrice(level.price));
            row.cells.push_back(level.status == 1 ? "启用" : "禁用");
            row.cells.push_back(level.description);
            row.cells.push_back(DateOnly(level.createTime));
            row.cells.push_back("修改|删除|" + std::string(level.status == 1 ? "禁用" : "启用"));
            row.enabled = level.status == 1;
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
            HandleRowAction(world, state, service, result.actionRowId, result.clickedAction);
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

    void HandleRowAction(World& world, NursingLevelPageState& state,
                         data::INursingLevelService& service, int levelId, int actionIndex) {
        const auto found = std::find_if(state.allRows.begin(), state.allRows.end(),
                                        [levelId](const data::NursingLevelRecord& level) {
                                            return level.id == levelId;
                                        });
        if (found == state.allRows.end()) {
            return;
        }
        const data::NursingLevelRecord& level = *found;
        if (actionIndex == 0) {
            OpenEditDialog(state, level);
        } else if (actionIndex == 1) {
            state.confirmOpen = true;
            state.confirmKind = 0;
            state.confirmId = level.id;
            state.confirmTitle = "删除护理等级";
            state.confirmMessage = "是否确认删除这条护理等级？";
        } else if (actionIndex == 2) {
            const bool enable = level.status != 1;
            state.confirmOpen = true;
            state.confirmKind = 1;
            state.confirmId = level.id;
            state.confirmTargetStatus = enable ? 1 : 0;
            state.confirmTitle = enable ? "启用护理等级" : "禁用护理等级";
            state.confirmMessage = std::string("是否确认") + (enable ? "启用" : "禁用") +
                                   "该护理等级？";
        }
        (void)world;
    }

    void ApplyConfirm(World& world, NursingLevelPageState& state,
                      data::INursingLevelService& service) {
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
    void OpenCreateDialog(NursingLevelPageState& state) {
        state.dialog.open = true;
        state.dialog.width = kDialogWidth;
        state.dialog.title = "添加护理等级";
        state.editingId = 0;
        state.name[0] = '\0';
        state.planIndex = -1;
        state.price[0] = '\0';
        state.status = 1;
        state.description[0] = '\0';
        state.dialogError.clear();
        state.eName.clear();
        state.ePlan.clear();
        state.ePrice.clear();
        state.eDescription.clear();
    }

    void OpenEditDialog(NursingLevelPageState& state, const data::NursingLevelRecord& level) {
        state.dialog.open = true;
        state.dialog.width = kDialogWidth;
        state.dialog.title = "修改护理等级";
        state.editingId = level.id;
        std::snprintf(state.name, sizeof(state.name), "%s", level.name.c_str());
        std::snprintf(state.price, sizeof(state.price), "%g", level.price);
        state.status = level.status;
        std::snprintf(state.description, sizeof(state.description), "%s", level.description.c_str());
        state.planIndex = -1;
        for (std::size_t index = 0; index < state.planOptions.size(); ++index) {
            if (state.planOptions[index].id == level.planId) {
                state.planIndex = static_cast<int>(index);
                break;
            }
        }
        state.dialogError.clear();
        state.eName.clear();
        state.ePlan.clear();
        state.ePrice.clear();
        state.eDescription.clear();
    }

    void DrawDialog(World& world, NursingLevelPageState& state,
                    data::INursingLevelService& service) {
        EnsurePlanOptions(world, state);
        if (!ui::BeginFormDialog(layout::Rect{}, state.dialog)) {
            (void)ui::EndFormDialog();
            return;
        }
        const float contentWidth = ImGui::GetContentRegionAvail().x;
        const float controlWidth = contentWidth - kLabelWidth - 8.0f;
        const float controlX = ImGui::GetCursorScreenPos().x + kLabelWidth + 8.0f;
        // 服务层错误（保存失败等）统一提示；字段校验错误就地显示在字段下方
        if (!state.dialogError.empty()) {
            ui::ScopedFontSize font(13.0f);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                               state.dialogError.c_str());
        }

        const bool nameEdited = DrawFormRow("等级名称", true, "##level_name", state.name,
                                            sizeof(state.name), controlWidth, "请输入等级名称",
                                            state.eName);
        if (nameEdited) {
            state.eName.clear();
        }

        // 护理计划下拉（必填，含就地红字/红框）
        {
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const char* label = "护理计划";
            const float labelLen = ImGui::CalcTextSize(label).x;
            const float starWidth = ImGui::CalcTextSize("*").x;
            ImGui::SetCursorScreenPos(
                ImVec2(origin.x + kLabelWidth - starWidth - labelLen,
                       origin.y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "*");
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(label);
            PushInputStyle(!state.ePlan.empty());
            ImGui::SetCursorScreenPos(ImVec2(origin.x + kLabelWidth + 8.0f, origin.y));
            ImGui::SetNextItemWidth(controlWidth);
            {
                ui::ScopedFontSize font(14.0f);
                const bool hasSelection =
                    state.planIndex >= 0 &&
                    state.planIndex < static_cast<int>(state.planOptions.size());
                const std::string preview =
                    hasSelection
                        ? state.planOptions[static_cast<std::size_t>(state.planIndex)].planName
                        : "请选择";
                if (ImGui::BeginCombo("##level_plan", preview.c_str())) {
                    for (int index = 0; index < static_cast<int>(state.planOptions.size()); ++index) {
                        if (ImGui::Selectable(
                                state.planOptions[static_cast<std::size_t>(index)].planName.c_str(),
                                state.planIndex == index)) {
                            state.planIndex = index;
                            state.ePlan.clear();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            PopInputStyle();
            if (!state.ePlan.empty()) {
                ImGui::SetCursorScreenPos(
                    ImVec2(origin.x + kLabelWidth + 8.0f, origin.y + kControlHeight + 2.0f));
                ui::ScopedFontSize font(12.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                                   state.ePlan.c_str());
                ImGui::SetCursorScreenPos(
                    ImVec2(origin.x, origin.y + kControlHeight + kFormRowGap + 16.0f));
            } else {
                ImGui::SetCursorScreenPos(
                    ImVec2(origin.x, origin.y + kControlHeight + kFormRowGap));
            }
        }

        const bool priceEdited = DrawFormRow("护理费用", true, "##level_price", state.price,
                                             sizeof(state.price), controlWidth, "请输入护理费用",
                                             state.ePrice, ImGuiInputTextFlags_CharsDecimal);
        if (priceEdited) {
            state.ePrice.clear();
        }

        // 状态（单选，始终有值，无需校验）
        {
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const char* label = "状态";
            const float labelLen = ImGui::CalcTextSize(label).x;
            const float starWidth = ImGui::CalcTextSize("*").x;
            ImGui::SetCursorScreenPos(
                ImVec2(origin.x + kLabelWidth - starWidth - labelLen,
                       origin.y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "*");
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(label);
            ImGui::SetCursorScreenPos(
                ImVec2(origin.x + kLabelWidth + 8.0f,
                       origin.y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f));
            {
                ui::ScopedFontSize font(14.0f);
                ImGui::RadioButton("启用##level_on", &state.status, 1);
                ImGui::SameLine(0.0f, 24.0f);
                ImGui::RadioButton("禁用##level_off", &state.status, 0);
            }
            ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + kControlHeight + kFormRowGap));
        }

        // 等级说明（多行，必填，含就地红字/红框）
        {
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const char* label = "等级说明";
            const float labelLen = ImGui::CalcTextSize(label).x;
            const float starWidth = ImGui::CalcTextSize("*").x;
            ImGui::SetCursorScreenPos(
                ImVec2(origin.x + kLabelWidth - starWidth - labelLen,
                       origin.y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "*");
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(label);
            PushInputStyle(!state.eDescription.empty());
            ImGui::SetCursorScreenPos(ImVec2(origin.x + kLabelWidth + 8.0f, origin.y));
            ImGui::SetNextItemWidth(controlWidth);
            bool descEdited = false;
            {
                ui::ScopedFontSize font(14.0f);
                descEdited = ImGui::InputTextMultiline("##level_desc", state.description,
                                                       sizeof(state.description),
                                                       ImVec2(controlWidth, 64.0f));
            }
            PopInputStyle();
            if (descEdited) {
                state.eDescription.clear();
            }
            if (!state.eDescription.empty()) {
                ImGui::SetCursorScreenPos(
                    ImVec2(origin.x + kLabelWidth + 8.0f, origin.y + 64.0f + 2.0f));
                ui::ScopedFontSize font(12.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                                   state.eDescription.c_str());
                ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 64.0f + kFormRowGap + 16.0f));
            } else {
                ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 64.0f + kFormRowGap));
            }
        }
        ImGui::Dummy(ImVec2(1.0f, 1.0f));

        const ui::DialogResult result = ui::EndFormDialog();
        if (result == ui::DialogResult::Ok) {
            SubmitDialog(world, state, service);
        } else if (result == ui::DialogResult::Cancel) {
            state.dialog.open = false;
        }
        (void)controlX;
    }

    void SubmitDialog(World& world, NursingLevelPageState& state,
                      data::INursingLevelService& service) {
        state.eName.clear();
        state.ePlan.clear();
        state.ePrice.clear();
        state.eDescription.clear();
        state.dialogError.clear();

        const std::string name = TextOf(state.name);
        if (name.empty()) {
            state.eName = "等级名称不能为空";
        }
        if (state.planIndex < 0 || state.planIndex >= static_cast<int>(state.planOptions.size())) {
            state.ePlan = "请选择护理计划（护理计划ID不能为空）";
        }
        bool ok = false;
        const double price = ParseDouble(TextOf(state.price), &ok);
        if (!ok) {
            state.ePrice = "护理费用不能为空";
        } else if (price <= 0.0) {
            state.ePrice = "护理费用必须大于 0";
        }
        const std::string description = TextOf(state.description);
        if (description.empty()) {
            state.eDescription = "等级说明不能为空";
        }
        if (!state.eName.empty() || !state.ePlan.empty() || !state.ePrice.empty() ||
            !state.eDescription.empty()) {
            PushToastMsg(world, "请检查表单必填项", ui::ToastType::Warning);
            return;
        }

        data::NursingLevelRecord record;
        record.id = state.editingId;
        record.name = name;
        record.price = price;
        record.planId = state.planOptions[static_cast<std::size_t>(state.planIndex)].id;
        record.planName = state.planOptions[static_cast<std::size_t>(state.planIndex)].planName;
        record.status = state.status;
        record.description = description;

        std::string error;
        const bool isEdit = state.editingId > 0;
        const bool success = isEdit ? service.Update(record, &error) : service.Add(record, &error);
        if (!success) {
            state.dialogError = error.empty() ? "保存失败，请稍后重试" : error;
            PushToastMsg(world, state.dialogError, ui::ToastType::Error);
            return;
        }
        PushToastMsg(world, isEdit ? "修改成功" : "新增成功", ui::ToastType::Success);
        state.dialog.open = false;
        state.reload = true;
    }
    // 导出当前查询结果为 CSV（可选能力，dev3 提供的 CsvExport 工具契约）
    void ExportLevels(World& world, const NursingLevelPageState& state) {
        std::vector<std::string> headers = {"序号",    "等级名称", "执行护理计划",
                                            "护理费用", "状态",    "等级说明",
                                            "创建时间"};
        std::vector<std::vector<std::string>> rows;
        for (std::size_t index = 0; index < state.allRows.size(); ++index) {
            const data::NursingLevelRecord& level = state.allRows[index];
            rows.push_back({std::to_string(index + 1), level.name, level.planName,
                            FormatPrice(level.price), level.status == 1 ? "启用" : "禁用",
                            level.description, DateOnly(level.createTime)});
        }
        const utils::CsvExportResult result = utils::ExportCsv("nursing_levels", headers, rows);
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
};

}  // namespace

void RegisterNursingLevelModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<NursingLevelPageState>();
    scheduler.Add(std::make_unique<NursingLevelPageSystem>(), 200);
}

}  // namespace zzyl
