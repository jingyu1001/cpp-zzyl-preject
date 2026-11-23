#pragma once

// =============================================================================
// 主题（配色 + ImGui 样式 + 字体句柄）
//
// 依据 Web 端《UI复刻规格_Cpp_1280x720_v0.1.md》§2 与
// 《Web架构提取_v0.1.md》§8：
//  - 品牌主色 #00B8A0（按钮/选中/高亮），链接色 #008D71
//  - 侧边栏 #304156，二级菜单 #1F2D3D，菜单文字 #BFCBD9，选中文字 #00B8A0（无底色块）
//  - 页面/卡片白底，文字 #303133/#262626/#595959/#999999
//  - 图表色板：折线 #4D8DFF/#00B8A0；环形轨道 #D7F1ED；等级 4 色；年龄男/女
// 渲染层只使用主题常量，不出现散落硬编码色值。
// =============================================================================

#include "imgui.h"

struct ImFont;

namespace zzyl::theme {

struct Palette {
    // 品牌
    ImU32 primary = IM_COL32(0x00, 0xB8, 0xA0, 0xFF);        // #00B8A0
    ImU32 primaryHover = IM_COL32(0x33, 0xC6, 0xB3, 0xFF);   // #33C6B3
    ImU32 primaryActive = IM_COL32(0x00, 0x97, 0x7F, 0xFF);  // #00977F
    ImU32 link = IM_COL32(0x00, 0x8D, 0x71, 0xFF);           // #008D71

    // 页面 / 面板（Web 看板页面为白底，卡片白底 + 极浅阴影）
    ImU32 background = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
    ImU32 panel = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
    ImU32 panelHover = IM_COL32(0xF3, 0xF8, 0xF9, 0xFF);     // 浅灰块 #F3F8F9
    ImU32 lightBlock = IM_COL32(0xF3, 0xF8, 0xF9, 0xFF);     // 日期框/预约行

    // 侧边栏（RuoYi 深色主题）
    ImU32 sidebar = IM_COL32(0x30, 0x41, 0x56, 0xFF);        // #304156
    ImU32 sidebarLogo = IM_COL32(0x2B, 0x2F, 0x3A, 0xFF);    // Logo 区
    ImU32 sidebarSub = IM_COL32(0x1F, 0x2D, 0x3D, 0xFF);     // 二级菜单 #1F2D3D
    ImU32 sidebarText = IM_COL32(0xBF, 0xCB, 0xD9, 0xFF);    // #BFCBD9
    ImU32 sidebarTextActive = IM_COL32(0x00, 0xB8, 0xA0, 0xFF);  // 选中文字 #00B8A0
    ImU32 sidebarHover = IM_COL32(0x26, 0x34, 0x45, 0xFF);   // #263445

    // 顶栏 / 标签栏
    ImU32 topbar = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
    ImU32 tagsView = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
    ImU32 tagsViewBorder = IM_COL32(0xD8, 0xDC, 0xE5, 0xFF);

    // 文字
    ImU32 textPrimary = IM_COL32(0x30, 0x31, 0x33, 0xFF);    // #303133
    ImU32 textTitle = IM_COL32(0x26, 0x26, 0x26, 0xFF);      // #262626
    ImU32 textRegular = IM_COL32(0x59, 0x59, 0x59, 0xFF);    // #595959
    ImU32 textSecondary = IM_COL32(0x90, 0x93, 0x99, 0xFF);  // #909399
    ImU32 textMuted = IM_COL32(0x99, 0x99, 0x99, 0xFF);      // #999999
    ImU32 textPlaceholder = IM_COL32(0xBF, 0xBF, 0xBF, 0xFF);  // #BFBFBF
    ImU32 textInverse = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
    ImU32 breadcrumbCurrent = IM_COL32(0x97, 0xA8, 0xBE, 0xFF);
    ImU32 iconRegular = IM_COL32(0x5A, 0x5E, 0x66, 0xFF);

    // 语义色
    ImU32 success = IM_COL32(0x67, 0xC2, 0x3A, 0xFF);
    ImU32 warning = IM_COL32(0xE6, 0xA2, 0x3C, 0xFF);
    ImU32 danger = IM_COL32(0xF5, 0x6C, 0x6C, 0xFF);
    ImU32 info = IM_COL32(0x90, 0x93, 0x99, 0xFF);

    // 边框 / 坐标轴
    ImU32 border = IM_COL32(0xE8, 0xE8, 0xE8, 0xFF);
    ImU32 borderLight = IM_COL32(0xF0, 0xF0, 0xF0, 0xFF);
    ImU32 borderAxis = IM_COL32(0xDC, 0xDC, 0xDC, 0xFF);

    // 图表色板
    ImU32 chartLineBlue = IM_COL32(0x4D, 0x8D, 0xFF, 0xFF);   // 收入/入住
    ImU32 chartLineTeal = IM_COL32(0x00, 0xB8, 0xA0, 0xFF);   // 退款/退住/服务
    ImU32 ringTrack = IM_COL32(0xD7, 0xF1, 0xED, 0xFF);       // 环形浅色轨道
    ImU32 ringMain[5] = {
        IM_COL32(0x46, 0xCD, 0xBC, 0xFF),  // 老人 #46CDBC
        IM_COL32(0x68, 0x9F, 0xFF, 0xFF),  // 床位 #689FFF
        IM_COL32(0xAD, 0xA5, 0xEE, 0xFF),  // 服务单 #ADA5EE
        IM_COL32(0xFF, 0xB0, 0x56, 0xFF),  // 员工 #FFB056
        IM_COL32(0xFE, 0x85, 0x85, 0xFF),  // 收入 #FE8585
    };
    ImU32 elderLevel[4] = {
        IM_COL32(0x47, 0xCB, 0xBA, 0xFF),  // 特级 #47CBBA
        IM_COL32(0x98, 0x91, 0xDF, 0xFF),  // 一级 #9891DF
        IM_COL32(0x8E, 0xBE, 0xF3, 0xFF),  // 二级 #8EBEF3
        IM_COL32(0xFF, 0xB0, 0x57, 0xFF),  // 三级 #FFB057
    };
    ImU32 ageMale = IM_COL32(0x27, 0xC2, 0xAE, 0xFF);    // #27C2AE
    ImU32 ageFemale = IM_COL32(0xFE, 0x6E, 0x6E, 0xFF);  // #FE6E6E
    ImU32 tagVisit = IM_COL32(0x27, 0xBA, 0x9B, 0xFF);   // 参观
    ImU32 tagReserve = IM_COL32(0xFE, 0x6E, 0x6E, 0xFF);  // 预约

    // 通用图表色板（Web getChartListColor）
    ImU32 chartPalette[8] = {
        IM_COL32(0xD7, 0xF1, 0xED, 0xFF), IM_COL32(0x46, 0xCD, 0xBC, 0xFF),
        IM_COL32(0x00, 0xA8, 0x70, 0xFF), IM_COL32(0xEB, 0xB1, 0x05, 0xFF),
        IM_COL32(0xED, 0x7B, 0x2F, 0xFF), IM_COL32(0xE3, 0x4D, 0x59, 0xFF),
        IM_COL32(0xED, 0x49, 0xB4, 0xFF), IM_COL32(0x83, 0x4E, 0xC2, 0xFF),
    };
};

struct Fonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
};

Palette& Colors();
Fonts& F();
void SetFonts(ImFont* regular, ImFont* bold);

// 应用全局 ImGui 样式（由 ImGuiLayer::Init 调用一次）
void ApplyStyle();

// 颜色工具
ImU32 WithAlpha(ImU32 color, float alpha);

}  // namespace zzyl::theme
