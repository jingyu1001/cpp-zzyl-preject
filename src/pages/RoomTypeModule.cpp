// =============================================================================
// 在住管理-房型设置（M2）· cpp-dev3
//
// 依据：
//  - docs/tasks/Sprint2_cpp-dev3_房型设置与护理项目.md（本页字段/交互）
//  - UI复刻规格_Cpp_v0.2.md §2/§3/§4.2/§5.2/§10（D-20 无搜索栏、D-23 图片占位）
//  - docs/architecture/Cpp架构设计_v0.2.md §2.1（页面系统模式）、§5（M0 契约）
//
// 页面结构（pageViewport 内，由上到下）：工具栏（新增）→ 数据表格（7 列 + 序号列）
// → 分页（贴底）；弹窗（500 宽单列）/ 确认框 / Toast 在本系统末尾绘制。
//
// 渲染与数据分离：列表只经 DataServices.roomType 接口读取，交互后重新 List 刷新；
// 图片列走 TextureCache（本页种子数据无图片路径 → 组件按契约显示占位，D-23）。
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
#include "zzyl/platform/FileDialog.h"
#include "zzyl/render/Texture.h"
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"
#include "zzyl/ui/Widgets.h"
#include "zzyl/utils/CsvExport.h"

namespace zzyl {
namespace {

// ---- 规格常量（UI v0.2 §3 / §4.2；属于布局约束，不是元素坐标）----
constexpr float kSectionGap = 12.0f;      // 工具栏/表格/分页之间的间距（§3.2：约 12）
constexpr float kFormLabelWidth = 80.0f;  // 表单 label 宽（§3.5：单列 label 80 右对齐）
constexpr float kFormCellPaddingY = 14.0f;  // 表单项 32 高 + 间距 28 → 上下各 14
constexpr float kDialogWidth = 500.0f;    // 房型弹窗宽（§3.5）
constexpr float kPhotoBoxSize = 148.0f;   // 照片选择框 148×148（§3.8 / §4.2）

// 房型图片素材库（assets/roomtypes/ 5 张，与 SQLite 种子 photo_path 对应）
const char* const kRoomTypePhotoLibrary[] = {
    "assets/roomtypes/单人套房.png",   "assets/roomtypes/双人套房.png",
    "assets/roomtypes/豪华单人间.png", "assets/roomtypes/豪华双人间.png",
    "assets/roomtypes/普通单人间.png",
};
constexpr int kRoomTypePhotoCount =
    static_cast<int>(sizeof(kRoomTypePhotoLibrary) / sizeof(kRoomTypePhotoLibrary[0]));
// Toast 绘制归属：外壳（ShellRenderSystem）每帧统一 DrawToasts，页面只 PushToast，
// 避免同一提示被绘制两遍（lead 2026-11-24 确认）。
constexpr bool kDrawToastsInPage = false;

// -----------------------------------------------------------------------------
// 页面私有状态（World 资源；页面自己读写，不进 ECS 组件）
// -----------------------------------------------------------------------------
struct RoomTypePageState {
    // 列表
    bool initialized = false;
    bool reloadRequested = true;
    data::RoomTypeQuery query;
    std::vector<data::RoomTypeRecord> results;
    ui::PaginationState pagination;
    std::vector<int> checkedIds;

    // 表单弹窗
    ui::DialogState dialog;
    bool editing = false;
    int editingId = 0;
    bool editingEnabled = true;
    std::string editingCreateTime;
    std::string photoPath;  // 当前选择的照片相对路径（空 = 无图；素材库挑选）
    char name[64] = {};
    char bedCountText[16] = {};
    char priceText[16] = {};
    char introduction[256] = {};
    char remark[128] = {};
    std::string nameError;
    std::string bedError;
    std::string priceError;
    std::string submitError;

