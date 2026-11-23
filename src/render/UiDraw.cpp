#include "zzyl/render/UiDraw.h"

#include <algorithm>
#include <cmath>

namespace zzyl::ui {
namespace {

ImFont* CurrentFont() { return ImGui::GetFont(); }

}  // namespace

ScopedFontSize::ScopedFontSize(float pixelSize) { ImGui::PushFont(nullptr, pixelSize); }
ScopedFontSize::~ScopedFontSize() { ImGui::PopFont(); }

ScopedBoldFont::ScopedBoldFont(float pixelSize) {
    ImFont* bold = theme::F().bold != nullptr ? theme::F().bold : ImGui::GetFont();
    ImGui::PushFont(bold, pixelSize);
}
ScopedBoldFont::~ScopedBoldFont() { ImGui::PopFont(); }

float FontHeight() { return ImGui::GetFontSize(); }

ImVec2 TextSize(const char* text) { return ImGui::CalcTextSize(text); }

void Fill(ImDrawList* drawList, const layout::Rect& rect, ImU32 color) {
    if (drawList == nullptr || rect.Empty()) {
        return;
    }
    drawList->AddRectFilled(Min(rect), Max(rect), color);
}

void FillRounded(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, float rounding,
                 ImDrawFlags flags) {
    if (drawList == nullptr || rect.Empty()) {
        return;
    }
    drawList->AddRectFilled(Min(rect), Max(rect), color, rounding, flags);
}

void StrokeRounded(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, float rounding,
                   float thickness, ImDrawFlags flags) {
    if (drawList == nullptr || rect.Empty()) {
        return;
    }
    drawList->AddRect(Min(rect), Max(rect), color, rounding, flags, thickness);
}

void Line(ImDrawList* drawList, float x0, float y0, float x1, float y1, ImU32 color, float thickness) {
    if (drawList == nullptr) {
        return;
    }
    drawList->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), color, thickness);
}

void Card(ImDrawList* drawList, const layout::Rect& rect, float rounding) {
    if (drawList == nullptr || rect.Empty()) {
        return;
    }
    // 投影层：Web 为 rgba(0,0,0,0.03) 0 2px 4px；白底上叠两层极浅阴影保证可辨识
    layout::Rect shadowFar = rect;
    shadowFar.x -= 2.0f;
    shadowFar.y += 2.0f;
    shadowFar.w += 4.0f;
    shadowFar.h += 4.0f;
    drawList->AddRectFilled(Min(shadowFar), Max(shadowFar), theme::WithAlpha(IM_COL32(0, 0, 0, 255), 0.025f),
                            rounding + 2.0f);
    layout::Rect shadowNear = rect;
    shadowNear.y += 1.0f;
    shadowNear.h += 1.5f;
    drawList->AddRectFilled(Min(shadowNear), Max(shadowNear),
                            theme::WithAlpha(IM_COL32(0, 0, 0, 255), 0.045f), rounding + 1.0f);
    FillRounded(drawList, rect, theme::Colors().panel, rounding);
}

void PageBackground(ImDrawList* drawList, const layout::Rect& rect) {
    Fill(drawList, rect, theme::Colors().background);
}

void VerticalGradient(ImDrawList* drawList, const layout::Rect& rect, ImU32 top, ImU32 bottom) {
    if (drawList == nullptr || rect.Empty()) {
        return;
    }
    drawList->AddRectFilledMultiColor(Min(rect), Max(rect), top, top, bottom, bottom);
}

void Text(ImDrawList* drawList, ImVec2 position, ImU32 color, const char* text) {
    if (drawList == nullptr || text == nullptr) {
        return;
    }
    drawList->AddText(position, color, text);
}

void TextCentered(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, const char* text) {
    if (drawList == nullptr || text == nullptr) {
        return;
    }
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 position(rect.x + (rect.w - size.x) * 0.5f, rect.y + (rect.h - size.y) * 0.5f);
    drawList->AddText(position, color, text);
}

void TextLeftMiddle(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, const char* text,
                    float leftPadding) {
    if (drawList == nullptr || text == nullptr) {
        return;
    }
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 position(rect.x + leftPadding, rect.y + (rect.h - size.y) * 0.5f);
    drawList->AddText(position, color, text);
}

void TextRightMiddle(ImDrawList* drawList, const layout::Rect& rect, ImU32 color, const char* text,
                     float rightPadding) {
    if (drawList == nullptr || text == nullptr) {
        return;
    }
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 position(rect.x + rect.w - rightPadding - size.x, rect.y + (rect.h - size.y) * 0.5f);
    drawList->AddText(position, color, text);
}

bool BeginFixedWindow(const char* id, const layout::Rect& rect) {
    ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y));
    ImGui::SetNextWindowSize(ImVec2(std::max(1.0f, rect.w), std::max(1.0f, rect.h)));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoFocusOnAppearing;
    return ImGui::Begin(id, nullptr, flags);
}

void EndFixedWindow() { ImGui::End(); }

bool InvisibleButtonAt(const char* id, const layout::Rect& rect) {
    ImGui::SetCursorScreenPos(Min(rect));
    return ImGui::InvisibleButton(id, ImVec2(std::max(1.0f, rect.w), std::max(1.0f, rect.h)));
}

void PlaceholderPanel(ImDrawList* drawList, const layout::Rect& rect, const char* title,
                      const char* note) {
    if (drawList == nullptr || rect.Empty()) {
        return;
    }
    const float rounding = 4.0f;  // 卡片圆角：Web UI 规格 4px
    Card(drawList, rect, rounding);

    // 限制占位文字在卡片内，避免窄卡片时文字溢出
    drawList->PushClipRect(Min(rect), Max(rect), true);

    {
        ScopedBoldFont titleFont(16.0f);
        TextLeftMiddle(drawList, layout::Rect{rect.x + 20.0f, rect.y + 12.0f, rect.w - 40.0f, 24.0f},
                       theme::Colors().textPrimary, title);
    }

    const layout::Rect noteRect{rect.x + 20.0f, rect.y + rect.h * 0.40f, rect.w - 40.0f,
                                rect.h * 0.34f};
    {
        ScopedFontSize noteFont(13.0f);
        TextCentered(drawList, noteRect, theme::Colors().textMuted, note);
    }
    drawList->PopClipRect();

    // 虚线感的占位边框（四角短线）
    const ImU32 dashColor = theme::WithAlpha(theme::Colors().primary, 0.35f);
    const float x0 = rect.x + 8.0f;
    const float y0 = rect.y + 8.0f;
    const float x1 = rect.Right() - 8.0f;
    const float y1 = rect.Bottom() - 8.0f;
    const float len = 14.0f;
    Line(drawList, x0, y0, x0 + len, y0, dashColor, rounding);
    Line(drawList, x0, y0, x0, y0 + len, dashColor, rounding);
    Line(drawList, x1, y0, x1 - len, y0, dashColor, rounding);
    Line(drawList, x1, y0, x1, y0 + len, dashColor, rounding);
    Line(drawList, x0, y1, x0 + len, y1, dashColor, rounding);
    Line(drawList, x0, y1, x0, y1 - len, dashColor, rounding);
    Line(drawList, x1, y1, x1 - len, y1, dashColor, rounding);
    Line(drawList, x1, y1, x1, y1 - len, dashColor, rounding);
}

}  // namespace zzyl::ui
