#pragma once

// =============================================================================
// 应用状态（ECS 资源与通用组件定义）
//
// 约定：
//  - 资源（Resource）存放全局单例数据：窗口快照 / 路由 / 会话 / 菜单 / 数据快照 /
//    命令队列 / 外壳布局 / UI 状态；
//  - 组件（Component）挂在实体上，用于页面内可复用元素的布局与渲染（AnchorLayout /
//    ScreenRect / NavItem 等）。
//  - 渲染系统只读状态；交互产生"命令"放入 CommandQueue，由 Simulation 阶段系统处理，
//    从而满足"渲染与数据分离"（页面不允许直接修改会话/路由）。
// =============================================================================

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "zzyl/core/World.h"
#include "zzyl/data/Model.h"
#include "zzyl/data/Services.h"
#include "zzyl/layout/Layout.h"

namespace zzyl {

// ---- 路由（本迭代仅登录页与首页；后续按菜单扩展） ----
enum class Route {
    Login,
    Dashboard,
};

struct RouteState {
    Route current = Route::Login;
};

// 当前登录会话（未登录时 loggedIn=false）
struct AuthSession {
    bool loggedIn = false;
    data::UserSession user;
};

// 登录页反馈（错误提示，供登录页绘制；数据由 AuthSystem 写入）
struct AuthFeedback {
    bool hasError = false;
    std::string message;
};

// 窗口快照（App 每帧写入，布局/渲染只读）
struct WindowState {
    int width = 1280;
    int height = 720;
    int framebufferWidth = 1280;
    int framebufferHeight = 720;
    float contentScale = 1.0f;
    float deltaTime = 0.0f;
    double totalTime = 0.0;
    std::uint64_t frameIndex = 0;
    bool fullscreen = false;
    bool minimized = false;
};

// UI 全局状态（仅表现层状态，不属于业务数据）
struct UiState {
    bool debugOverlay = false;
    bool sidebarCollapsed = false;
};

// 交互命令：渲染层 -> 应用层的唯一通道（每帧末清空）
struct CommandQueue {
    enum class Type {
        Login,     // arg0=用户名 arg1=密码 arg2=验证码
        Logout,
        Navigate,  // arg0=目标路由，如 "/dashboard"
    };

    struct Command {
        Type type = Type::Login;
        std::string arg0;
        std::string arg1;
        std::string arg2;
    };

    std::vector<Command> commands;
    void Clear() { commands.clear(); }
};

// 数据服务集合（数据层入口；后续替换实现即可接 MySQL / SQLite）
struct DataServices {
    std::unique_ptr<data::IAuthService> auth;
    std::unique_ptr<data::IDashboardService> dashboard;
};

// 看板数据快照资源（由 DataBootstrapSystem 加载，渲染层只读）
using DashboardData = data::DashboardSnapshot;

// ---- 布局组件 ----

// 锚定空间：没有父实体时，以 ShellLayout 的哪一块作为基准矩形
enum class AnchorSpace {
    Viewport,      // 整个窗口
    PageViewport,  // 内容区去掉 padding
    Sidebar,       // 整个侧边栏
    SidebarMenu,   // 侧边栏菜单区
    TopBar,        // 顶栏
};

// 归一化锚定：绝对矩形 = 基准矩形 * (x,y,w,h) + inset 像素内边距；
// 尺寸可再受 min/max 像素限制。父实体存在时优先用父实体的 ScreenRect。
struct AnchorLayout {
    AnchorSpace space = AnchorSpace::PageViewport;
    Entity parent = kInvalidEntity;
    float x = 0.0f;
    float y = 0.0f;
    float w = 1.0f;
    float h = 1.0f;
    layout::EdgeInsets inset;
    float minWidth = 0.0f;
    float minHeight = 0.0f;
    float maxWidth = 1.0e9f;
    float maxHeight = 1.0e9f;
};

// 每帧由 LayoutSystem 计算的绝对屏幕矩形
struct ScreenRect {
    layout::Rect rect;
};

// 侧边栏菜单项实体
struct NavItem {
    std::string title;
    std::string route;
    std::string icon;
    int index = 0;
    int depth = 0;  // 0 = 顶级
    bool active = false;
    bool hasChildren = false;
};

}  // namespace zzyl
