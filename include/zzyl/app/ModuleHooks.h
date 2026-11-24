#pragma once

// =============================================================================
// 模块注册钩子（3 名程序员的扩展点）
//
// App 启动时调用下面 3 个函数，各模块在自己的 .cpp 里向 World 注册实体/组件、
// 向 SystemScheduler 注册系统。程序员只改自己模块的实现文件，不需要碰 App.cpp，
// 因此 3 条任务线互不冲突。
// =============================================================================

#include "zzyl/core/SystemScheduler.h"
#include "zzyl/core/World.h"

namespace zzyl {

// cpp-dev1：应用外壳（顶栏 / 侧边栏 / 内容区 / 导航切换）+ 通用列表页组件（Sprint 2）
void RegisterShellModule(World& world, SystemScheduler& scheduler);

// cpp-dev2：登录页（Sprint 1）；Sprint 2：系统管理-用户管理页
void RegisterLoginModule(World& world, SystemScheduler& scheduler);
void RegisterSystemUserModule(World& world, SystemScheduler& scheduler);

// cpp-dev3：首页看板（Sprint 1）；Sprint 2：在住管理-房型设置页 + 服务管理-护理项目页
void RegisterDashboardModule(World& world, SystemScheduler& scheduler);
void RegisterRoomTypeModule(World& world, SystemScheduler& scheduler);
void RegisterNursingProjectModule(World& world, SystemScheduler& scheduler);

}  // namespace zzyl