    // 确认框（删除）
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

// 床位费用展示：Web 列表显示整数/最多两位小数（4000 / 1500）
std::string FormatPrice(double price) {
    char buffer[32];
    const double rounded = std::floor(price * 100.0 + 0.5) / 100.0;
    if (std::fabs(rounded - std::floor(rounded)) < 0.005) {
        std::snprintf(buffer, sizeof(buffer), "%.0f", rounded);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.2f", rounded);
    }
    return std::string(buffer);
}

const data::RoomTypeRecord* FindRecord(const std::vector<data::RoomTypeRecord>& records, int id) {
    for (const data::RoomTypeRecord& record : records) {
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
// 布局：pageViewport → 工具栏 / 表格 / 分页（全部由 Widgets 高度辅助函数推导）
// -----------------------------------------------------------------------------
struct RoomTypePageLayout {
    layout::Rect page;
    layout::Rect toolbar;
    layout::Rect table;
    layout::Rect pagination;
};

RoomTypePageLayout ComputePageLayout(const layout::ShellLayout& shell) {
    RoomTypePageLayout layoutState;
    layoutState.page = shell.pageViewport;
    const float toolbarHeight = ui::ToolbarHeight();
    const float paginationHeight = ui::PaginationHeight();
    layoutState.toolbar = layout::Rect{layoutState.page.x, layoutState.page.y,
                                       layoutState.page.w, toolbarHeight};
    const float tableTop = layoutState.toolbar.Bottom() + kSectionGap;
    const float paginationTop = layoutState.page.Bottom() - paginationHeight;
    layoutState.pagination =
        layout::Rect{layoutState.page.x, paginationTop, layoutState.page.w, paginationHeight};
    layoutState.table =
        layout::Rect{layoutState.page.x, tableTop, layoutState.page.w,
                     std::max(0.0f, paginationTop - kSectionGap - tableTop)};
    return layoutState;
}

// -----------------------------------------------------------------------------
// 数据加载（只经 IRoomTypeService 接口）
// -----------------------------------------------------------------------------
void ReloadRows(data::IRoomTypeService& service, RoomTypePageState& state) {
    state.results = service.List(state.query);
    state.pagination.total = static_cast<int>(state.results.size());
    if (state.pagination.pageSize <= 0) {
        state.pagination.pageSize = 10;
    }
    const int maxPage =
        std::max(1, (state.pagination.total + state.pagination.pageSize - 1) / state.pagination.pageSize);
    state.pagination.page = std::max(1, std::min(state.pagination.page, maxPage));
    state.reloadRequested = false;
}

// -----------------------------------------------------------------------------
// 表单：打开 / 回填 / 提交
// -----------------------------------------------------------------------------
void ClearFormErrors(RoomTypePageState& state) {
    state.nameError.clear();
    state.bedError.clear();
    state.priceError.clear();
    state.submitError.clear();
}

void OpenAddDialog(RoomTypePageState& state) {
    state.editing = false;
    state.editingId = 0;
    state.editingEnabled = true;
    state.editingCreateTime.clear();
    state.photoPath.clear();
    std::snprintf(state.name, sizeof(state.name), "%s", "");
    std::snprintf(state.bedCountText, sizeof(state.bedCountText), "%s", "1");
    std::snprintf(state.priceText, sizeof(state.priceText), "%s", "");
    std::snprintf(state.introduction, sizeof(state.introduction), "%s", "");
    std::snprintf(state.remark, sizeof(state.remark), "%s", "");
    ClearFormErrors(state);
    state.dialog.title = "添加房型";
    state.dialog.width = kDialogWidth;
    state.dialog.open = true;
}

void OpenEditDialog(RoomTypePageState& state, const data::RoomTypeRecord& record) {
    state.editing = true;
    state.editingId = record.id;
    state.editingEnabled = record.enabled;
    state.editingCreateTime = record.createTime;
    state.photoPath = record.photoPath;
    std::snprintf(state.name, sizeof(state.name), "%s", record.name.c_str());
    std::snprintf(state.bedCountText, sizeof(state.bedCountText), "%d", record.bedCount);
    std::snprintf(state.priceText, sizeof(state.priceText), "%.2f", record.price);
    std::snprintf(state.introduction, sizeof(state.introduction), "%s", record.introduction.c_str());
    std::snprintf(state.remark, sizeof(state.remark), "%s", record.remark.c_str());
    ClearFormErrors(state);
    state.dialog.title = "修改房型";
    state.dialog.width = kDialogWidth;
    state.dialog.open = true;
}

// 校验 + 提交（返回 true 表示已成功保存并关闭弹窗）
bool SubmitForm(World& world, data::IRoomTypeService& service, RoomTypePageState& state) {
    ClearFormErrors(state);
    bool valid = true;
    if (state.name[0] == '\0') {
        state.nameError = "请输入房型名称";
        valid = false;
    }
    int bedCount = 0;
    if (state.bedCountText[0] == '\0') {
        state.bedError = "请输入床位数量";
        valid = false;
    } else if (!ParseIntText(state.bedCountText, &bedCount) || bedCount < 0) {
        state.bedError = "床位数量必须为整数";
        valid = false;
    }
    double price = 0.0;
    if (state.priceText[0] == '\0') {
        state.priceError = "请输入床位费用";
        valid = false;
    } else if (!ParseDoubleText(state.priceText, &price) || price <= 0.0) {
        state.priceError = "床位费用必须大于 0";
        valid = false;
    }
    if (!valid) {
        return false;
    }

    data::RoomTypeRecord record;
    record.id = state.editing ? state.editingId : 0;
    record.name = state.name;
    record.bedCount = bedCount;
    record.price = price;
    record.introduction = state.introduction;
    record.photoPath = state.photoPath;  // 新增/编辑保留所选图片（素材库选择，空=无图）
    record.remark = state.remark;
    record.enabled = state.editing ? state.editingEnabled : true;
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
void ApplyRemove(World& world, data::IRoomTypeService& service, RoomTypePageState& state) {
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

void SetStatus(World& world, data::IRoomTypeService& service, RoomTypePageState& state, int id,
               bool enabled) {
    std::string error;
    if (service.SetStatus(id, enabled, &error)) {
        PushToast(world, enabled ? "已启用" : "已禁用", ui::ToastType::Success);
    } else {
        PushToast(world, error.empty() ? "状态修改失败" : error, ui::ToastType::Error);
    }
    state.reloadRequested = true;
}

void HandleRowAction(World& world, data::IRoomTypeService& service, RoomTypePageState& state,
                     int rowId, int actionIndex) {
    const data::RoomTypeRecord* record = FindRecord(state.results, rowId);
    if (record == nullptr) {
        return;
    }
    switch (actionIndex) {
        case 0:  // 修改
            OpenEditDialog(state, *record);
            break;
        case 1:  // 删除
            state.confirmOpen = true;
            state.confirmMessage =
                "是否确认删除房型名称为\"" + record->name + "\"的数据项？";
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

// 表单控件样式作用域：白底 + 边框 + 圆角 4 + 32 高（与 Widgets 输入框一致，
// 避免主题默认"白底无边"在白色弹窗里看不见控件）
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

// 虚线边框（弹窗照片预览框用）
void DrawDashedFrame(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, float thickness,
                     float dash, float gap) {
    for (float x = rect.x; x < rect.Right(); x += dash + gap) {
        const float end = std::min(x + dash, rect.Right());
        drawList->AddLine(ImVec2(x, rect.y), ImVec2(end, rect.y), color, thickness);
        drawList->AddLine(ImVec2(x, rect.Bottom()), ImVec2(end, rect.Bottom()), color, thickness);
    }
    for (float y = rect.y; y < rect.Bottom(); y += dash + gap) {
        const float end = std::min(y + dash, rect.Bottom());
        drawList->AddLine(ImVec2(rect.x, y), ImVec2(rect.x, end), color, thickness);
        drawList->AddLine(ImVec2(rect.Right(), y), ImVec2(rect.Right(), end), color, thickness);
    }
}

// 照片选择：148×148 预览（虚线框 / 底 #FAFAFA / contain）+ 素材库 5 张 + 「无图」选项，
// 纹理走 TextureCache（WIC）；加载失败回退占位，不崩溃。
void DrawPhotoPicker(World& world, RoomTypePageState& state) {
    const theme::Palette& colors = theme::Colors();
    render::TextureCache* textures = world.TryResource<render::TextureCache>();
    constexpr float previewSize = kPhotoBoxSize;
    constexpr float thumbSize = 44.0f;
    constexpr float thumbGap = 4.0f;
    constexpr int columns = 3;
    constexpr int itemCount = kRoomTypePhotoCount + 1;  // 5 张素材 + 无图
    constexpr int rows = (itemCount + columns - 1) / columns;
    const float gridWidth =
        static_cast<float>(columns) * thumbSize + static_cast<float>(columns - 1) * thumbGap;
    const float gridHeight =
        static_cast<float>(rows) * thumbSize + static_cast<float>(rows - 1) * thumbGap;
    const float labelHeight = 16.0f;
    const float blockWidth = previewSize + 12.0f + gridWidth;
    const float blockHeight = std::max(previewSize, labelHeight + gridHeight);

    const ImVec2 blockStart = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(blockWidth, blockHeight));
    ImGui::SetCursorScreenPos(blockStart);
    ImGui::InvisibleButton("##room_photo_picker", ImVec2(blockWidth, blockHeight));
    const bool clicked = ImGui::IsItemClicked();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // ---- 预览（148×148：虚线框 + #FAFAFA 底 + contain）----
    const layout::Rect previewRect{blockStart.x, blockStart.y, previewSize, previewSize};
    ui::FillRounded(drawList, previewRect, IM_COL32(0xFA, 0xFA, 0xFA, 0xFF), 4.0f);
    DrawDashedFrame(drawList, previewRect, colors.borderAxis, 1.0f, 5.0f, 4.0f);
    if (!state.photoPath.empty()) {
        bool drawn = false;
        if (textures != nullptr) {
            const render::Texture& texture = textures->Get(state.photoPath.c_str());
            if (texture.Valid()) {
                layout::Rect inner = previewRect;
                inner.x += 4.0f;
                inner.y += 4.0f;
                inner.w -= 8.0f;
                inner.h -= 8.0f;
                render::DrawImageContain(drawList, texture, inner);
                drawn = true;
            }
        }
        if (!drawn) {
            ui::ScopedFontSize hintFont(12.0f);
            ui::TextCentered(drawList, previewRect, colors.textMuted, "图片加载失败，已回退占位");
        }
    } else {
        ui::ScopedFontSize hintFont(12.0f);
        ui::TextCentered(drawList,
                         layout::Rect{previewRect.x, previewRect.CenterY() - 10.0f, previewRect.w,
                                      20.0f},
                         colors.textMuted, "点击素材库或本地上传");
    }

    // ---- 素材库（5 张素材 + 无图；点击切换并写回 state.photoPath）----
    const float gridX = blockStart.x + previewSize + 12.0f;
    const float gridY = blockStart.y + labelHeight;
    {
        ui::ScopedFontSize labelFont(11.0f);
        drawList->AddText(ImVec2(gridX, blockStart.y), colors.textMuted, "从素材库选择");
    }
    for (int item = 0; item < itemCount; ++item) {
        const int column = item % columns;
        const int row = item / columns;
        const layout::Rect thumb{gridX + static_cast<float>(column) * (thumbSize + thumbGap),
                                 gridY + static_cast<float>(row) * (thumbSize + thumbGap), thumbSize,
                                 thumbSize};
        const bool isNone = item >= kRoomTypePhotoCount;
        const std::string itemPath =
            isNone ? std::string() : std::string(kRoomTypePhotoLibrary[item]);
        const bool selected = isNone ? state.photoPath.empty() : (state.photoPath == itemPath);
        const bool hovered = mouse.x >= thumb.x && mouse.x <= thumb.Right() && mouse.y >= thumb.y &&
                             mouse.y <= thumb.Bottom();
        ui::FillRounded(drawList, thumb, colors.lightBlock, 3.0f);
        if (isNone) {
            DrawDashedFrame(drawList, thumb, colors.borderAxis, 1.0f, 3.0f, 3.0f);
            ui::ScopedFontSize noneFont(11.0f);
            ui::TextCentered(drawList, thumb, colors.textMuted, "无图");
        } else if (textures != nullptr) {
            const render::Texture& texture = textures->Get(itemPath.c_str());
            if (texture.Valid()) {
                render::DrawImageCover(drawList, texture, thumb);
            } else {
                ui::ScopedFontSize noneFont(10.0f);
                ui::TextCentered(drawList, thumb, colors.textMuted, "无图");
            }
        }
        if (hovered && !selected) {
            ui::FillRounded(drawList, thumb, theme::WithAlpha(colors.primary, 0.08f), 3.0f);
        }
        if (selected) {
            ui::StrokeRounded(drawList, thumb, colors.primary, 3.0f, 2.0f);
        }
        if (clicked && hovered) {
            state.photoPath = itemPath;
        }
    }

    // ---- 本地上传（platform::PickImageFile：png/jpg/jpeg/bmp/webp，≤5MB）----
    // 平台层负责复制到 <exe>/assets/uploads/roomtype/ 并返回 assets/... 相对路径；
    // 超限/非图片会返回 error（不复制文件），这里只 Toast 提示，取消则静默。
    const layout::Rect uploadRect{gridX, blockStart.y + labelHeight + gridHeight + 6.0f, gridWidth,
                                  26.0f};
    const bool uploadHovered = mouse.x >= uploadRect.x && mouse.x <= uploadRect.Right() &&
                               mouse.y >= uploadRect.y && mouse.y <= uploadRect.Bottom();
    ui::FillRounded(drawList, uploadRect,
                    uploadHovered ? theme::WithAlpha(colors.primary, 0.10f) : colors.panel, 4.0f);
    ui::StrokeRounded(drawList, uploadRect, colors.borderAxis, 4.0f, 1.0f);
    {
        ui::ScopedFontSize uploadFont(12.0f);
        ui::TextCentered(drawList, uploadRect, colors.primary, "本地上传");
    }
    if (clicked && uploadHovered) {
        constexpr std::size_t kMaxUploadBytes = 5u * 1024u * 1024u;
        const platform::FilePickResult picked =
            platform::PickImageFile("uploads/roomtype", kMaxUploadBytes);
        if (picked.ok) {
            state.photoPath = picked.relativePath;
            PushToast(world, "图片已上传：" + picked.fileName, ui::ToastType::Success);
        } else if (!picked.canceled) {
            PushToast(world, picked.error.empty() ? "本地图片上传失败" : picked.error,
                      ui::ToastType::Error);
        }
    }
}

// 导出当前查询结果（CSV：exports/ 目录，UTF-8 BOM，自动建目录）
void ExportRoomTypes(World& world, const RoomTypePageState& state) {
    std::vector<std::string> headers = {"序号", "房间图片", "房型类型", "床位费用",
                                        "房型介绍", "创建时间", "状态"};
    std::vector<std::vector<std::string>> rows;
    rows.reserve(state.results.size());
    for (std::size_t index = 0; index < state.results.size(); ++index) {
        const data::RoomTypeRecord& record = state.results[index];
        rows.push_back({std::to_string(index + 1), record.photoPath, record.name,
                        FormatPrice(record.price), record.introduction, record.createTime,
                        record.enabled ? "启用" : "禁用"});
    }
    const utils::CsvExportResult result = utils::ExportCsv("room_types", headers, rows);
    if (result.ok) {
        const std::size_t separator = result.path.find_last_of("\\/");
        const std::string fileName =
            separator == std::string::npos ? result.path : result.path.substr(separator + 1);
        PushToast(world,
                  "导出成功：" + fileName + "（共 " + std::to_string(rows.size()) + " 条）",
                  ui::ToastType::Success);
    } else {
        PushToast(world, "导出失败：" + result.error, ui::ToastType::Error);
    }
}

void DrawRoomTypeForm(World& world, data::IRoomTypeService& service, RoomTypePageState& state) {
    (void)service;
    const FormStyleGuard styleGuard;
    const float introHeight = ImGui::GetIO().DisplaySize.y < 700.0f ? 48.0f : 64.0f;

    if (ImGui::BeginTable("##room_type_form", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, kFormLabelWidth);
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch);

        // 房型名称*
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("房型名称", true);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##room_name", "请输入房型名称", state.name, sizeof(state.name));
        DrawFieldError(state.nameError);

        // 床位数量*
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("床位数量", true);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##room_beds", "请输入床位数量", state.bedCountText,
                                 sizeof(state.bedCountText));
        DrawFieldError(state.bedError);

        // 床位费用*
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("床位费用", true);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##room_price", "请输入床位费用", state.priceText,
                                 sizeof(state.priceText));
        DrawFieldError(state.priceError);

        // 介绍
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("介绍", false);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextMultiline("##room_intro", state.introduction, sizeof(state.introduction),
                                  ImVec2(ImGui::GetContentRegionAvail().x, introHeight));

        // 照片（素材库选择：148×148 预览 + 5 张素材 + 无图）
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("照片", false);
        ImGui::TableNextColumn();
        DrawPhotoPicker(world, state);

        // 备注
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawFormLabel("备注", false);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputTextWithHint("##room_remark", "请输入备注", state.remark, sizeof(state.remark));

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
// 表格数据（当前页切片 → TableRow）
// -----------------------------------------------------------------------------
void BuildTableRows(RoomTypePageState& state, std::vector<ui::TableRow>& rows) {
    rows.clear();
    const int total = static_cast<int>(state.results.size());
    const int pageSize = std::max(1, state.pagination.pageSize);
    const int maxPage = std::max(1, (total + pageSize - 1) / pageSize);
    state.pagination.page = std::max(1, std::min(state.pagination.page, maxPage));
    const int start = (state.pagination.page - 1) * pageSize;
    const int end = std::min(total, start + pageSize);
    rows.reserve(static_cast<std::size_t>(std::max(0, end - start)));
    for (int index = start; index < end; ++index) {
        const data::RoomTypeRecord& record = state.results[static_cast<std::size_t>(index)];
        ui::TableRow row;
        row.id = record.id;
        row.enabled = record.enabled;
        row.cells = {
            record.photoPath,                        // 房间图片（空 → 组件占位）
            record.name,                             // 房型类型
            FormatPrice(record.price),               // 床位费用
            record.introduction,                     // 房型介绍
            record.createTime,                       // 创建时间
            record.enabled ? "启用" : "禁用",        // 状态（Switch 用 row.enabled）
            record.enabled ? "修改|删除|禁用" : "修改|删除|启用",  // 操作（逐行动态标签）
        };
        rows.push_back(std::move(row));
    }
}

// -----------------------------------------------------------------------------
// 页面系统（Render 阶段；只读数据 + 交互后刷新）
// -----------------------------------------------------------------------------
class RoomTypePageSystem final : public ISystem {
public:
    const char* Name() const override { return "RoomTypePageSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& frame) override {
        const RouteState* route = world.TryResource<RouteState>();
        if (route == nullptr || route->current != Route::RoomType) {
            return;
        }
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        const DataServices* services = world.TryResource<DataServices>();
        RoomTypePageState* state = world.TryResource<RoomTypePageState>();
        if (shell == nullptr || services == nullptr || !services->roomType || state == nullptr) {
            return;
        }
        data::IRoomTypeService& service = *services->roomType;

        // 页面底色（外壳已铺整窗白底，这里保证内容区）
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        ui::PageBackground(drawList, shell->content);

        if (!state->initialized) {
            state->initialized = true;
            state->reloadRequested = true;
        }
        if (state->reloadRequested) {
            ReloadRows(service, *state);
        }

        const RoomTypePageLayout layoutState = ComputePageLayout(*shell);
        render::TextureCache* textures = world.TryResource<render::TextureCache>();

        // 页面宿主窗口：Widgets 组件依赖 ImGui 当前窗口（GetWindowDrawList / 交互），
        // 统一开一个铺满 pageViewport 的固定窗口；表格行数超出时由表格组件内部滚动。
        if (!ui::BeginFixedWindow("##room_type_page", layoutState.page)) {
            ui::EndFixedWindow();
            return;
        }

        // ---- ① 工具栏：新增 + 导出（CSV 当前查询结果）----
        std::vector<ui::ToolbarButton> toolbarButtons;
        toolbarButtons.push_back(ui::ToolbarButton{ui::ToolbarKind::Add, "新增", true});
        toolbarButtons.push_back(ui::ToolbarButton{ui::ToolbarKind::Export, "导出", true});
        const ui::ToolbarResult toolbar = ui::DrawToolbar(layoutState.toolbar, toolbarButtons);
        if (toolbar.clicked == 0) {
            OpenAddDialog(*state);
        } else if (toolbar.clicked == 1) {
            ExportRoomTypes(world, *state);
        }

        // ---- ② 数据表格 ----
        std::vector<ui::TableColumn> columns;
        columns.push_back(
            ui::TableColumn{"photo", "房间图片", 100.0f, ui::Align::Center, ui::CellKind::Image});
        columns.push_back(
            ui::TableColumn{"name", "房型类型", 140.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"price", "床位费用", 120.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"introduction", "房型介绍", 160.0f, ui::Align::Left, ui::CellKind::Text});
        columns.push_back(ui::TableColumn{"createTime", "创建时间", 150.0f, ui::Align::Center,
                                          ui::CellKind::Text});
        columns.push_back(
            ui::TableColumn{"status", "状态", 100.0f, ui::Align::Center, ui::CellKind::Switch});
        columns.push_back(
            ui::TableColumn{"actions", "操作", 200.0f, ui::Align::Center, ui::CellKind::Actions});

        std::vector<ui::TableRow> rows;
        BuildTableRows(*state, rows);

        ui::TableDrawOptions options;
        options.showCheckbox = false;
        options.showIndex = true;
        options.actionLabels = {"修改", "删除", "启用/禁用"};

        const ui::TableResult table = ui::DrawDataTable(layoutState.table, columns, rows,
                                                        state->checkedIds, textures, options);

        // 状态开关（Switch）
        for (const ui::TableResult::SwitchToggle& toggle : table.switchToggles) {
            SetStatus(world, service, *state, toggle.rowId, toggle.enabled);
        }
        // 操作列（修改/删除/启停）
        if (table.clickedAction >= 0 && table.actionRowId > 0) {
            HandleRowAction(world, service, *state, table.actionRowId, table.clickedAction);
        }

        // ---- ③ 分页（贴底）----
        ui::DrawPagination(layoutState.pagination, state->pagination);

        // ---- ④ 表单弹窗 ----
        if (state->dialog.open) {
            if (ui::BeginFormDialog(layoutState.page, state->dialog)) {
                DrawRoomTypeForm(world, service, *state);
            }
            const ui::DialogResult dialogResult = ui::EndFormDialog();
            if (dialogResult == ui::DialogResult::Ok) {
                SubmitForm(world, service, *state);
            }
        }

        // ---- ⑤ 删除确认框 ----
        if (state->confirmOpen) {
            const ui::ConfirmAction action = ui::DrawConfirmDialog(
                layoutState.page, state->confirmOpen, "系统提示", state->confirmMessage);
            if (action == ui::ConfirmAction::Ok) {
                ApplyRemove(world, service, *state);
                state->confirmOpen = false;
            }
        }

        // ---- ⑥ Toast：页面只 PushToast，由外壳每帧统一 DrawToasts（lead 确认口径）----
        if (kDrawToastsInPage) {
            if (auto* toasts = world.TryResource<ui::ToastState>()) {
                ui::DrawToasts(shell->viewport, *toasts, frame.deltaTime);
            }
        }

        ui::EndFixedWindow();
    }
};

}  // namespace

void RegisterRoomTypeModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<RoomTypePageState>();
    scheduler.Add(std::make_unique<RoomTypePageSystem>(), 200);
}

}  // namespace zzyl
