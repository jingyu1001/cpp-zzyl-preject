// =============================================================================
// 服务管理-护理项目（M3）· cpp-dev3
//
// 依据：
//  - docs/tasks/Sprint2_cpp-dev3_房型设置与护理项目.md（本页字段/交互）
//  - UI复刻规格_Cpp_v0.2.md §2/§3/§4.3/§5.1/§6（12 张素材图标映射）
//  - docs/architecture/Cpp架构设计_v0.2.md §2.1（页面系统模式）、§4（WIC 纹理）、§5（M0 契约）
//
// 页面结构：搜索栏（名称/状态）→ 工具栏（新增）→ 数据表格（10 列）
// → 分页（贴底）；弹窗（500 宽单列）/ 删除确认 / Toast 在本系统末尾绘制。
//
// 渲染与数据分离：列表只经 DataServices.nursingProject 接口读取，交互后重新 List；
// 图片列与弹窗预览全部走 TextureCache（assets/icons/*.png），加载失败显示占位不崩溃。
// 所有位置/尺寸由 ShellLayout::pageViewport + Widgets 高度辅助函数推导，无绝对坐标。
// =============================================================================

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
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

// ---- 规格常量（UI v0.2 §3 / §4.3 / §6；属于布局约束，不是元素坐标）----
constexpr float kSectionGap = 12.0f;          // 搜索栏/工具栏/表格/分页之间间距
constexpr float kFormLabelWidth = 80.0f;      // 表单 label 宽（§3.5）
constexpr float kFormCellPaddingY = 14.0f;    // 表单项 32 高 + 间距 28
constexpr float kDialogWidth = 500.0f;        // 护理项目弹窗宽（§3.5）
constexpr float kImagePreviewSize = 148.0f;   // 图片预览 148×148（§3.8 / §6.3）
constexpr float kThumbSize = 32.0f;           // 素材库缩略图
constexpr float kThumbGap = 3.0f;
constexpr int kThumbColumns = 6;
// Toast 绘制归属：外壳（ShellRenderSystem）每帧统一 DrawToasts，页面只 PushToast。
constexpr bool kDrawToastsInPage = false;

// -----------------------------------------------------------------------------
// 页面私有状态（World 资源）
// -----------------------------------------------------------------------------
struct NursingProjectPageState {
    // 列表
    bool initialized = false;
    bool reloadRequested = true;
    data::NursingProjectQuery query;
    std::vector<data::NursingProjectRecord> results;
    std::vector<ui::SearchField> searchFields;
    ui::PaginationState pagination;
    std::vector<int> checkedIds;

    // 表单弹窗
    ui::DialogState dialog;
    bool editing = false;
    int editingId = 0;
    std::string editingCreateTime;
    char name[64] = {};
    char orderNoText[16] = {};
    char unit[32] = {};
    char priceText[16] = {};
    bool enabled = true;
    std::string imagePath;
    char nursingRequirement[256] = {};
    std::vector<std::string> imageLibrary;
    bool imageLibraryLoaded = false;
    std::string nameError;
    std::string orderNoError;
    std::string priceError;
    std::string submitError;

    // 删除确认框
    bool confirmOpen = false;
    std::string confirmMessage;
    int pendingRemoveId = 0;
};

// -----------------------------------------------------------------------------
// 通用小工具
// -----------------------------------------------------------------------------
void PushToast(World& world, const std::string& text, ui::ToastType type) {
    if (auto* toasts = world.TryResource<ui::ToastState>()) {
        ui::PushToast(*toasts, text, type);
    }
}

std::string NowText() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
    return std::string(buffer);
}

// 价格展示（元，两位小数）
std::string FormatPrice(double price) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", std::floor(price * 100.0 + 0.5) / 100.0);
    return std::string(buffer);
}

int QueryStatusFromText(const std::string& text) {
    if (text == "启用") {
        return 1;
    }
    if (text == "禁用") {
        return 0;
    }
    return -1;
}

const data::NursingProjectRecord* FindRecord(const std::vector<data::NursingProjectRecord>& records,
                                             int id) {
    for (const data::NursingProjectRecord& record : records) {
        if (record.id == id) {
            return &record;
        }
    }
    return nullptr;
}

