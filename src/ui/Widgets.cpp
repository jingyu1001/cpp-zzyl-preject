// =============================================================================
// 通用列表页组件实现（M0）· cpp-dev1
//
// 契约：include/zzyl/ui/Widgets.h（只读）｜规格：UI复刻规格_Cpp_v0.2.md §3
//
// 约定：
//  - 组件只做绘制与交互采集，不读业务数据/数据服务；
//  - 所有组件必须在页面自己的 ImGui 窗口内调用（页面用 ui::BeginFixedWindow 包住），
//    因为交互依赖 InvisibleButton；
//  - 尺寸/颜色来自 UI 规格 v0.2 §3；Theme 未收录的 Element Plus 灰阶/浅色按钮色
//    定义为本文件内部常量（任务单 §2 允许）；
//  - 文字绘制一律用 ui::ScopedFontSize / ScopedBoldFont 控制字号。
// =============================================================================

#include "zzyl/ui/Widgets.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

#include "zzyl/render/Texture.h"
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"

namespace zzyl::ui {
namespace {

using layout::Rect;

// ---- 规格尺寸（UI复刻规格_Cpp_v0.2 §3 实测基准，1280×720） ----
namespace spec {
constexpr float kFontBase = 14.0f;    // 正文/控件/按钮
constexpr float kFontSmall = 13.0f;   // 表头
constexpr float kFontTiny = 12.0f;    // 标签/辅助
constexpr float kFontTitle = 18.0f;   // 弹窗标题
constexpr float kControlHeight = 32.0f;
constexpr float kLabelWidth = 68.0f;
constexpr float kLabelGap = 8.0f;
constexpr float kItemGapX = 24.0f;
constexpr float kRowSpacing = 28.0f;
constexpr float kSearchButtonWidth = 80.0f;
constexpr float kButtonGap = 12.0f;
constexpr int kSearchColumns = 3;  // 搜索栏固定 3 列（控件按列宽自适应收缩）
constexpr float kToolbarGap = 10.0f;
constexpr float kTableHeaderHeight = 40.0f;
constexpr float kTableRowHeight = 49.0f;
constexpr float kTableCheckWidth = 50.0f;
constexpr float kTableIndexWidth = 50.0f;
constexpr float kTableScrollbar = 6.0f;
constexpr float kTableScrollbarGap = 6.0f;  // 内容区底部与横向滚动条之间的留白（BUG-S2-01）
constexpr float kTableImageSize = 60.0f;
constexpr float kPaginationHeight = 32.0f;
constexpr float kPaginationButton = 32.0f;
constexpr float kPaginationGap = 8.0f;
constexpr float kPaginationPageSizeWidth = 88.0f;
constexpr float kPaginationJumpWidth = 42.0f;
constexpr float kDialogTopRatio = 0.06f;
constexpr float kDialogPadding = 20.0f;
constexpr float kDialogTitleHeight = 40.0f;
constexpr float kDialogBottomPadding = 16.0f;
constexpr float kDialogFooterGap = 16.0f;
constexpr float kDialogButtonWidth = 80.0f;
constexpr float kConfirmWidth = 420.0f;
constexpr float kConfirmHeight = 128.0f;
constexpr float kToastHeight = 38.0f;
constexpr float kToastTop = 16.0f;
constexpr float kToastGap = 12.0f;
}  // namespace spec

// ---- Element Plus 扩展色（UI v0.2 §3 实测；Theme 未收录的列表页专用灰阶/浅色） ----
namespace el {
constexpr ImU32 kWhite = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
constexpr ImU32 kTextLabel = IM_COL32(0x60, 0x62, 0x66, 0xFF);        // #606266
constexpr ImU32 kTableHeaderText = IM_COL32(0x51, 0x5A, 0x6E, 0xFF);  // #515A6E
constexpr ImU32 kPlaceholder = IM_COL32(0xA8, 0xAB, 0xB2, 0xFF);      // #A8ABB2
constexpr ImU32 kBorderInput = IM_COL32(0xDC, 0xDF, 0xE6, 0xFF);      // #DCDFE6
constexpr ImU32 kBorderLight = IM_COL32(0xEB, 0xEE, 0xF5, 0xFF);      // #EBEEF5
constexpr ImU32 kHeaderBg = IM_COL32(0xF8, 0xF8, 0xF9, 0xFF);         // #F8F8F9
constexpr ImU32 kRowHover = IM_COL32(0xF5, 0xF7, 0xFA, 0xFF);         // #F5F7FA
constexpr ImU32 kSwitchOff = IM_COL32(0xDC, 0xDF, 0xE6, 0xFF);
constexpr ImU32 kScrollTrack = IM_COL32(0xF1, 0xF1, 0xF1, 0xFF);
constexpr ImU32 kScrollThumb = IM_COL32(0xC0, 0xC0, 0xC0, 0xFF);
constexpr ImU32 kSuccessBg = IM_COL32(0xF0, 0xF9, 0xEB, 0xFF);
constexpr ImU32 kWarningBg = IM_COL32(0xFD, 0xF6, 0xEC, 0xFF);
constexpr ImU32 kDangerBg = IM_COL32(0xFE, 0xF0, 0xF0, 0xFF);
constexpr ImU32 kAddBg = IM_COL32(0xE5, 0xF7, 0xF5, 0xFF);        // 新增
constexpr ImU32 kAddBorder = IM_COL32(0x7F, 0xDB, 0xD7, 0xFF);
constexpr ImU32 kEditBg = IM_COL32(0xF0, 0xF9, 0xEB, 0xFF);       // 修改
constexpr ImU32 kDeleteBg = IM_COL32(0xFE, 0xF0, 0xF0, 0xFF);     // 删除
constexpr ImU32 kImportBg = IM_COL32(0xF4, 0xF4, 0xF5, 0xFF);     // 导入
constexpr ImU32 kImportText = IM_COL32(0x90, 0x93, 0x99, 0xFF);
constexpr ImU32 kExportBg = IM_COL32(0xFD, 0xF6, 0xEC, 0xFF);     // 导出
constexpr ImU32 kExportText = IM_COL32(0xE6, 0xA2, 0x3C, 0xFF);
constexpr ImU32 kDisabledText = IM_COL32(0xC0, 0xC4, 0xCC, 0xFF);
}  // namespace el

// =============================================================================
// 基础工具
// =============================================================================
Rect CenterIn(const Rect& parent, float width, float height) {
    return Rect{parent.CenterX() - width * 0.5f, parent.CenterY() - height * 0.5f, width, height};
}

bool IsHovered(const Rect& rect) {
    return !rect.Empty() && ImGui::IsMouseHoveringRect(ui::Min(rect), ui::Max(rect), false);
}

float TextWidth(const char* text) { return ImGui::CalcTextSize(text).x; }

// UTF-8 安全截断（按像素宽度追加省略号）
std::string Ellipsize(const std::string& text, float maxWidth) {
    if (text.empty() || maxWidth <= 0.0f) {
        return std::string();
    }
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
        return text;
    }
    const char* ellipsis = "…";
    const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;
    if (maxWidth <= ellipsisWidth) {
        return std::string(ellipsis);
    }
    const char* data = text.c_str();
    std::size_t length = 0;
    while (data[length] != '\0') {
        std::size_t next = length + 1;
        while (data[next] != '\0' && (static_cast<unsigned char>(data[next]) & 0xC0U) == 0x80U) {
            ++next;
        }
        const std::string candidate(data, next);
        if (ImGui::CalcTextSize(candidate.c_str()).x + ellipsisWidth > maxWidth) {
            break;
        }
        length = next;
    }
    return std::string(data, length) + ellipsis;
}

// UTF-8 安全换行（确认框文案等短文本）
std::vector<std::string> WrapText(const std::string& text, float maxWidth) {
    std::vector<std::string> lines;
    if (text.empty() || maxWidth <= 0.0f) {
        return lines;
    }
    std::string line;
    std::size_t index = 0;
    while (index < text.size()) {
        if (text[index] == '\n') {
            lines.push_back(line);
            line.clear();
            ++index;
            continue;
        }
        std::size_t next = index + 1;
        while (next < text.size() && (static_cast<unsigned char>(text[next]) & 0xC0U) == 0x80U) {
            ++next;
        }
        const std::string character = text.substr(index, next - index);
        const std::string candidate = line + character;
        if (!line.empty() && ImGui::CalcTextSize(candidate.c_str()).x > maxWidth) {
            lines.push_back(line);
            line = character;
        } else {
            line = candidate;
        }
        index = next;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

void DrawClearCross(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float half = size * 0.5f;
    drawList->AddLine(ImVec2(center.x - half, center.y - half),
                      ImVec2(center.x + half, center.y + half), color, 1.4f);
    drawList->AddLine(ImVec2(center.x - half, center.y + half),
                      ImVec2(center.x + half, center.y - half), color, 1.4f);
}

// 统一按钮：底色 + 可选边框 + 居中文字；enabled=false 时半透明且不可点
bool DrawFlatButton(const char* id, const Rect& rect, ImU32 background, ImU32 textColor,
                    ImU32 borderColor, const char* label, float fontSize, bool enabled = true,
                    float rounding = 4.0f) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool pressed = ui::InvisibleButtonAt(id, rect);
    const bool hovered = enabled && ImGui::IsItemHovered();
    const float alpha = enabled ? 1.0f : 0.5f;
    ui::FillRounded(drawList, rect, theme::WithAlpha(background, alpha), rounding);
    if (borderColor != 0) {
        ui::StrokeRounded(drawList, rect, theme::WithAlpha(borderColor, alpha), rounding, 1.0f);
    }
    if (hovered) {
        ui::FillRounded(drawList, rect, theme::WithAlpha(IM_COL32(0, 0, 0, 255), 0.04f), rounding);
    }
    {
        ui::ScopedFontSize font(fontSize);
        ui::TextCentered(drawList, rect, theme::WithAlpha(textColor, alpha), label);
    }
    return pressed && enabled;
}

// ---- 小图标（几何绘制，替代 SVG 图标资源） ----
void DrawMagnifier(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float radius = size * 0.32f;
    drawList->AddCircle(center, radius, color, 12, 1.5f);
    drawList->AddLine(ImVec2(center.x + radius * 0.72f, center.y + radius * 0.72f),
                      ImVec2(center.x + size * 0.5f, center.y + size * 0.5f), color, 1.5f);
}

void DrawRefreshIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float radius = size * 0.32f;
    drawList->PathArcTo(center, radius, 0.7f, 5.2f, 14);
    drawList->PathStroke(color, 0, 1.5f);
    const ImVec2 tip(center.x + radius * 0.95f, center.y - radius * 0.2f);
    drawList->AddTriangleFilled(tip, ImVec2(tip.x - 4.0f, tip.y - 1.0f),
                                ImVec2(tip.x - 1.0f, tip.y + 4.0f), color);
}

void DrawPlusIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float half = size * 0.32f;
    drawList->AddLine(ImVec2(center.x - half, center.y), ImVec2(center.x + half, center.y), color,
                      1.5f);
    drawList->AddLine(ImVec2(center.x, center.y - half), ImVec2(center.x, center.y + half), color,
                      1.5f);
}

void DrawPencilIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float half = size * 0.34f;
    drawList->AddLine(ImVec2(center.x - half, center.y + half),
                      ImVec2(center.x + half, center.y - half), color, 1.6f);
    drawList->AddTriangleFilled(ImVec2(center.x + half, center.y - half),
                                ImVec2(center.x + half, center.y - half + 4.5f),
                                ImVec2(center.x + half - 4.5f, center.y - half), color);
}

void DrawTrashIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float half = size * 0.3f;
    const float top = center.y - half;
    drawList->AddLine(ImVec2(center.x - half, top), ImVec2(center.x + half, top), color, 1.5f);
    drawList->AddRect(ImVec2(center.x - half, top + 1.5f), ImVec2(center.x + half, center.y + half),
                      color, 1.5f, 0, 1.5f);
    drawList->AddLine(ImVec2(center.x - 2.0f, top - 2.0f), ImVec2(center.x + 2.0f, top - 2.0f), color,
                      1.5f);
}

void DrawArrowIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, bool up) {
    const float half = size * 0.34f;
    const float direction = up ? -1.0f : 1.0f;
    drawList->AddLine(ImVec2(center.x, center.y - half * direction),
                      ImVec2(center.x, center.y + half * direction), color, 1.5f);
    drawList->AddLine(ImVec2(center.x, center.y + half * direction),
                      ImVec2(center.x - half * 0.7f, center.y + half * 0.2f * direction), color, 1.5f);
    drawList->AddLine(ImVec2(center.x, center.y + half * direction),
                      ImVec2(center.x + half * 0.7f, center.y + half * 0.2f * direction), color, 1.5f);
    const float base = center.y + half * 1.05f * direction;
    drawList->AddLine(ImVec2(center.x - half, base), ImVec2(center.x + half, base), color, 1.5f);
}

// 图标 + 文字的浅色按钮（工具栏 / 搜索按钮）
bool DrawIconButton(const char* id, const Rect& rect, ImU32 background, ImU32 textColor,
                    ImU32 borderColor, const char* label,
                    void (*icon)(ImDrawList*, ImVec2, float, ImU32), bool enabled = true) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool pressed = ui::InvisibleButtonAt(id, rect);
    const bool hovered = enabled && ImGui::IsItemHovered();
    const float alpha = enabled ? 1.0f : 0.5f;
    ui::FillRounded(drawList, rect, theme::WithAlpha(background, alpha), 4.0f);
    if (borderColor != 0) {
        ui::StrokeRounded(drawList, rect, theme::WithAlpha(borderColor, alpha), 4.0f, 1.0f);
    }
    if (hovered) {
        ui::FillRounded(drawList, rect, theme::WithAlpha(IM_COL32(0, 0, 0, 255), 0.04f), 4.0f);
    }
    ui::ScopedFontSize font(spec::kFontBase);
    const float textW = TextWidth(label);
    const float iconSize = 13.0f;
    const float gap = 4.0f;
    const float groupW = iconSize + gap + textW;
    const float startX = rect.CenterX() - groupW * 0.5f;
    if (icon != nullptr) {
        icon(drawList, ImVec2(startX + iconSize * 0.5f, rect.CenterY()), iconSize,
             theme::WithAlpha(textColor, alpha));
    }
    ui::Text(drawList,
             ImVec2(startX + iconSize + gap, rect.CenterY() - ImGui::GetFontSize() * 0.5f),
             theme::WithAlpha(textColor, alpha), label);
    return pressed && enabled;
}

// =============================================================================
// 文本输入 / 下拉选择（搜索栏与分页共用）
// =============================================================================
struct TextFieldResult {
    bool changed = false;
    bool enter = false;
};

TextFieldResult DrawTextField(const char* id, const Rect& rect, std::string& value, const char* hint,
                              bool clearable = true) {
    TextFieldResult result;
    if (rect.Empty()) {
        return result;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(8.0f, std::max(2.0f, (rect.h - spec::kFontBase) * 0.5f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, el::kWhite);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, el::kWhite);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, el::kWhite);
    ImGui::PushStyleColor(ImGuiCol_Border, el::kBorderInput);
    ImGui::PushStyleColor(ImGuiCol_Text, el::kTextLabel);
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, el::kPlaceholder);
    {
        ui::ScopedFontSize font(spec::kFontBase);
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        ImGui::SetCursorScreenPos(ui::Min(rect));
        ImGui::SetNextItemWidth(rect.w);
        result.enter = ImGui::InputTextWithHint(id, hint != nullptr ? hint : "", buffer,
                                                sizeof(buffer),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
        if (std::strcmp(buffer, value.c_str()) != 0) {
            value.assign(buffer);
            result.changed = true;
        }
        const bool inputHovered = ImGui::IsItemHovered();
        const bool inputActive = ImGui::IsItemActive();
        if (clearable && !value.empty() && (inputHovered || inputActive)) {
            const std::string clearId = std::string(id) + "_clear";
            const Rect clearRect{rect.Right() - 22.0f, rect.y, 22.0f, rect.h};
            if (ui::InvisibleButtonAt(clearId.c_str(), clearRect)) {
                value.clear();
                result.changed = true;
            }
            DrawClearCross(ImGui::GetWindowDrawList(), ui::Center(clearRect), 10.0f,
                           el::kPlaceholder);
        }
    }
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);
    return result;
}

