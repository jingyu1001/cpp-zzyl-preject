#include "zzyl/app/ModuleRegistry.h"

#include <memory>

#include "zzyl/app/AppState.h"
#include "zzyl/app/ModuleHooks.h"
#include "zzyl/app/Systems.h"
#include "zzyl/data/MenuCatalog.h"
#include "zzyl/data/MockServices.h"

namespace zzyl {

void RegisterAllModules(World& world, SystemScheduler& scheduler) {
    // ---- 数据服务（数据层入口；后续换 MySQL / SQLite 实现只改这里） ----
    DataServices services;
    services.auth = std::make_unique<data::MockAuthService>();
    services.dashboard = std::make_unique<data::MockDashboardService>();
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

    // ---- 应用层系统 ----
    scheduler.Add(CreateRouteSystem(), 0);          // Simulation
    scheduler.Add(CreateDataBootstrapSystem(), 10); // Simulation（启动时加载看板 Mock 数据）
    scheduler.Add(CreateLayoutSystem(), 0);         // Layout

    // ---- 业务模块（按文件归属分别由 cpp-dev1/2/3 维护） ----
    RegisterShellModule(world, scheduler);
    RegisterLoginModule(world, scheduler);
    RegisterDashboardModule(world, scheduler);
}

}  // namespace zzyl
