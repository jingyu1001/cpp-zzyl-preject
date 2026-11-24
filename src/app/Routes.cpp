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
    if (path == "/enterQuit/checkIn") {
        outRoute = Route::CheckInList;
        return true;
    }
    if (path == "/enterQuit/checkInForm") {
        outRoute = Route::CheckInForm;
        return true;
    }
    if (path == "/enterQuit/checkInInfo") {
        outRoute = Route::CheckInDetail;
        return true;
    }
    if (path == "/nursing/plan") {
        outRoute = Route::NursingPlan;
        return true;
    }
    if (path == "/nursing/level") {
        outRoute = Route::NursingLevel;
        return true;
    }
    if (path == "/liveIn/floor") {
        outRoute = Route::BedPreview;
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
        case Route::CheckInList:
            return "/enterQuit/checkIn";
        case Route::CheckInForm:
            return "/enterQuit/checkInForm";
        case Route::CheckInDetail:
            return "/enterQuit/checkInInfo";
        case Route::NursingPlan:
            return "/nursing/plan";
        case Route::NursingLevel:
            return "/nursing/level";
        case Route::BedPreview:
            return "/liveIn/floor";
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
        case Route::CheckInList:
            return "入住办理";
        case Route::CheckInForm:
            return "办理入住";
        case Route::CheckInDetail:
            return "入住详情";
        case Route::NursingPlan:
            return "护理计划";
        case Route::NursingLevel:
            return "护理等级";
        case Route::BedPreview:
            return "床位预览";
    }
    return "首页";
}

bool IsRoutablePath(const std::string& path) {
    Route ignored = Route::Dashboard;
    return TryRouteFromPath(path, ignored);
}

}  // namespace zzyl