bool DrawSelectField(const char* id, const Rect& rect, std::string& value,
                     const std::vector<std::string>& options, const char* hint) {
    bool changed = false;
    if (rect.Empty()) {
        return changed;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(8.0f, std::max(2.0f, (rect.h - spec::kFontBase) * 0.5f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, el::kWhite);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, el::kWhite);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, el::kWhite);
    ImGui::PushStyleColor(ImGuiCol_Border, el::kBorderInput);
    ImGui::PushStyleColor(ImGuiCol_Text, el::kTextLabel);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, el::kWhite);
    // 下拉箭头背景默认取 ImGuiCol_Button（主题为主色绿）→ 改为白底浅灰 hover
    ImGui::PushStyleColor(ImGuiCol_Button, el::kWhite);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, el::kRowHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, el::kRowHover);
    {
        ui::ScopedFontSize font(spec::kFontBase);
        const bool empty = value.empty();
        ImGui::SetCursorScreenPos(ui::Min(rect));
        ImGui::SetNextItemWidth(rect.w);
        if (empty) {
            ImGui::PushStyleColor(ImGuiCol_Text, el::kPlaceholder);
        }
        if (ImGui::BeginCombo(id, empty ? (hint != nullptr ? hint : "") : value.c_str(),
                              ImGuiComboFlags_HeightRegular)) {
            for (const std::string& option : options) {
                const bool selected = (option == value);
                if (ImGui::Selectable(option.c_str(), selected)) {
                    value = option;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (empty) {
            ImGui::PopStyleColor();
        }
    }
    ImGui::PopStyleColor(9);
    ImGui::PopStyleVar(3);
    return changed;
}

// =============================================================================
// 工具栏样式（§3.2）
// =============================================================================
struct ToolbarStyle {
    ImU32 background;
    ImU32 text;
    ImU32 border;
    void (*icon)(ImDrawList*, ImVec2, float, ImU32);
};

ToolbarStyle StyleForToolbarKind(ToolbarKind kind) {
    const theme::Palette& colors = theme::Colors();
    switch (kind) {
        case ToolbarKind::Add:
            return {el::kAddBg, colors.primary, el::kAddBorder, &DrawPlusIcon};
        case ToolbarKind::Edit:
            return {el::kEditBg, colors.success, colors.success, &DrawPencilIcon};
        case ToolbarKind::Delete:
            return {el::kDeleteBg, colors.danger, colors.danger, &DrawTrashIcon};
        case ToolbarKind::Import:
            return {el::kImportBg, el::kImportText, el::kImportText,
                    [](ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
                        DrawArrowIcon(drawList, center, size, color, true);
                    }};
        case ToolbarKind::Export:
            return {el::kExportBg, el::kExportText, el::kExportText,
                    [](ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
                        DrawArrowIcon(drawList, center, size, color, false);
                    }};
    }
    return {el::kAddBg, colors.primary, el::kAddBorder, &DrawPlusIcon};
}

// =============================================================================
// 表格小控件
// =============================================================================
bool DrawCheckboxControl(const char* id, const Rect& hitRect, bool checked, bool indeterminate,
                         bool interactive) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    bool pressed = false;
    bool hovered = false;
    if (interactive) {
        pressed = ui::InvisibleButtonAt(id, hitRect);
        hovered = ImGui::IsItemHovered();
    }
    const Rect box = CenterIn(hitRect, 16.0f, 16.0f);
    const ImU32 primary = theme::Colors().primary;
    const bool marked = checked || indeterminate;
    ui::FillRounded(drawList, box, marked ? primary : el::kWhite, 2.0f);
    ui::StrokeRounded(drawList, box, marked ? primary : (hovered ? primary : el::kBorderInput), 2.0f,
                      1.0f);
    if (indeterminate && !checked) {
        ui::Line(drawList, box.x + 4.0f, box.CenterY(), box.Right() - 4.0f, box.CenterY(), el::kWhite,
                 1.6f);
    } else if (checked) {
        drawList->AddLine(ImVec2(box.x + 4.0f, box.CenterY()),
                          ImVec2(box.x + 6.5f, box.Bottom() - 4.5f), el::kWhite, 1.6f);
        drawList->AddLine(ImVec2(box.x + 6.5f, box.Bottom() - 4.5f),
                          ImVec2(box.Right() - 3.5f, box.y + 4.5f), el::kWhite, 1.6f);
    }
    return pressed;
}

bool DrawSwitchControl(const char* id, const Rect& hitRect, bool enabled, bool interactive) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    bool pressed = false;
    if (interactive) {
        pressed = ui::InvisibleButtonAt(id, hitRect);
    }
    const Rect track = CenterIn(hitRect, 40.0f, 20.0f);
    ui::FillRounded(drawList, track, enabled ? theme::Colors().primary : el::kSwitchOff, 10.0f);
    const ImVec2 knobCenter(enabled ? track.Right() - 10.0f : track.x + 10.0f, track.CenterY());
    drawList->AddCircleFilled(knobCenter, 8.0f, el::kWhite);
    return pressed;
}

void DrawStatusTagControl(const Rect& cellRect, bool enabled) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const char* text = enabled ? "启用" : "禁用";
    ui::ScopedFontSize font(spec::kFontTiny);
    const float width = std::min(cellRect.w, TextWidth(text) + 16.0f);
    const Rect tag = CenterIn(cellRect, width, 24.0f);
    ui::FillRounded(drawList, tag, enabled ? el::kSuccessBg : el::kDangerBg, 4.0f);
    ui::TextCentered(drawList, tag, enabled ? theme::Colors().success : theme::Colors().danger,
                     text);
}

// 单元格文本（对齐 + 省略号）；返回是否被截断
bool DrawTableCellText(ImDrawList* drawList, const Rect& cellRect, const std::string& text,
                       float fontSize, ImU32 color, Align align, float paddingX) {
    ui::ScopedFontSize font(fontSize);
    const float maxWidth = std::max(0.0f, cellRect.w - paddingX * 2.0f);
    const std::string shown = Ellipsize(text, maxWidth);
    const Rect innerRect{cellRect.x + paddingX, cellRect.y, maxWidth, cellRect.h};
    switch (align) {
        case Align::Left:
            ui::TextLeftMiddle(drawList, innerRect, color, shown.c_str());
            break;
        case Align::Right:
            ui::TextRightMiddle(drawList, innerRect, color, shown.c_str());
            break;
        case Align::Center:
        default:
            ui::TextCentered(drawList, innerRect, color, shown.c_str());
            break;
    }
    return shown.size() != text.size();
}

std::vector<std::string> SplitActions(const std::string& cell,
                                      const std::vector<std::string>& fallback) {
    std::vector<std::string> labels;
    if (!cell.empty()) {
        std::size_t begin = 0;
        while (begin <= cell.size()) {
            const std::size_t end = cell.find('|', begin);
            std::string piece =
                cell.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
            const std::size_t first = piece.find_first_not_of(" \t");
            const std::size_t last = piece.find_last_not_of(" \t");
            if (first != std::string::npos && last != std::string::npos) {
                labels.push_back(piece.substr(first, last - first + 1));
            }
            if (end == std::string::npos) {
                break;
            }
            begin = end + 1;
        }
    }
    if (labels.empty()) {
        labels = fallback;
    }
    return labels;
}

void DrawTableScrollbar(ImDrawList* drawList, const Rect& trackRect, float viewSize,
                        float contentSize, float scroll, bool vertical) {
    if (contentSize <= viewSize + 0.5f) {
        return;
    }
    const float thickness = spec::kTableScrollbar;
    const float radius = thickness * 0.5f;
    ui::FillRounded(drawList, trackRect, el::kScrollTrack, radius);
    const float view = vertical ? trackRect.h : trackRect.w;
    const float thumbSize = std::max(24.0f, view * viewSize / std::max(1.0f, contentSize));
    const float travel = std::max(0.0f, view - thumbSize);
    const float maxScroll = std::max(1.0f, contentSize - viewSize);
    const float offset = travel * std::clamp(scroll / maxScroll, 0.0f, 1.0f);
    const Rect thumb = vertical ? Rect{trackRect.x, trackRect.y + offset, thickness, thumbSize}
                                : Rect{trackRect.x + offset, trackRect.y, thumbSize, thickness};
    ui::FillRounded(drawList, thumb, el::kScrollThumb, radius);
}

// 滚动条交互：按住轨道拖动（缩略块跟随鼠标），返回滚动值是否变化。
bool DragTableScrollbar(const char* id, const Rect& trackRect, float viewSize, float contentSize,
                        float& scroll, bool vertical) {
    if (contentSize <= viewSize + 0.5f || trackRect.Empty()) {
        return false;
    }
    bool changed = false;
    if (ui::InvisibleButtonAt(id, trackRect)) {
        // 按下即跳转（后续帧由 IsItemActive 跟随鼠标）
        changed = true;
    }
    if (ImGui::IsItemActive()) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float view = vertical ? trackRect.h : trackRect.w;
        const float thumbSize = std::max(24.0f, view * viewSize / std::max(1.0f, contentSize));
        const float travel = std::max(1.0f, view - thumbSize);
        const float mousePos = vertical ? mouse.y : mouse.x;
        const float trackStart = vertical ? trackRect.y : trackRect.x;
        const float t = std::clamp((mousePos - trackStart - thumbSize * 0.5f) / travel, 0.0f, 1.0f);
        const float target = t * std::max(0.0f, contentSize - viewSize);
        if (std::abs(target - scroll) > 0.5f) {
            scroll = target;
            changed = true;
        }
    }
    return changed;
}