// 文本数字校验（弹性解析：允许首尾空格；整段必须是合法数字）
bool ParseIntText(const char* text, int* outValue) {
    if (text == nullptr || outValue == nullptr) {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text) {
        return false;
    }
    while (*end == ' ' || *end == '\t') {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    *outValue = static_cast<int>(parsed);
    return true;
}

bool ParseDoubleText(const char* text, double* outValue) {
    if (text == nullptr || outValue == nullptr) {
        return false;
    }
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == text) {
        return false;
    }
    while (*end == ' ' || *end == '\t') {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    *outValue = parsed;
    return true;
}

// -----------------------------------------------------------------------------
// 布局：pageViewport → 搜索栏 / 工具栏 / 表格 / 分页（高度全部来自 Widgets 辅助函数）
// -----------------------------------------------------------------------------
struct NursingPageLayout {
    layout::Rect page;
    layout::Rect search;
    layout::Rect toolbar;
    layout::Rect table;
    layout::Rect pagination;
};

NursingPageLayout ComputePageLayout(const layout::ShellLayout& shell, int searchFieldCount) {
    NursingPageLayout layoutState;
    layoutState.page = shell.pageViewport;
    const float searchHeight = ui::SearchBarHeight(searchFieldCount);
    const float toolbarHeight = ui::ToolbarHeight();
    const float paginationHeight = ui::PaginationHeight();

    float top = layoutState.page.y;
    layoutState.search = layout::Rect{layoutState.page.x, top, layoutState.page.w, searchHeight};
    top += searchHeight + kSectionGap;
    layoutState.toolbar = layout::Rect{layoutState.page.x, top, layoutState.page.w, toolbarHeight};
    top += toolbarHeight + kSectionGap;

    const float paginationTop = layoutState.page.Bottom() - paginationHeight;
    layoutState.pagination =
        layout::Rect{layoutState.page.x, paginationTop, layoutState.page.w, paginationHeight};
    layoutState.table = layout::Rect{layoutState.page.x, top, layoutState.page.w,
                                     std::max(0.0f, paginationTop - kSectionGap - top)};
    return layoutState;
}

// -----------------------------------------------------------------------------
// 数据加载（只经 INursingProjectService 接口）
// -----------------------------------------------------------------------------
void ReloadRows(data::INursingProjectService& service, NursingProjectPageState& state) {
    state.results = service.List(state.query);
    state.pagination.total = static_cast<int>(state.results.size());
    if (state.pagination.pageSize <= 0) {
        state.pagination.pageSize = 10;
    }
    const int maxPage = std::max(
        1, (state.pagination.total + state.pagination.pageSize - 1) / state.pagination.pageSize);
    state.pagination.page = std::max(1, std::min(state.pagination.page, maxPage));
    state.reloadRequested = false;
}

// -----------------------------------------------------------------------------
// 表单：打开 / 回填 / 提交
// -----------------------------------------------------------------------------
void ClearFormErrors(NursingProjectPageState& state) {
    state.nameError.clear();
    state.orderNoError.clear();
    state.priceError.clear();
    state.submitError.clear();
}

void EnsureImageLibrary(data::INursingProjectService& service, NursingProjectPageState& state) {
    if (!state.imageLibraryLoaded) {
        state.imageLibrary = service.ImageLibrary();
        state.imageLibraryLoaded = true;
    }
}

void OpenAddDialog(data::INursingProjectService& service, NursingProjectPageState& state) {
    EnsureImageLibrary(service, state);
    state.editing = false;
    state.editingId = 0;
    state.editingCreateTime.clear();
    std::snprintf(state.name, sizeof(state.name), "%s", "");
    std::snprintf(state.orderNoText, sizeof(state.orderNoText), "%d",
                  static_cast<int>(state.results.size()) + 1);
    std::snprintf(state.unit, sizeof(state.unit), "%s", "次");
    std::snprintf(state.priceText, sizeof(state.priceText), "%s", "");
    state.enabled = true;
    state.imagePath = state.imageLibrary.empty() ? std::string() : state.imageLibrary.front();
    std::snprintf(state.nursingRequirement, sizeof(state.nursingRequirement), "%s", "");
    ClearFormErrors(state);
    state.dialog.title = "添加护理项目";
    state.dialog.width = kDialogWidth;
    state.dialog.open = true;
}

void OpenEditDialog(data::INursingProjectService& service, NursingProjectPageState& state,
                    const data::NursingProjectRecord& record) {
    EnsureImageLibrary(service, state);
    state.editing = true;
    state.editingId = record.id;
    state.editingCreateTime = record.createTime;
    std::snprintf(state.name, sizeof(state.name), "%s", record.name.c_str());
    std::snprintf(state.orderNoText, sizeof(state.orderNoText), "%d", record.orderNo);
    std::snprintf(state.unit, sizeof(state.unit), "%s", record.unit.c_str());
    std::snprintf(state.priceText, sizeof(state.priceText), "%.2f", record.price);
    state.enabled = record.enabled;
    state.imagePath = record.imagePath;
    if (state.imagePath.empty() && !state.imageLibrary.empty()) {
        state.imagePath = state.imageLibrary.front();
    }
    std::snprintf(state.nursingRequirement, sizeof(state.nursingRequirement), "%s",
                  record.nursingRequirement.c_str());
    ClearFormErrors(state);
    state.dialog.title = "修改护理项目";
    state.dialog.width = kDialogWidth;
    state.dialog.open = true;
}

bool SubmitForm(World& world, data::INursingProjectService& service,
                NursingProjectPageState& state) {
    ClearFormErrors(state);
    bool valid = true;
    if (state.name[0] == '\0') {
        state.nameError = "请输入名称";
        valid = false;
    }
    int orderNo = 0;
    if (state.orderNoText[0] != '\0' && !ParseIntText(state.orderNoText, &orderNo)) {
        state.orderNoError = "排序号必须为整数";
        valid = false;
    }
    double price = 0.0;
    if (state.priceText[0] == '\0') {
        state.priceError = "请输入价格";
        valid = false;
    } else if (!ParseDoubleText(state.priceText, &price) || price <= 0.0) {
        state.priceError = "价格必须大于 0";
        valid = false;
    }
    if (!valid) {
        return false;
    }

    data::NursingProjectRecord record;
    record.id = state.editing ? state.editingId : 0;
    record.name = state.name;
    record.unit = state.unit;
    record.orderNo = orderNo;
    record.price = price;
    record.enabled = state.enabled;
    record.nursingRequirement = state.nursingRequirement;
    record.imagePath = state.imagePath;
    record.createTime = state.editing ? state.editingCreateTime : NowText();

    std::string error;
    const bool ok = state.editing ? service.Update(record, &error) : service.Add(record, &error);
    if (!ok) {
        state.submitError = error.empty() ? "保存失败，请稍后重试" : error;
        return false;
    }
    PushToast(world, state.editing ? "修改成功" : "新增成功", ui::ToastType::Success);
    state.dialog.open = false;
    state.reloadRequested = true;
    return true;
}

// -----------------------------------------------------------------------------
// 行操作：修改 / 删除（确认框）/ 启停
// -----------------------------------------------------------------------------
void ApplyRemove(World& world, data::INursingProjectService& service,
                 NursingProjectPageState& state) {
    if (state.pendingRemoveId == 0) {
        return;
    }
    std::string error;
    if (service.Remove({state.pendingRemoveId}, &error)) {
        PushToast(world, "删除成功", ui::ToastType::Success);
    } else {
        PushToast(world, error.empty() ? "删除失败" : error, ui::ToastType::Error);
    }
    state.pendingRemoveId = 0;
    state.reloadRequested = true;
}

void SetStatus(World& world, data::INursingProjectService& service,
               NursingProjectPageState& state, int id, bool enabled) {
    std::string error;
    if (service.SetStatus(id, enabled, &error)) {
        PushToast(world, enabled ? "已启用" : "已禁用", ui::ToastType::Success);
    } else {
        PushToast(world, error.empty() ? "状态修改失败" : error, ui::ToastType::Error);
    }
    state.reloadRequested = true;
}

void HandleRowAction(World& world, data::INursingProjectService& service,
                     NursingProjectPageState& state, int rowId, int actionIndex) {
    const data::NursingProjectRecord* record = FindRecord(state.results, rowId);
    if (record == nullptr) {
        return;
    }
    switch (actionIndex) {
        case 0:  // 修改
            OpenEditDialog(service, state, *record);
            break;
        case 1:  // 删除
            state.confirmOpen = true;
            state.confirmMessage = "确定删除该护理项目吗？";
            state.pendingRemoveId = record->id;
            break;
        case 2:  // 禁用 / 启用
            SetStatus(world, service, state, record->id, !record->enabled);
            break;
        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// 表单内容（标题栏 / 底部确定取消由 Widgets 负责，这里只画内容区）
// -----------------------------------------------------------------------------

// 表单控件样式作用域：白底 + 边框 + 圆角 4 + 32 高（与 Widgets 输入框一致）
struct FormStyleGuard {
    FormStyleGuard() {
        const theme::Palette& colors = theme::Colors();
        const float displayHeight = ImGui::GetIO().DisplaySize.y;
        // 小窗口（高 < 700）压缩表单项间距，保证 500 宽弹窗底部按钮不超出屏幕
        const float cellPaddingY = displayHeight < 700.0f ? 8.0f : kFormCellPaddingY;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 9.0f));  // 控件高 32
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, cellPaddingY));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, colors.panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, colors.panel);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, colors.panel);
        ImGui::PushStyleColor(ImGuiCol_Border, colors.borderAxis);
        ImGui::PushStyleColor(ImGuiCol_Text, colors.textRegular);
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, colors.textPlaceholder);
    }
    ~FormStyleGuard() {
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(5);
    }
    FormStyleGuard(const FormStyleGuard&) = delete;
    FormStyleGuard& operator=(const FormStyleGuard&) = delete;
};

