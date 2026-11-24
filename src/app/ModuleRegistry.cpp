#include "zzyl/app/ModuleRegistry.h"

#include <memory>

#include "zzyl/app/AppState.h"
#include "zzyl/app/ModuleHooks.h"
#include "zzyl/app/Systems.h"
#include "zzyl/data/MenuCatalog.h"
#include "zzyl/data/MockServices.h"
#include "zzyl/data/SeedServices.h"
#include "zzyl/render/Texture.h"
#include "zzyl/ui/Widgets.h"

namespace zzyl {

void RegisterAllModules(World& world, SystemScheduler& scheduler) {
    // ---- 数据服务（数据层入口；后续换 MySQL / SQLite 实现只改这里） ----
    DataServices services;
    services.auth = std::make_unique<data::MockAuthService>();
    services.dashboard = std::make_unique<data::MockDashboardService>();
    services.user = std::make_unique<data::SeedUserService>();
    services.roomType = std::make_unique<data::SeedRoomTypeService>();
    services.nursingProject = std::make_unique<data::SeedNursingProjectService>();
    world.EmplaceResource<DataServices>(std::move(services));

    // ---- 全局资源 ----
    world.EmplaceResource<RouteState>();
    world.EmplaceResource<AuthSession>();
    world.EmplaceResource<AuthFeedback>();
    world.EmplaceResource<WindowState>();
    world.EmplaceResource<UiState>();
    world.EmplaceResource<CommandQueue>();
    world.EmplaceResource<data::MenuCatalog>(data::BuildDefaultMenuCatalog());
    world.EmplaceResource<DashboardData>();
    world.EmplaceResource<layout::ShellLayout>();
    // Sprint 2 新增资源
    world.EmplaceResource<render::TextureCache>();  // WIC 图片纹理缓存
    world.EmplaceResource<ui::ToastState>();        // 全局 Toast（顶部居中消息提示）

    // ---- 应用层系统 ----
    scheduler.Add(CreateRouteSystem(), 0);          // Simulation
    scheduler.Add(CreateDataBootstrapSystem(), 10); // Simulation（启动时加载看板 Mock 数据）
    scheduler.Add(CreateLayoutSystem(), 0);         // Layout

    // ---- 业务模块（按文件归属分别由 cpp-dev1/2/3 维护） ----
    RegisterShellModule(world, scheduler);            // dev1：外壳 + 通用组件
    RegisterLoginModule(world, scheduler);            // dev2：登录页
    RegisterSystemUserModule(world, scheduler);       // dev2：用户管理（Sprint 2）
    RegisterDashboardModule(world, scheduler);        // dev3：首页看板
    RegisterRoomTypeModule(world, scheduler);         // dev3：房型设置（Sprint 2）
    RegisterNursingProjectModule(world, scheduler);   // dev3：护理项目（Sprint 2）
}

}  // namespace zzyl
