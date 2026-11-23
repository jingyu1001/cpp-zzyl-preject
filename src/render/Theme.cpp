#include "zzyl/render/Theme.h"

namespace zzyl::theme {
namespace {

Palette g_palette;
Fonts g_fonts;

}  // namespace

Palette& Colors() { return g_palette; }
Fonts& F() { return g_fonts; }

void SetFonts(ImFont* regular, ImFont* bold) {
    g_fonts.regular = regular;
    g_fonts.bold = bold;
}

ImU32 WithAlpha(ImU32 color, float alpha) {
    const int a = static_cast<int>(alpha * 255.0f + 0.5f);
    return (color & 0x00FFFFFFu) | (static_cast<ImU32>(a < 0 ? 0 : (a > 255 ? 255 : a)) << IM_COL32_A_SHIFT);
}

void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.ScrollbarSize = 6.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 0.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    const Palette& p = g_palette;
    style.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(p.textPrimary);
    style.Colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(p.textMuted);
    style.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(p.panel);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(p.panel);
    style.Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(p.border);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(1, 1, 1, 1);
    style.Colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(p.panelHover);
    style.Colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(p.panelHover);
    style.Colors[ImGuiCol_TitleBg] = ImGui::ColorConvertU32ToFloat4(p.panel);
    style.Colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(p.panel);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.945f, 0.945f, 0.945f, 1.0f);  // #F1F1F1
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.753f, 0.753f, 0.753f, 1.0f);  // #C0C0C0
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.68f, 0.68f, 0.68f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(p.primary);
    style.Colors[ImGuiCol_SliderGrab] = ImGui::ColorConvertU32ToFloat4(p.primary);
    style.Colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(p.primary);
    style.Colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(p.primaryHover);
    style.Colors[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(p.primaryActive);
    style.Colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(p.panelHover);
    style.Colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(p.panelHover);
    style.Colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(p.panelHover);
    style.Colors[ImGuiCol_Separator] = ImGui::ColorConvertU32ToFloat4(p.border);
    style.Colors[ImGuiCol_TextSelectedBg] = ImGui::ColorConvertU32ToFloat4(WithAlpha(p.primary, 0.35f));
    style.Colors[ImGuiCol_NavCursor] = ImGui::ColorConvertU32ToFloat4(WithAlpha(p.primary, 0.60f));
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.45f);
}

}  // namespace zzyl::theme