void DrawFormLabel(const char* text, bool required) {
    const theme::Palette& colors = theme::Colors();
    ImGui::AlignTextToFramePadding();
    const float starWidth = required ? ImGui::CalcTextSize("*").x + 4.0f : 0.0f;
    const float textWidth = ImGui::CalcTextSize(text).x + starWidth;
    const float cellPadding = ImGui::GetStyle().CellPadding.x;
    const float offset = std::max(0.0f, kFormLabelWidth - textWidth - cellPadding * 2.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    if (required) {
        ImGui::PushStyleColor(ImGuiCol_Text, colors.danger);
        ImGui::TextUnformatted("*");
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 4.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, colors.textRegular);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void DrawFieldError(const std::string& message) {
    if (message.empty()) {
        return;
    }
    const theme::Palette& colors = theme::Colors();
    ui::ScopedFontSize errorFont(12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, colors.danger);
    ImGui::TextUnformatted(message.c_str());
    ImGui::PopStyleColor();
}

// 图片选择：148×148 预览（contain）+ 右侧 12 张素材缩略图（点击切换），
// 纹理全部来自 TextureCache；加载失败显示占位文字，不崩溃。
void DrawImagePicker(World& world, data::INursingProjectService& service,
                     NursingProjectPageState& state) {
    const theme::Palette& colors = theme::Colors();
    // TextureCache::Get 需要非 const 实例（契约如此），页面只读使用其纹理
    render::TextureCache* textures = world.TryResource<render::TextureCache>();
    EnsureImageLibrary(service, state);
    if (!state.editing && state.imagePath.empty() && !state.imageLibrary.empty()) {
        state.imagePath = state.imageLibrary.front();
    }

    const int count = static_cast<int>(state.imageLibrary.size());
    const int rows = std::max(1, (count + kThumbColumns - 1) / kThumbColumns);
    const float gridWidth =
        kThumbColumns * kThumbSize + static_cast<float>(kThumbColumns - 1) * kThumbGap;
    const float gridHeight = static_cast<float>(rows) * kThumbSize
                             + static_cast<float>(std::max(0, rows - 1)) * kThumbGap;
    const float blockWidth = kImagePreviewSize + 12.0f + gridWidth;
    const float blockHeight = std::max(kImagePreviewSize, gridHeight);

    const ImVec2 blockStart = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(blockWidth, blockHeight));

    // 整块一个 InvisibleButton，点击位置换算到具体缩略图
    ImGui::SetCursorScreenPos(blockStart);
    ImGui::InvisibleButton("##nursing_image_picker", ImVec2(blockWidth, blockHeight));
    const bool clicked = ImGui::IsItemClicked();
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // ---- 预览（148×148 contain）----
    const layout::Rect previewRect{blockStart.x, blockStart.y, kImagePreviewSize, kImagePreviewSize};
    ui::FillRounded(drawList, previewRect, colors.lightBlock, 4.0f);
    if (textures != nullptr && !state.imagePath.empty()) {
        const render::Texture& texture = textures->Get(state.imagePath.c_str());
        if (texture.Valid()) {
            layout::Rect inner = previewRect;
            inner.x += 4.0f;
            inner.y += 4.0f;
            inner.w -= 8.0f;
            inner.h -= 8.0f;
            render::DrawImageContain(drawList, texture, inner);
        } else {
            ui::ScopedFontSize hintFont(12.0f);
            ui::TextCentered(drawList, previewRect, colors.textMuted, "图片加载失败");
        }
    } else {
        ui::ScopedFontSize hintFont(12.0f);
        ui::TextCentered(drawList, previewRect, colors.textMuted, "请选择图片");
    }
    ui::StrokeRounded(drawList, previewRect, colors.borderAxis, 4.0f, 1.0f);

    // ---- 素材库（12 张，6 列 × 2 行）----
    const float gridX = blockStart.x + kImagePreviewSize + 12.0f;
    const float gridY = blockStart.y;
    for (int index = 0; index < count; ++index) {
        const int column = index % kThumbColumns;
        const int row = index / kThumbColumns;
        const layout::Rect thumb{gridX + static_cast<float>(column) * (kThumbSize + kThumbGap),
                                 gridY + static_cast<float>(row) * (kThumbSize + kThumbGap),
                                 kThumbSize, kThumbSize};
        ui::FillRounded(drawList, thumb, colors.lightBlock, 2.0f);
        if (textures != nullptr) {
            const render::Texture& texture =
                textures->Get(state.imageLibrary[static_cast<std::size_t>(index)].c_str());
            if (texture.Valid()) {
                render::DrawImageCover(drawList, texture, thumb);
            }
        }
        const bool selected =
            state.imagePath == state.imageLibrary[static_cast<std::size_t>(index)];
        if (selected) {
            ui::StrokeRounded(drawList, thumb, colors.primary, 2.0f, 2.0f);
        }
        if (clicked && mouse.x >= thumb.x && mouse.x <= thumb.Right() && mouse.y >= thumb.y &&
            mouse.y <= thumb.Bottom()) {
            state.imagePath = state.imageLibrary[static_cast<std::size_t>(index)];
        }
    }
}

void DrawNursingForm(World& world, data::INursingProjectService& service,
                     NursingProjectPageState& state) {
    const FormStyleGuard styleGuard;

    if (ImGui::BeginTable("##nursing_form", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, kFormLabelWidth);
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);

        // 名称*
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("名称", true);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##nursing_name", "请输入名称", state.name, sizeof(state.name));
        DrawFieldError(state.nameError);

        // 排序号
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("排序号", false);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##nursing_order", "请输入排序号", state.orderNoText,
                                 sizeof(state.orderNoText));
        DrawFieldError(state.orderNoError);

        // 单位
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("单位", false);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##nursing_unit", "请输入单位", state.unit, sizeof(state.unit));

        // 价格*
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("价格", true);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##nursing_price", "请输入价格", state.priceText,
                                 sizeof(state.priceText));
        DrawFieldError(state.priceError);

        // 状态（单选组）
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("状态", false);
        ImGui::TableNextColumn();
        if (ImGui::RadioButton("启用", state.enabled)) {
            state.enabled = true;
        }
        ImGui::SameLine(0.0f, 16.0f);
        if (ImGui::RadioButton("禁用", !state.enabled)) {
            state.enabled = false;
        }

        // 图片（148×148 预览 + 素材库选择）
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("图片", false);
        ImGui::TableNextColumn();
        DrawImagePicker(world, service, state);

        // 护理要求（多行可单行）
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("护理要求", false);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextMultiline("##nursing_requirement", state.nursingRequirement,
                                  sizeof(state.nursingRequirement),
                                  ImVec2(ImGui::GetContentRegionAvail().x, 48.0f));

        if (!state.submitError.empty()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            DrawFieldError(state.submitError);
        }
        ImGui::EndTable();
    }
}