int SearchRowCount(int fieldCount) {
    if (fieldCount <= 0) {
        return 0;
    }
    return (fieldCount + spec::kSearchColumns - 1) / spec::kSearchColumns;
}

// =============================================================================
// 表单弹窗帧状态（Begin/End 配对）
// =============================================================================
struct FormDialogFrame {
    bool windowOpened = false;
    bool closeRequested = false;
    DialogState* state = nullptr;
    int styleColorPushes = 0;
    int styleVarPushes = 0;
};

FormDialogFrame g_formDialogFrame;

void PushDialogStyle() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, el::kWhite);
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(spec::kDialogPadding, 0.0f));
    g_formDialogFrame.styleColorPushes = 2;
    g_formDialogFrame.styleVarPushes = 3;
}

void PopDialogStyle() {
    ImGui::PopStyleColor(g_formDialogFrame.styleColorPushes);
    ImGui::PopStyleVar(g_formDialogFrame.styleVarPushes);
}

void DrawDialogOverlay(const char* id) {
    const ImGuiIO& io = ImGui::GetIO();
    const Rect full{0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y};
    // 注意：不能用 ui::BeginFixedWindow —— 它带 NoBringToFrontOnFocus，
    // ImGui 会把这类窗口 push_front（渲染在最底层），遮罩/确认框会被页面窗口盖住。
    // 这里用普通窗口（无 NoBringToFrontOnFocus）以确保弹窗层位于页面之上。
    ImGui::SetNextWindowPos(ImVec2(full.x, full.y));
    ImGui::SetNextWindowSize(ImVec2(std::max(1.0f, full.w), std::max(1.0f, full.h)));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoNavFocus |
                                   ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin(id, nullptr, flags)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(full.w, full.h),
                                IM_COL32(0, 0, 0, 110));
    }
    ImGui::End();
}

// 置顶卡片窗口（确认框用；规则同 DrawDialogOverlay）
bool BeginTopCard(const char* id, const Rect& rect) {
    ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y));
    ImGui::SetNextWindowSize(ImVec2(std::max(1.0f, rect.w), std::max(1.0f, rect.h)));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoNavFocus |
                                   ImGuiWindowFlags_NoFocusOnAppearing;
    return ImGui::Begin(id, nullptr, flags);
}

void EndTopCard() { ImGui::End(); }

}  // namespace

