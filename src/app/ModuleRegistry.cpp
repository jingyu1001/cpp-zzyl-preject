#include "zzyl/app/ModuleRegistry.h"

#include <cstdio>
#include <memory>
#include <string>

#include "zzyl/app/AppState.h"
#include "zzyl/app/ModuleHooks.h"
#include "zzyl/app/Systems.h"
#include "zzyl/data/MenuCatalog.h"
#include "zzyl/data/MockServices.h"
#include "zzyl/data/SeedServices.h"
#include "zzyl/data/SqliteData.h"
#include "zzyl/render/Texture.h"
#include "zzyl/ui/Widgets.h"

namespace zzyl {

void RegisterAllModules(World& world, SystemScheduler& scheduler, bool emptyData,
                        const std::string& dbPath) {
    // ---- 数据服务（Sprint 3：SQLite 持久化；--empty-data 走内存空集合） ----
    DataServices services;
    services.auth = std::make_unique<data::MockAuthService>();
    services.dashboard = std::make_unique<data::MockDashboardService>();

    bool sqliteActive = false;
    std::string activeDbPath;
    if (emptyData) {
        // 空态测试钩子：内存服务 + 空集合（不写库，行为与 Sprint 2 一致）
        services.user = std::make_unique<data::SeedUserService>(true);
        services.roomType = std::make_unique<data::SeedRoomTypeService>(true);
        services.nursingProject = std::make_unique<data::SeedNursingProjectService>(true);
        services.checkIn = std::make_unique<data::SeedCheckInService>(true);
        std::printf("[Data] 空数据模式（--empty-data）：使用内存空集合，不打开 SQLite\n");
    } else {
        auto store = std::make_shared<data::SqliteStore>();
        const std::string path = dbPath.empty() ? data::SqliteStore::DefaultPath() : dbPath;
        std::string dbError;
        if (store->Open(path, true, &dbError)) {
            sqliteActive = true;
            activeDbPath = store->Path();
            world.EmplaceResource<std::shared_ptr<data::SqliteStore>>(store);
            services.user = std::make_unique<data::SqliteUserService>(store.get());
            services.roomType = std::make_unique<data::SqliteRoomTypeService>(store.get());
            services.nursingProject = std::make_unique<data::SqliteNursingProjectService>(store.get());
            services.checkIn = std::make_unique<data::SqliteCheckInService>(store.get());
            std::printf("[Data] SQLite 已启用：%s\n", activeDbPath.c_str());
        } else {
            std::fprintf(stderr,
                         "[Data] SQLite 打开失败（%s），回退内存种子数据（不持久化）：%s\n",
                         path.c_str(), dbError.c_str());
            services.user = std::make_unique<data::SeedUserService>(false);
            services.roomType = std::make_unique<data::SeedRoomTypeService>(false);
            services.nursingProject = std::make_unique<data::SeedNursingProjectService>(false);
            services.checkIn = std::make_unique<data::SeedCheckInService>(false);
        }
    }
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
    world.EmplaceResource<render::TextureCache>();
    world.EmplaceResource<ui::ToastState>();
    world.EmplaceResource<CheckInSelection>();  // M4 列表→详情 选中记录

    // 数据源统计（启动日志，便于验证 SQLite 种子/持久化）
    if (auto* svc = world.TryResource<DataServices>(); svc != nullptr) {
        const std::size_t userCount = svc->user ? svc->user->List(data::UserQuery{}).size() : 0;
        const std::size_t roomCount =
            svc->roomType ? svc->roomType->List(data::RoomTypeQuery{}).size() : 0;
        const std::size_t projectCount =
            svc->nursingProject ? svc->nursingProject->List(data::NursingProjectQuery{}).size() : 0;
        const std::size_t checkInCount =
            svc->checkIn ? svc->checkIn->List(data::CheckInQuery{}).size() : 0;
        std::printf("[Data] 数据源=%s | 用户=%zu 房型=%zu 护理项目=%zu 入住办理=%zu\n",
                    sqliteActive ? "SQLite" : (emptyData ? "内存(空)" : "内存种子"), userCount,
                    roomCount, projectCount, checkInCount);
        // M4 v4 迁移自检：样例记录（字段/家属/护理等级字典）
        if (svc->checkIn != nullptr) {
            const auto checkIns = svc->checkIn->List(data::CheckInQuery{});
            const auto levels = svc->checkIn->NursingLevels();
            if (!checkIns.empty()) {
                const data::CheckInRecord& sample = checkIns.front();
                std::printf(
                    "[Data] 入住样例：id=%d 姓名=%s 床位=%s 等级=%s 状态=%d 家属=%zu | 护理等级字典=%zu 条\n",
                    sample.id, sample.elderName.c_str(), sample.bedNumber.c_str(),
                    sample.nursingLevelName.c_str(), sample.status, sample.families.size(),
                    levels.size());
            }
        }
    }

    // ---- 应用层系统 ----
    scheduler.Add(CreateRouteSystem(), 0);          // Simulation
    scheduler.Add(CreateDataBootstrapSystem(), 10); // Simulation（看板 Mock 数据）
    scheduler.Add(CreateLayoutSystem(), 0);         // Layout

    // ---- 业务模块 ----
    RegisterShellModule(world, scheduler);            // dev1：外壳 + 通用组件
    RegisterLoginModule(world, scheduler);            // dev2：登录页
    RegisterSystemUserModule(world, scheduler);       // dev2：用户管理
    RegisterDashboardModule(world, scheduler);        // dev3：首页看板
    RegisterRoomTypeModule(world, scheduler);         // dev3：房型设置
    RegisterNursingProjectModule(world, scheduler);   // dev3：护理项目
    RegisterCheckInListModule(world, scheduler);      // dev1：M4 入住办理列表
    RegisterCheckInFormModule(world, scheduler);      // dev2：M4 办理入住表单
    RegisterCheckInDetailModule(world, scheduler);    // dev2：M4 入住详情
}

}  // namespace zzyl