// -----------------------------------------------------------------------------
// 表格数据（当前页切片 → TableRow；编号列为 1 基序号）
// -----------------------------------------------------------------------------
void BuildTableRows(NursingProjectPageState& state, std::vector<ui::TableRow>& rows) {
    rows.clear();
    const int total = static_cast<int>(state.results.size());
    const int pageSize = std::max(1, state.pagination.pageSize);
    const int maxPage = std::max(1, (total + pageSize - 1) / pageSize);
    state.pagination.page = std::max(1, std::min(state.pagination.page, maxPage));
    const int start = (state.pagination.page - 1) * pageSize;
    const int end = std::min(total, start + pageSize);
    rows.reserve(static_cast<std::size_t>(std::max(0, end - start)));
    for (int index = start; index < end; ++index) {
        const data::NursingProjectRecord& record = state.results[static_cast<std::size_t>(index)];
        ui::TableRow row;
        row.id = record.id;
        row.enabled = record.enabled;
        row.cells = {
            std::to_string(index + 1),                 // 编号（1 基）
            record.name,                               // 名称
            std::to_string(record.orderNo),            // 排序号
            record.unit,                               // 单位
            FormatPrice(record.price),                 // 价格（元）
            record.imagePath,                          // 图片（TextureCache 缩略图）
            record.nursingRequirement,                 // 护理要求
            record.enabled ? "启用" : "禁用",          // 状态（StatusTag）
            record.createTime,                         // 创建时间
            record.enabled ? "修改|删除|禁用" : "修改|删除|启用",  // 操作（逐行动态标签）
        };
        rows.push_back(std::move(row));
    }
}