// =============================================================================
// 搜索栏（§3.1）
// =============================================================================
SearchBarResult DrawSearchBar(const Rect& area, std::vector<SearchField>& fields) {
    SearchBarResult result;
    if (area.Empty()) {
        return result;
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ui::Fill(drawList, area, theme::Colors().panel);

    const float columnWidth =
        (area.w - spec::kItemGapX * static_cast<float>(spec::kSearchColumns - 1)) /
        static_cast<float>(spec::kSearchColumns);
    for (std::size_t index = 0; index < fields.size(); ++index) {
        SearchField& field = fields[index];
        const int row = static_cast<int>(index) / spec::kSearchColumns;
        const int column = static_cast<int>(index) % spec::kSearchColumns;
        const float x = area.x + static_cast<float>(column) * (columnWidth + spec::kItemGapX);
        const float y =
            area.y + static_cast<float>(row) * (spec::kControlHeight + spec::kRowSpacing);
        const Rect labelRect{x, y, spec::kLabelWidth, spec::kControlHeight};
        {
            ui::ScopedFontSize font(spec::kFontBase);
            ui::TextRightMiddle(drawList, labelRect, el::kTextLabel, field.label.c_str());
        }
        const float maxControl =
            std::max(80.0f, columnWidth - spec::kLabelWidth - spec::kLabelGap);
        const float controlWidth = std::clamp(field.controlWidth, 80.0f, maxControl);
        const Rect controlRect{x + spec::kLabelWidth + spec::kLabelGap, y, controlWidth,
                               spec::kControlHeight};
        char fieldId[64];
        std::snprintf(fieldId, sizeof(fieldId), "##zzyl_search_field_%d", static_cast<int>(index));
        switch (field.type) {
            case SearchFieldType::Text:
            case SearchFieldType::DateRange: {
                const char* hint =
                    field.type == SearchFieldType::DateRange ? "开始日期 - 结束日期" : "请输入";
                const TextFieldResult fieldResult =
                    DrawTextField(fieldId, controlRect, field.value, hint);
                if (fieldResult.enter) {
                    result.search = true;
                }
                break;
            }
            case SearchFieldType::Select:
                DrawSelectField(fieldId, controlRect, field.value, field.options, "请选择");
                break;
        }
    }

    const int rows = SearchRowCount(static_cast<int>(fields.size()));
    const float buttonY =
        area.y + static_cast<float>(rows) * (spec::kControlHeight + spec::kRowSpacing);
    const Rect searchRect{area.x, buttonY, spec::kSearchButtonWidth, spec::kControlHeight};
    if (DrawIconButton("##zzyl_search_button", searchRect, theme::Colors().primary,
                       theme::Colors().textInverse, 0, "搜索", &DrawMagnifier)) {
        result.search = true;
    }
    const Rect resetRect{searchRect.Right() + spec::kButtonGap, buttonY, spec::kSearchButtonWidth,
                         spec::kControlHeight};
    if (DrawIconButton("##zzyl_reset_button", resetRect, el::kWhite, el::kTextLabel,
                       el::kBorderInput, "重置", &DrawRefreshIcon)) {
        for (SearchField& field : fields) {
            field.value.clear();
        }
        result.reset = true;
    }
    return result;
}

float SearchBarHeight(int fieldCount) {
    const int rows = SearchRowCount(fieldCount);
    return static_cast<float>(rows) * (spec::kControlHeight + spec::kRowSpacing) +
           spec::kControlHeight;
}

// =============================================================================
// 工具栏（§3.2）
// =============================================================================
ToolbarResult DrawToolbar(const Rect& area, std::vector<ToolbarButton>& buttons) {
    ToolbarResult result;
    if (area.Empty()) {
        return result;
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ui::Fill(drawList, area, theme::Colors().panel);
    float x = area.x;
    const float y = area.y + std::max(0.0f, (area.h - spec::kControlHeight) * 0.5f);
    for (std::size_t index = 0; index < buttons.size(); ++index) {
        const ToolbarButton& button = buttons[index];
        const Rect buttonRect{x, y, spec::kSearchButtonWidth, spec::kControlHeight};
        const ToolbarStyle style = StyleForToolbarKind(button.kind);
        const char* label = button.label.empty() ? "按钮" : button.label.c_str();
        char buttonId[64];
        std::snprintf(buttonId, sizeof(buttonId), "##zzyl_toolbar_%d", static_cast<int>(index));
        if (DrawIconButton(buttonId, buttonRect, style.background, style.text, style.border, label,
                           style.icon, button.enabled)) {
            result.clicked = static_cast<int>(index);
        }
        x += spec::kSearchButtonWidth + spec::kToolbarGap;
    }
    return result;
}

float ToolbarHeight() { return spec::kControlHeight; }

// =============================================================================
// 数据表格（§3.3）
// =============================================================================
TableResult DrawDataTable(const Rect& area, const std::vector<TableColumn>& columns,
                          const std::vector<TableRow>& rows, std::vector<int>& checkedIds,
                          const render::TextureCache* textures, const TableDrawOptions& options) {
    TableResult result;
    if (area.Empty()) {
        result.checkedIds = checkedIds;
        return result;
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImGuiIO& io = ImGui::GetIO();
    const bool interactiveWindow = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    const auto isChecked = [&checkedIds](int id) {
        return std::find(checkedIds.begin(), checkedIds.end(), id) != checkedIds.end();
    };
    const auto setChecked = [&checkedIds](int id, bool checked) {
        const auto it = std::find(checkedIds.begin(), checkedIds.end(), id);
        if (checked && it == checkedIds.end()) {
            checkedIds.push_back(id);
        } else if (!checked && it != checkedIds.end()) {
            checkedIds.erase(it);
        }
    };

    // ---- 列宽：固定列 + 基准宽度比例缩放；放不下时横向滚动 ----
    float fixedWidth = 0.0f;
    if (options.showCheckbox) {
        fixedWidth += spec::kTableCheckWidth;
    }
    if (options.showIndex) {
        fixedWidth += spec::kTableIndexWidth;
    }
    float baseWidth = 0.0f;
    for (const TableColumn& column : columns) {
        baseWidth += std::max(1.0f, column.width);
    }
    if (baseWidth <= 0.0f) {
        baseWidth = 1.0f;
    }

    const float contentHeight = static_cast<float>(rows.size()) * spec::kTableRowHeight;
    float availWidth = area.w;
    float bodyHeight = area.h - spec::kTableHeaderHeight;
    bool needVertical = contentHeight > bodyHeight + 0.5f;
    bool needHorizontal = false;
    float columnScale = 1.0f;
    float contentWidth = availWidth;
    for (int iteration = 0; iteration < 3; ++iteration) {
        availWidth = area.w - (needVertical ? spec::kTableScrollbar : 0.0f);
        const float naturalWidth = fixedWidth + baseWidth;
        needHorizontal = naturalWidth > availWidth + 0.5f;
        if (needHorizontal) {
            columnScale = 1.0f;
            contentWidth = naturalWidth;
        } else {
            columnScale = (availWidth - fixedWidth) / baseWidth;
            contentWidth = availWidth;
        }
        bodyHeight = area.h - spec::kTableHeaderHeight -
                     (needHorizontal ? (spec::kTableScrollbar + spec::kTableScrollbarGap) : 0.0f);
        bodyHeight = std::max(0.0f, bodyHeight);
        needVertical = contentHeight > bodyHeight + 0.5f;
    }

    // ---- 滚动状态（当前窗口内按表格实例区分） ----
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID scrollKeyY = ImGui::GetID("##zzyl_table_scroll_y");
    const ImGuiID scrollKeyX = ImGui::GetID("##zzyl_table_scroll_x");
    const float maxScrollY = std::max(0.0f, contentHeight - bodyHeight);
    const float maxScrollX = std::max(0.0f, contentWidth - availWidth);
    float scrollY = std::clamp(storage->GetFloat(scrollKeyY, 0.0f), 0.0f, maxScrollY);
    float scrollX = std::clamp(storage->GetFloat(scrollKeyX, 0.0f), 0.0f, maxScrollX);

    const Rect headerRect{area.x, area.y, availWidth, spec::kTableHeaderHeight};
    const Rect bodyRect{area.x, area.y + spec::kTableHeaderHeight, availWidth, bodyHeight};

    // ---- 滚轮（仅窗口可用时） ----
    const bool pointerInBody =
        interactiveWindow && bodyRect.Contains(io.MousePos.x, io.MousePos.y) &&
        IsHovered(bodyRect);
    if (pointerInBody && io.MouseWheel != 0.0f) {
        if (io.KeyShift && needHorizontal) {
            scrollX = std::clamp(scrollX - io.MouseWheel * 60.0f, 0.0f, maxScrollX);
        } else if (needVertical) {
            scrollY = std::clamp(scrollY - io.MouseWheel * spec::kTableRowHeight, 0.0f, maxScrollY);
        } else if (needHorizontal) {
            // 无纵向滚动时，滚轮直接横向滚动（另有 Shift+滚轮 / 拖动滚动条两种方式）
            scrollX = std::clamp(scrollX - io.MouseWheel * 60.0f, 0.0f, maxScrollX);
        }
    }

    // ---- 表头 ----
    ui::Fill(drawList, headerRect, el::kHeaderBg);
    drawList->PushClipRect(ui::Min(headerRect), ui::Max(headerRect), true);
    {
        float cellX = area.x - (needHorizontal ? scrollX : 0.0f);
        if (options.showCheckbox) {
            const Rect checkCell{cellX, headerRect.y, spec::kTableCheckWidth, headerRect.h};
            bool allChecked = !rows.empty();
            bool anyChecked = false;
            for (const TableRow& row : rows) {
                if (isChecked(row.id)) {
                    anyChecked = true;
                } else {
                    allChecked = false;
                }
            }
            if (DrawCheckboxControl("##zzyl_table_check_all",
                                    Rect{checkCell.CenterX() - 10.0f, checkCell.y, 20.0f,
                                         checkCell.h},
                                    allChecked, anyChecked && !allChecked, true)) {
                for (const TableRow& row : rows) {
                    setChecked(row.id, !allChecked);
                }
            }
            cellX += spec::kTableCheckWidth;
        }
        if (options.showIndex) {
            const Rect indexCell{cellX, headerRect.y, spec::kTableIndexWidth, headerRect.h};
            {
                ui::ScopedBoldFont font(spec::kFontSmall);
                ui::TextCentered(drawList, indexCell, el::kTableHeaderText, "序号");
            }
            cellX += spec::kTableIndexWidth;
        }
        for (const TableColumn& column : columns) {
            const float cellW = std::max(1.0f, column.width * columnScale);
            const Rect cellRect{cellX, headerRect.y, cellW, headerRect.h};
            {
                ui::ScopedBoldFont font(spec::kFontSmall);
                const std::string title = Ellipsize(column.title, std::max(0.0f, cellW - 16.0f));
                ui::TextCentered(drawList, cellRect, el::kTableHeaderText, title.c_str());
            }
            cellX += cellW;
        }
        ui::Line(drawList, area.x, headerRect.Bottom() - 0.5f, area.x + availWidth,
                 headerRect.Bottom() - 0.5f, el::kBorderLight, 1.0f);
    }
    drawList->PopClipRect();

    // ---- 表体 ----
    drawList->PushClipRect(ui::Min(bodyRect), ui::Max(bodyRect), true);
    ui::Fill(drawList, bodyRect, el::kWhite);
    if (rows.empty()) {
        ui::ScopedFontSize font(spec::kFontBase);
        ui::TextCentered(drawList, bodyRect, theme::Colors().textSecondary, "暂无数据");
    } else {
        const float originX = area.x - (needHorizontal ? scrollX : 0.0f);
        const float originY = bodyRect.y - scrollY;
        const std::string emptyText;
        for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
            const TableRow& row = rows[rowIndex];
            const float rowY = originY + static_cast<float>(rowIndex) * spec::kTableRowHeight;
            const Rect rowRect{area.x, rowY, contentWidth, spec::kTableRowHeight};
            if (rowRect.Bottom() <= bodyRect.y || rowRect.y >= bodyRect.Bottom()) {
                continue;
            }
            const bool fullyVisible =
                rowRect.y >= bodyRect.y - 0.5f && rowRect.Bottom() <= bodyRect.Bottom() + 0.5f;
            const bool rowHovered = fullyVisible && pointerInBody && IsHovered(rowRect);
            if (rowHovered) {
                ui::Fill(drawList, rowRect, el::kRowHover);
            }
            ui::Line(drawList, area.x, rowRect.Bottom() - 0.5f, area.x + contentWidth,
                     rowRect.Bottom() - 0.5f, el::kBorderLight, 1.0f);

            bool childHovered = false;
            bool childClicked = false;
            float cellX = originX;
            char idBuffer[96];

            if (options.showCheckbox) {
                const Rect checkCell{cellX, rowY, spec::kTableCheckWidth, spec::kTableRowHeight};
                std::snprintf(idBuffer, sizeof(idBuffer), "##zzyl_row_check_%d",
                              static_cast<int>(rowIndex));
                const bool checked = isChecked(row.id);
                const bool checkboxPressed = DrawCheckboxControl(
                    idBuffer,
                    Rect{checkCell.CenterX() - 10.0f, checkCell.y, 20.0f, checkCell.h}, checked,
                    false, fullyVisible);
                if (fullyVisible) {
                    if (ImGui::IsItemHovered()) {
                        childHovered = true;
                    }
                    if (checkboxPressed) {
                        setChecked(row.id, !checked);
                        childClicked = true;
                    }
                }
                cellX += spec::kTableCheckWidth;
            }

            if (options.showIndex) {
                const Rect indexCell{cellX, rowY, spec::kTableIndexWidth, spec::kTableRowHeight};
                char indexText[16];
                std::snprintf(indexText, sizeof(indexText), "%d", static_cast<int>(rowIndex) + 1);
                DrawTableCellText(drawList, indexCell, indexText, spec::kFontBase, el::kTextLabel,
                                  Align::Center, 6.0f);
                cellX += spec::kTableIndexWidth;
            }

            for (std::size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
                const TableColumn& column = columns[columnIndex];
                const float cellW = std::max(1.0f, column.width * columnScale);
                const Rect cellRect{cellX, rowY, cellW, spec::kTableRowHeight};
                const std::string& text =
                    columnIndex < row.cells.size() ? row.cells[columnIndex] : emptyText;
                const bool cellHovered = fullyVisible && IsHovered(cellRect);

                switch (column.kind) {
                    case CellKind::Text: {
                        const bool truncated = DrawTableCellText(
                            drawList, cellRect, text, spec::kFontBase, el::kTextLabel, column.align,
                            8.0f);
                        if (cellHovered && truncated) {
                            ImGui::SetTooltip("%s", text.c_str());
                        }
                        break;
                    }
                    case CellKind::Image: {
                        const float imageSize =
                            std::min(spec::kTableImageSize, spec::kTableRowHeight - 4.0f);
                        const Rect imageRect = CenterIn(cellRect, imageSize, imageSize);
                        const render::Texture* texture = nullptr;
                        if (textures != nullptr && !text.empty()) {
                            // TextureCache::Get 为首次加载需要写缓存（契约传 const 指针），
                            // 这里做 const_cast；组件不向业务层写任何数据。
                            auto* mutableCache = const_cast<render::TextureCache*>(textures);
                            const render::Texture& loaded = mutableCache->Get(text.c_str());
                            if (loaded.Valid()) {
                                texture = &loaded;
                            }
                        }
                        if (texture != nullptr) {
                            render::DrawImageCover(drawList, *texture, imageRect);
                        } else {
                            ui::FillRounded(drawList, imageRect, el::kHeaderBg, 2.0f);
                            ui::ScopedFontSize font(spec::kFontTiny);
                            ui::TextCentered(drawList, imageRect, theme::Colors().textSecondary,
                                             "无图");
                        }
                        break;
                    }
                    case CellKind::StatusTag:
                        DrawStatusTagControl(cellRect, row.enabled);
                        break;
                    case CellKind::Switch: {
                        std::snprintf(idBuffer, sizeof(idBuffer), "##zzyl_row_switch_%d",
                                      static_cast<int>(rowIndex));
                        const Rect switchHit{cellRect.CenterX() - 24.0f, cellRect.y, 48.0f,
                                             spec::kTableRowHeight};
                        const bool switchPressed =
                            DrawSwitchControl(idBuffer, switchHit, row.enabled, fullyVisible);
                        if (fullyVisible) {
                            if (ImGui::IsItemHovered()) {
                                childHovered = true;
                            }
                            if (switchPressed) {
                                result.switchToggles.push_back(
                                    TableResult::SwitchToggle{row.id, !row.enabled});
                                childClicked = true;
                            }
                        }
                        break;
                    }
                    case CellKind::Actions: {
                        const std::vector<std::string> labels =
                            SplitActions(text, options.actionLabels);
                        ui::ScopedFontSize font(spec::kFontBase);
                        float totalWidth = 0.0f;
                        for (const std::string& label : labels) {
                            totalWidth += TextWidth(label.c_str());
                        }
                        totalWidth += 8.0f * static_cast<float>(labels.size() > 0 ? labels.size() - 1
                                                                                : 0);
                        float actionX = cellRect.CenterX() - totalWidth * 0.5f;
                        for (std::size_t actionIndex = 0; actionIndex < labels.size();
                             ++actionIndex) {
                            const float labelWidth = TextWidth(labels[actionIndex].c_str());
                            const Rect hitRect{actionX - 2.0f, rowY, labelWidth + 4.0f,
                                               spec::kTableRowHeight};
                            std::snprintf(idBuffer, sizeof(idBuffer),
                                          "##zzyl_row_action_%d_%d", static_cast<int>(rowIndex),
                                          static_cast<int>(actionIndex));
                            bool pressed = false;
                            bool actionHovered = false;
                            if (fullyVisible) {
                                pressed = ui::InvisibleButtonAt(idBuffer, hitRect);
                                actionHovered = ImGui::IsItemHovered();
                            }
                            if (actionHovered) {
                                childHovered = true;
                            }
                            if (pressed) {
                                result.clickedAction = static_cast<int>(actionIndex);
                                result.actionRowId = row.id;
                                childClicked = true;
                            }
                            const float textY =
                                rowY + (spec::kTableRowHeight - ImGui::GetFontSize()) * 0.5f;
                            ui::Text(drawList, ImVec2(actionX, textY), theme::Colors().primary,
                                     labels[actionIndex].c_str());
                            if (actionHovered) {
                                ui::Line(drawList, actionX, textY + ImGui::GetFontSize(),
                                         actionX + labelWidth, textY + ImGui::GetFontSize(),
                                         theme::Colors().primary, 1.0f);
                            }
                            actionX += labelWidth + 8.0f;
                        }
                        break;
                    }
                }
                cellX += cellW;
            }

            // 普通行点击（勾选/开关/操作按钮已消费点击）
            if (fullyVisible && rowHovered && !childHovered && !childClicked &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                result.clickedRow = row.id;
            }
        }
    }
    drawList->PopClipRect();

    // ---- 滚动条（独立占位；支持拖动，不覆盖行内容）----
    if (needVertical) {
        const Rect track{area.x + availWidth, bodyRect.y, spec::kTableScrollbar, bodyRect.h};
        DrawTableScrollbar(drawList, track, bodyRect.h, contentHeight, scrollY, true);
        if (DragTableScrollbar("##zzyl_table_scroll_v", track, bodyRect.h, contentHeight, scrollY,
                               true)) {
            scrollY = std::clamp(scrollY, 0.0f, maxScrollY);
        }
    }
    if (needHorizontal) {
        // 横向滚动条独立占位：绘制在内容区下方（bodyHeight 已预留 gap + 滚动条高度），
        // 不与表格行/分页重叠（BUG-S2-01）。
        const Rect track{area.x,
                         area.y + spec::kTableHeaderHeight + bodyHeight + spec::kTableScrollbarGap,
                         availWidth, spec::kTableScrollbar};
        DrawTableScrollbar(drawList, track, availWidth, contentWidth, scrollX, false);
        if (DragTableScrollbar("##zzyl_table_scroll_h", track, availWidth, contentWidth, scrollX,
                               false)) {
            scrollX = std::clamp(scrollX, 0.0f, maxScrollX);
        }
    }
    storage->SetFloat(scrollKeyY, scrollY);
    storage->SetFloat(scrollKeyX, scrollX);

    result.checkedIds = checkedIds;
    return result;
}

// =============================================================================
// 分页（§3.4）
// =============================================================================
PaginationResult DrawPagination(const Rect& area, PaginationState& state) {
    PaginationResult result;
    if (area.Empty()) {
        return result;
    }
    const int pageSize = std::max(1, state.pageSize);
    const int totalPages = std::max(1, (std::max(0, state.total) + pageSize - 1) / pageSize);
    const int oldPage = state.page;
    state.page = std::clamp(state.page, 1, totalPages);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float y = area.y + std::max(0.0f, (area.h - spec::kPaginationHeight) * 0.5f);
    const float height = spec::kPaginationHeight;
    const float buttonSize = spec::kPaginationButton;
    const float gap = spec::kPaginationGap;

    // 页码按钮列表（不超过 7 个，中间用省略号占位）
    std::vector<int> pages;
    if (totalPages <= 7) {
        for (int page = 1; page <= totalPages; ++page) {
            pages.push_back(page);
        }
    } else {
        pages.push_back(1);
        const int begin = std::max(2, state.page - 1);
        const int finish = std::min(totalPages - 1, state.page + 1);
        if (begin > 2) {
            pages.push_back(-1);
        }
        for (int page = begin; page <= finish; ++page) {
            pages.push_back(page);
        }
        if (finish < totalPages - 1) {
            pages.push_back(-2);
        }
        pages.push_back(totalPages);
    }

    char totalText[64];
    std::snprintf(totalText, sizeof(totalText), "共 %d 条", std::max(0, state.total));

    // 计算所需宽度并决定是否精简（先隐藏跳页，再压缩页码）
    bool showJump = true;
    {
        ui::ScopedFontSize font(spec::kFontBase);
        const auto measure = [&](bool jump, const std::vector<int>& pageList) {
            float width = TextWidth(totalText) + 12.0f + spec::kPaginationPageSizeWidth + 16.0f;
            width += buttonSize + gap;
            width += static_cast<float>(pageList.size()) * buttonSize;
            width += gap * static_cast<float>(pageList.size() > 0 ? pageList.size() - 1 : 0);
            width += gap + buttonSize;
            if (jump) {
                width += 10.0f + TextWidth("前往") + 8.0f + spec::kPaginationJumpWidth + 8.0f +
                         TextWidth("页");
            }
            return width;
        };
        if (measure(true, pages) > area.w) {
            showJump = false;
        }
        while (measure(showJump, pages) > area.w && pages.size() > 1) {
            pages.assign(1, state.page);
        }
    }

    // 从右向左布局
    ui::ScopedFontSize textFont(spec::kFontBase);
    float x = area.Right();
    {
        const float width = TextWidth("页");
        const Rect rect{x - width, y, width, height};
        ui::TextLeftMiddle(drawList, rect, el::kTextLabel, "页");
        x -= width;
    }
    if (showJump) {
        x -= 8.0f;
        const Rect jumpRect{x - spec::kPaginationJumpWidth, y, spec::kPaginationJumpWidth, height};
        x -= spec::kPaginationJumpWidth;
        x -= 8.0f;
        {
            const float width = TextWidth("前往");
            const Rect goRect{x - width, y, width, height};
            ui::TextLeftMiddle(drawList, goRect, el::kTextLabel, "前往");
            x -= width;
        }
        x -= 10.0f;
        static std::string jumpBuffer;
        static ImGuiID jumpOwner = 0;
        const ImGuiID jumpKey = ImGui::GetID("##zzyl_pagination_jump");
        if (jumpOwner != jumpKey) {
            jumpOwner = jumpKey;
            jumpBuffer = std::to_string(state.page);
        }
        const TextFieldResult jumpResult =
            DrawTextField("##zzyl_pagination_jump", jumpRect, jumpBuffer, "", false);
        jumpBuffer.erase(std::remove_if(jumpBuffer.begin(), jumpBuffer.end(),
                                        [](unsigned char character) {
                                            return std::isdigit(character) == 0;
                                        }),
                         jumpBuffer.end());
        if (jumpBuffer.size() > 6) {
            jumpBuffer.resize(6);
        }
        if (jumpResult.enter && !jumpBuffer.empty()) {
            const int target = std::clamp(std::atoi(jumpBuffer.c_str()), 1, totalPages);
            if (target != state.page) {
                state.page = target;
                result.changed = true;
            }
            jumpBuffer = std::to_string(state.page);
        }
    }
    {
        const Rect rect{x - buttonSize, y, buttonSize, height};
        x -= buttonSize;
        const bool enabled = state.page < totalPages;
        if (DrawFlatButton("##zzyl_pagination_next", rect, el::kWhite,
                           enabled ? el::kTextLabel : el::kDisabledText, el::kBorderInput, ">",
                           spec::kFontBase, enabled)) {
            state.page += 1;
            result.changed = true;
        }
    }
    for (std::size_t index = pages.size(); index-- > 0;) {
        x -= gap;
        const Rect rect{x - buttonSize, y, buttonSize, height};
        x -= buttonSize;
        const int number = pages[index];
        if (number < 0) {
            ui::ScopedFontSize dotsFont(spec::kFontBase);
            ui::TextCentered(drawList, rect, el::kTextLabel, "…");
            continue;
        }
        const bool current = number == state.page;
        char pageId[48];
        std::snprintf(pageId, sizeof(pageId), "##zzyl_pagination_page_%d", number);
        char numberText[16];
        std::snprintf(numberText, sizeof(numberText), "%d", number);
        if (DrawFlatButton(pageId, rect, current ? theme::Colors().primary : el::kWhite,
                           current ? theme::Colors().textInverse : el::kTextLabel,
                           current ? 0 : el::kBorderInput, numberText, spec::kFontBase, true,
                           2.0f)) {
            state.page = number;
            result.changed = true;
        }
    }
    {
        x -= gap;
        const Rect rect{x - buttonSize, y, buttonSize, height};
        x -= buttonSize;
        const bool enabled = state.page > 1;
        if (DrawFlatButton("##zzyl_pagination_prev", rect, el::kWhite,
                           enabled ? el::kTextLabel : el::kDisabledText, el::kBorderInput, "<",
                           spec::kFontBase, enabled)) {
            state.page -= 1;
            result.changed = true;
        }
    }
    {
        x -= 16.0f;
        const Rect rect{x - spec::kPaginationPageSizeWidth, y, spec::kPaginationPageSizeWidth,
                        height};
        x -= spec::kPaginationPageSizeWidth;
        char pageSizeLabel[32];
        std::snprintf(pageSizeLabel, sizeof(pageSizeLabel), "%d条/页", state.pageSize);
        std::string value = pageSizeLabel;
        static const std::vector<std::string> choices = {"10条/页", "20条/页", "30条/页",
                                                         "50条/页"};
        if (DrawSelectField("##zzyl_pagination_page_size", rect, value, choices, "10条/页")) {
            for (int choice : {10, 20, 30, 50}) {
                char label[32];
                std::snprintf(label, sizeof(label), "%d条/页", choice);
                if (value == label && choice != state.pageSize) {
                    state.pageSize = choice;
                    state.page = 1;
                    result.changed = true;
                    break;
                }
            }
        }
    }
    {
        x -= 12.0f;
        const float width = TextWidth(totalText);
        const Rect rect{x - width, y, width, height};
        ui::TextLeftMiddle(drawList, rect, el::kTextLabel, totalText);
    }
    if (oldPage != state.page) {
        result.changed = true;
    }
    return result;
}

float PaginationHeight() { return spec::kPaginationHeight; }

// =============================================================================
// 表单弹窗（§3.5）· 组件负责遮罩/标题栏/底部确定取消
// =============================================================================
bool BeginFormDialog(const Rect& pageArea, DialogState& state) {
    (void)pageArea;
    g_formDialogFrame = FormDialogFrame{};
    g_formDialogFrame.state = &state;
    if (!state.open) {
        return false;
    }
    const ImGuiIO& io = ImGui::GetIO();
    DrawDialogOverlay("##zzyl_form_dialog_overlay");

    const float width = state.width > 0.0f ? state.width : 500.0f;
    const float x = (io.DisplaySize.x - width) * 0.5f;
    const float y = io.DisplaySize.y * spec::kDialogTopRatio;
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, 4096.0f));
    PushDialogStyle();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##zzyl_form_dialog", nullptr, flags);
    g_formDialogFrame.windowOpened = true;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    // 标题栏 40 高：标题 18px + 右上关闭 ×
    {
        ui::ScopedBoldFont titleFont(spec::kFontTitle);
        ui::TextLeftMiddle(drawList,
                           Rect{windowPos.x + spec::kDialogPadding, windowPos.y + 10.0f,
                                width - spec::kDialogPadding * 2.0f - 32.0f,
                                spec::kDialogTitleHeight - 10.0f},
                           theme::Colors().textPrimary, state.title.c_str());
    }
    const Rect closeRect{windowPos.x + width - spec::kDialogPadding - 22.0f, windowPos.y + 12.0f,
                         22.0f, 22.0f};
    {
        const bool closePressed = ui::InvisibleButtonAt("##zzyl_form_dialog_close", closeRect);
        const bool closeHovered = ImGui::IsItemHovered();
        const ImU32 closeColor =
            closeHovered ? theme::Colors().textPrimary : theme::Colors().textSecondary;
        const ImVec2 center = ui::Center(closeRect);
        drawList->AddLine(ImVec2(center.x - 5.0f, center.y - 5.0f),
                          ImVec2(center.x + 5.0f, center.y + 5.0f), closeColor, 1.5f);
        drawList->AddLine(ImVec2(center.x - 5.0f, center.y + 5.0f),
                          ImVec2(center.x + 5.0f, center.y - 5.0f), closeColor, 1.5f);
        if (closePressed) {
            g_formDialogFrame.closeRequested = true;
        }
    }
    ui::Line(drawList, windowPos.x + spec::kDialogPadding,
             windowPos.y + spec::kDialogTitleHeight - 0.5f,
             windowPos.x + width - spec::kDialogPadding,
             windowPos.y + spec::kDialogTitleHeight - 0.5f, el::kBorderLight, 1.0f);
    // 内容区起点（调用方在此之后绘制表单）
    ImGui::SetCursorScreenPos(
        ImVec2(windowPos.x + spec::kDialogPadding, windowPos.y + spec::kDialogTitleHeight + 4.0f));
    return true;
}

