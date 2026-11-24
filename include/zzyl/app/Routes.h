#pragma once

// =============================================================================
// 路由表（Sprint 2）：Web 路由 path <-> C++ Route 枚举
//
// 现有页面：登录 / 首页看板 / 系统管理-用户管理 / 在住管理-房型设置 / 服务管理-护理项目。
// 其余菜单项本期不可达：TryRouteFromPath 返回 false，由外壳（dev1）提示"后续迭代接入"。
// =============================================================================

#include <string>

namespace zzyl {

enum class Route {
    Login,
    Dashboard,
    SystemUser,
    RoomType,
    NursingProject,
};

// "/system/user" -> Route::SystemUser；未收录的路径返回 false。
bool TryRouteFromPath(const std::string& path, Route& outRoute);

// Route -> 规范路径（用于 RouteState.path / 菜单高亮 / 面包屑）
const char* RoutePath(Route route);

// Route -> 页面标题（面包屑 / 标签栏）
const char* RouteTitle(Route route);

// 便捷判断：该 path 本期是否可达
bool IsRoutablePath(const std::string& path);

}  // namespace zzyl