// -----------------------------------------------------------------------------
// 页面系统（Render 阶段；只读数据 + 交互后刷新）
// -----------------------------------------------------------------------------
class NursingProjectPageSystem final : public ISystem {
public:
    const char* Name() const override { return "NursingProjectPageSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& frame) override {
        const RouteState* route = world.TryResource<RouteState>();
        if (route == nullptr || route->current != Route::NursingProject) {
            return;
        }
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        const DataServices* services = world.TryResource<DataServices>();
        NursingProjectPageState* state = world.TryResource<NursingProjectPageState>();
        if (shell == nullptr || services == nullptr || !services->nursingProject ||
            state == nullptr) {
            return;
        }
        data::INursingProjectService& service = *services->nursingProject;

        if (!state->initialized) {
            state->initialized = true;
            state->reloadRequested = true;
            state->pagination.pageSize = 10;
        }
        // 搜索栏字段（名称 / 状态），值保存在页面 State 中跨帧保留
        if (state->searchFields.empty()) {
            ui::SearchField keyword;
            keyword.key = "keyword";
            keyword.label = "名称";
            keyword.type = ui::SearchFieldType::Text;
            keyword.controlWidth = 240.0f;
            ui::SearchField status;
            status.key = "status";
            status.label = "状态";
            status.type = ui::SearchFieldType::Select;
            status.options = {"启用", "禁用"};
            status.controlWidth = 240.0f;
            state->searchFields.push_back(std::move(keyword));
            state->searchFields.push_back(std::move(status));
        }

        if (state->reloadRequested) {
            ReloadRows(service, *state);
        }

        // 页面底色
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        ui::PageBackground(drawList, shell->content);

        const NursingPageLayout layoutState =
            ComputePageLayout(*shell, static_cast<int>(state->searchFields.size()));
        render::TextureCache* textures = world.TryResource<render::TextureCache>();

        // 页面宿主窗口：Widgets 组件依赖 ImGui 当前窗口（GetWindowDrawList / 交互），
        // 统一开一个铺满 pageViewport 的固定窗口；表格行数超出时由表格组件内部滚动。
        if (!ui::BeginFixedWindow("##nursing_project_page", layoutState.page)) {
            ui::EndFixedWindow();
            return;
        }

        // ---- ① 搜索栏 ----
        const ui::SearchBarResult search = ui::DrawSearchBar(layoutState.search, state->searchFields);
        if (search.search) {
            state->query.keyword = state->searchFields[0].value;
            state->query.status = QueryStatusFromText(state->searchFields[1].value);
            state->pagination.page = 1;
            state->reloadRequested = true;
        }
        if (search.reset) {
            for (ui::SearchField& field : state->searchFields) {
                field.value.clear();
            }
            state->query = data::NursingProjectQuery{};
            state->pagination.page = 1;
            state->reloadRequested = true;
        }
        if (state->reloadRequested) {
            ReloadRows(service, *state);
        }

        // ---- ② 工具栏：新增 ----
        std::vector<ui::ToolbarButton> toolbarButtons;
        toolbarButtons.push_back(ui::ToolbarButton{ui::ToolbarKind::Add, "新增", true});
        const ui::ToolbarResult toolbar = ui::DrawToolbar(layoutState.toolbar, toolbarButtons);
        if (toolbar.clicked == 0) {
            OpenAddDialog(service, *state);
        }

        // ---- ③ 数据表格（10 列，图片列用 12 张素材）----
        std::vector<ui::TableColumn> columns;
        columns.push_back(
            ui::TableColumn{"index", "编号", 120.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"name", "名称", 150.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"orderNo", "排序号", 100.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"unit", "单位", 100.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"price", "价格", 150.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"image", "图片", 150.0f, ui::Align::Center, ui::CellKind::Image});
        columns.push_back(ui::TableColumn{"requirement", "护理要求", 180.0f, ui::Align::Left,
                                          ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"status", "状态", 120.0f, ui::Align::Center, ui::CellKind::StatusTag});
        columns.push_back(ui::TableColumn{"createTime", "创建时间", 180.0f, ui::Align::Center,
                                          ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"actions", "操作", 200.0f, ui::Align::Center, ui::CellKind::Actions});

        std::vector<ui::TableRow> rows;
        BuildTableRows(*state, rows);

        ui::TableDrawOptions options;
        options.showCheckbox = false;
        options.showIndex = false;
        options.actionLabels = {"修改", "删除", "启用/禁用"};

        const ui::TableResult table = ui::DrawDataTable(layoutState.table, columns, rows,
                                                        state->checkedIds, textures, options);

        for (const ui::TableResult::SwitchToggle& toggle : table.switchToggles) {
            SetStatus(world, service, *state, toggle.rowId, toggle.enabled);
        }
        if (table.clickedAction >= 0 && table.actionRowId > 0) {
            HandleRowAction(world, service, *state, table.actionRowId, table.clickedAction);
        }

        // ---- ④ 分页（贴底）----
        ui::DrawPagination(layoutState.pagination, state->pagination);

        // ---- ⑤ 表单弹窗 ----
        if (state->dialog.open) {
            if (ui::BeginFormDialog(layoutState.page, state->dialog)) {
                DrawNursingForm(world, service, *state);
            }
            const ui::DialogResult dialogResult = ui::EndFormDialog();
            if (dialogResult == ui::DialogResult::Ok) {
                SubmitForm(world, service, *state);
            }
        }

        // ---- ⑥ 删除确认框 ----
        if (state->confirmOpen) {
            const ui::ConfirmAction action = ui::DrawConfirmDialog(
                layoutState.page, state->confirmOpen, "系统提示", state->confirmMessage);
            if (action == ui::ConfirmAction::Ok) {
                ApplyRemove(world, service, *state);
                state->confirmOpen = false;
            }
        }

        // ---- ⑦ Toast：页面只 PushToast，由外壳每帧统一 DrawToasts（lead 确认口径）----
        if (kDrawToastsInPage) {
            if (auto* toasts = world.TryResource<ui::ToastState>()) {
                ui::DrawToasts(shell->viewport, *toasts, frame.deltaTime);
            }
        }

        ui::EndFixedWindow();
    }
};

}  // namespace

void RegisterNursingProjectModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<NursingProjectPageState>();
    scheduler.Add(std::make_unique<NursingProjectPageSystem>(), 200);
}

}  // namespace zzyl
