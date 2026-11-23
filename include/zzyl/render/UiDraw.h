#pragma once

// =============================================================================
// 绘制工具（ImDrawList 之上的薄封装）
//
// 约定：
//  - 只做"画"，不持有状态、不读业务数据；
//  - 页面/外壳系统通过本工具画底板、文字、卡片；交互控件直接用 ImGui 原生
//    （InvisibleButton/InputText/Button），避免自造控件体系。
// =============================================================================

#include "imgui.h"

#include "zzyl/layout/Layout.h"
#include "zzyl/render/Theme.h"

namespace zzyl::ui {

// ---- 坐标转换 ----
inline ImVec2 Min(const layout::Rect& rect) { return ImVec2(rect.x, rect.y); }
inline ImVec2 Max(const layout::Rect& rect) { return ImVec2(rect.x + rect.w, rect.y + rect.h); }
inline ImVec2 Center(const layout::Rect& rect) { return ImVec2(rect.CenterX(), rect.CenterY()); }

// ---- 字体 ----
// 在作用域内切换字号（ImGui 1.92 动态字体：按需光栅化，可任意字号）
class ScopedFontSize {
public:
    explicit ScopedFontSize(float pixelSize);
    ~ScopedFontSize();
    ScopedFontSize(const ScopedFontSize&) = delete;
    ScopedFontSize& operator=(const ScopedFontSize&) = delete;
};

// 在作用域内切换为粗体 + 指定字号
class ScopedBoldFont {
public:
    explicit ScopedBoldFont(float pixelSize);
    ~ScopedBoldFont();
    ScopedBoldFont(const ScopedBoldFont&) = delete;
    ScopedBoldFont& operator=(const ScopedBoldFont&) = delete;
};

float FontHeight();
ImVec2 TextSize(const char* text);

// ---- 基础图元 ----
void Fill(ImDrawList* drawList, const layout::Rect& rect, ImU32 color);
void FillRounded(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, float rounding,
                 ImDrawFlags flags = 0);
void StrokeRounded(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, float rounding,
                   float thickness = 1.0f, ImDrawFlags flags = 0);
void Line(ImDrawList* drawList, float x0, float y0, float x1, float y1, ImU32 color,
          float thickness = 1.0f);

// 白色卡片：柔和投影 + 圆角
void Card(ImDrawList* drawList, const layout::Rect& rect, float rounding);
// 页面底色
void PageBackground(ImDrawList* drawList, const layout::Rect& rect);
void VerticalGradient(ImDrawList* drawList, const layout::Rect& rect, ImU32 top, ImU32 bottom);

// ---- 文字 ----
void Text(ImDrawList* drawList, ImVec2 position, ImU32 color, const char* text);
void TextCentered(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, const char* text);
void TextLeftMiddle(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, const char* text,
                    float leftPadding = 0.0f);
void TextRightMiddle(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, const char* text,
                     float rightPadding = 0.0f);

// ---- 固定区域窗口（外壳/页面用：无标题栏/不可拖动/无背景，背景由绘制代码自绘）----
bool BeginFixedWindow(const char* id, const layout::Rect& rect);
void EndFixedWindow();

// ---- 交互 ----
// 在当前窗口中用 InvisibleButton 占位 rect（页面系统自定义 hover/click 视觉）
bool InvisibleButtonAt(const char* id, const layout::Rect& rect);

// ---- 占位面板（骨架阶段用；正式页面请勿依赖）----
void PlaceholderPanel(ImDrawList* drawList, const layout::Rect& rect, const char* title,
                      const char* note);

}  // namespace zzyl::ui
