#pragma once

// =============================================================================
// 静态菜单目录（本迭代为静态版；后续由后端 /getRouters 接口动态下发）
// 数据来源：Web 端 src/views 目录 + RuoYi 菜单约定（zznursing 项目实际菜单）。
// 菜单是"数据"，渲染在外壳系统（cpp-dev1）中完成。
// =============================================================================

#include <string>
#include <vector>

namespace zzyl::data {

struct MenuEntry {
    std::string title;   // 显示名（中文）
    std::string icon;    // 图标键（home/nursing/system/... 渲染层自行解释）
    std::string route;   // 路由（/dashboard、/nursing/floor ...）
    std::vector<MenuEntry> children;

    bool HasChildren() const { return !children.empty(); }
};

struct MenuCatalog {
    std::vector<MenuEntry> roots;
};

// 与 Web 端菜单结构对齐的静态菜单
MenuCatalog BuildDefaultMenuCatalog();

}  // namespace zzyl::data