DialogResult EndFormDialog() {
    DialogResult result = DialogResult::None;
    if (!g_formDialogFrame.windowOpened) {
        g_formDialogFrame = FormDialogFrame{};
        return result;
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const float width = windowSize.x;
    const float padX = spec::kDialogPadding;

    // 内容底部留白 + 分隔线
    const float contentBottom = ImGui::GetCursorScreenPos().y + 8.0f;
    ui::Line(drawList, windowPos.x + padX, contentBottom, windowPos.x + width - padX, contentBottom,
             el::kBorderLight, 1.0f);

    const float footerTop = contentBottom + spec::kDialogFooterGap;
    const Rect cancelRect{windowPos.x + width - padX - spec::kDialogButtonWidth, footerTop,
                          spec::kDialogButtonWidth, spec::kControlHeight};
    const Rect okRect{cancelRect.x - spec::kButtonGap - spec::kDialogButtonWidth, footerTop,
                      spec::kDialogButtonWidth, spec::kControlHeight};
    const bool okPressed = DrawFlatButton("##zzyl_form_dialog_ok", okRect, theme::Colors().primary,
                                          theme::Colors().textInverse, 0, "确定", spec::kFontBase);
    const bool cancelPressed =
        DrawFlatButton("##zzyl_form_dialog_cancel", cancelRect, el::kWhite, el::kTextLabel,
                       el::kBorderInput, "取消", spec::kFontBase);

    // 预留底部空间（AlwaysAutoResize 依赖内容尺寸）
    ImGui::SetCursorScreenPos(ImVec2(windowPos.x + padX, footerTop + spec::kControlHeight));
    ImGui::Dummy(ImVec2(1.0f, spec::kDialogBottomPadding));

    if (okPressed) {
        result = DialogResult::Ok;
    } else if (cancelPressed || g_formDialogFrame.closeRequested) {
        result = DialogResult::Cancel;
        if (g_formDialogFrame.state != nullptr) {
            g_formDialogFrame.state->open = false;
        }
    }

    ImGui::End();
    PopDialogStyle();
    g_formDialogFrame = FormDialogFrame{};
    return result;
}

// =============================================================================
// 确认框（§3.6）
// =============================================================================
ConfirmAction DrawConfirmDialog(const Rect& pageArea, bool& open, const std::string& title,
                                const std::string& message) {
    ConfirmAction result = ConfirmAction::None;
    if (!open) {
        return result;
    }
    (void)pageArea;
    const ImGuiIO& io = ImGui::GetIO();
    DrawDialogOverlay("##zzyl_confirm_overlay");

    const float width = spec::kConfirmWidth;
    const float height = spec::kConfirmHeight;
    const float x = (io.DisplaySize.x - width) * 0.5f;
    const float y = (io.DisplaySize.y - height) * 0.5f;
    const Rect card{x, y, width, height};
    if (!BeginTopCard("##zzyl_confirm_dialog", card)) {
        EndTopCard();
        return result;
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ui::FillRounded(drawList, card, el::kWhite, 4.0f);
    ui::StrokeRounded(drawList, card, el::kBorderLight, 4.0f, 1.0f);

    {
        ui::ScopedBoldFont titleFont(spec::kFontTitle);
        ui::TextLeftMiddle(drawList, Rect{card.x + 20.0f, card.y + 14.0f, card.w - 70.0f, 26.0f},
                           theme::Colors().textPrimary, title.c_str());
    }
    const Rect closeRect{card.Right() - 40.0f, card.y + 6.0f, 34.0f, 34.0f};
    bool closePressed = false;
    {
        closePressed = ui::InvisibleButtonAt("##zzyl_confirm_close", closeRect);
        const bool closeHovered = ImGui::IsItemHovered();
        const ImU32 closeColor =
            closeHovered ? theme::Colors().textPrimary : theme::Colors().textSecondary;
        const ImVec2 center = ui::Center(closeRect);
        drawList->AddLine(ImVec2(center.x - 5.0f, center.y - 5.0f),
                          ImVec2(center.x + 5.0f, center.y + 5.0f), closeColor, 1.5f);
        drawList->AddLine(ImVec2(center.x - 5.0f, center.y + 5.0f),
                          ImVec2(center.x + 5.0f, center.y - 5.0f), closeColor, 1.5f);
    }

    const float textLeft = card.x + 20.0f + 24.0f;
    {
        const ImVec2 warningCenter(card.x + 20.0f + 9.0f, card.y + 66.0f);
        drawList->AddCircleFilled(warningCenter, 9.0f, theme::Colors().warning);
        ui::ScopedBoldFont warningFont(12.0f);
        ui::TextCentered(drawList,
                         Rect{warningCenter.x - 9.0f, warningCenter.y - 9.0f, 18.0f, 18.0f},
                         el::kWhite, "!");
    }
    {
        ui::ScopedFontSize bodyFont(spec::kFontBase);
        const std::vector<std::string> lines =
            WrapText(message, std::max(60.0f, card.Right() - 24.0f - textLeft));
        float lineY = card.y + 57.0f;
        for (const std::string& line : lines) {
            ui::Text(drawList, ImVec2(textLeft, lineY), el::kTextLabel, line.c_str());
            lineY += 22.0f;
        }
    }

    const Rect cancelRect{card.Right() - 20.0f - 60.0f, card.Bottom() - 20.0f - spec::kControlHeight,
                          60.0f, spec::kControlHeight};
    const Rect okRect{cancelRect.x - spec::kButtonGap - 60.0f, cancelRect.y, 60.0f,
                      spec::kControlHeight};
    const bool okPressed = DrawFlatButton("##zzyl_confirm_ok", okRect, theme::Colors().primary,
                                          theme::Colors().textInverse, 0, "确定", spec::kFontBase);
    const bool cancelPressed =
        DrawFlatButton("##zzyl_confirm_cancel", cancelRect, el::kWhite, el::kTextLabel,
                       el::kBorderInput, "取消", spec::kFontBase);
    EndTopCard();

    if (okPressed) {
        open = false;
        result = ConfirmAction::Ok;
    } else if (cancelPressed || closePressed) {
        open = false;
        result = ConfirmAction::Cancel;
    }
    return result;
}

// =============================================================================
// Toast（§3.7）
// =============================================================================
void PushToast(ToastState& state, const std::string& text, ToastType type) {
    ToastItem item;
    item.text = text;
    item.type = type;
    item.remainingSeconds = 3.0f;
    state.items.push_back(std::move(item));
}

void DrawToasts(const Rect& windowArea, ToastState& state, float deltaTime) {
    for (std::size_t index = state.items.size(); index-- > 0;) {
        state.items[index].remainingSeconds -= deltaTime;
        if (state.items[index].remainingSeconds <= 0.0f) {
            state.items.erase(state.items.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
    if (state.items.empty() || windowArea.Empty()) {
        return;
    }
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float top = windowArea.y + spec::kToastTop;
    for (const ToastItem& item : state.items) {
        ImU32 background = el::kSuccessBg;
        ImU32 foreground = theme::Colors().success;
        const char* icon = "✓";
        switch (item.type) {
            case ToastType::Success:
                background = el::kSuccessBg;
                foreground = theme::Colors().success;
                icon = "✓";
                break;
            case ToastType::Warning:
                background = el::kWarningBg;
                foreground = theme::Colors().warning;
                icon = "!";
                break;
            case ToastType::Error:
                background = el::kDangerBg;
                foreground = theme::Colors().danger;
                icon = "×";
                break;
        }
        ui::ScopedFontSize font(spec::kFontBase);
        const float textWidth = ImGui::CalcTextSize(item.text.c_str()).x;
        const float width = std::min(windowArea.w - 32.0f, textWidth + 16.0f + 20.0f + 16.0f);
        const Rect toastRect{windowArea.CenterX() - width * 0.5f, top, width, spec::kToastHeight};
        ui::FillRounded(drawList, toastRect, background, 4.0f);
        ui::StrokeRounded(drawList, toastRect, theme::WithAlpha(foreground, 0.25f), 4.0f, 1.0f);
        const ImVec2 iconCenter(toastRect.x + 16.0f + 8.0f, toastRect.CenterY());
        drawList->AddCircleFilled(iconCenter, 8.0f, foreground);
        ui::TextCentered(drawList, Rect{iconCenter.x - 8.0f, iconCenter.y - 8.0f, 16.0f, 16.0f},
                         el::kWhite, icon);
        const std::string shown = Ellipsize(item.text, std::max(20.0f, width - 60.0f));
        ui::TextLeftMiddle(drawList,
                           Rect{toastRect.x + 16.0f + 20.0f + 8.0f, toastRect.y,
                                toastRect.w - 16.0f - 20.0f - 8.0f - 16.0f, toastRect.h},
                           foreground, shown.c_str());
        top += spec::kToastHeight + spec::kToastGap;
    }
}

}  // namespace zzyl::ui
