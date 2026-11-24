#include "zzyl/app/Routes.h"

namespace zzyl {

bool TryRouteFromPath(const std::string& path, Route& outRoute) {
    if (path == "/index" || path == "/dashboard" || path.empty()) {
        outRoute = Route::Dashboard;
        return true;
    }
    if (path == "/login") {
        outRoute = Route::Login;
        return true;
    }
    if (path == "/system/user") {
        outRoute = Route::SystemUser;
        return true;
    }
    if (path == "/liveIn/houseSet") {
        outRoute = Route::RoomType;
        return true;
    }
    if (path == "/nursing/project") {
        outRoute = Route::NursingProject;
        return true;
    }
    return false;
}

const char* RoutePath(Route route) {
    switch (route) {
        case Route::Login:
            return "/login";
        case Route::Dashboard:
            return "/index";
        case Route::SystemUser:
            return "/system/user";
        case Route::RoomType:
            return "/liveIn/houseSet";
        case Route::NursingProject:
            return "/nursing/project";
    }
    return "/index";
}

const char* RouteTitle(Route route) {
    switch (route) {
        case Route::Login:
            return "登录";
        case Route::Dashboard:
            return "首页";
        case Route::SystemUser:
            return "用户管理";
        case Route::RoomType:
            return "房型设置";
        case Route::NursingProject:
            return "护理项目";
    }
    return "首页";
}

bool IsRoutablePath(const std::string& path) {
    Route ignored = Route::Dashboard;
    return TryRouteFromPath(path, ignored);
}

}  // namespace zzyl
