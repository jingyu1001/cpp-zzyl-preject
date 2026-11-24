#pragma once

#include "zzyl/core/SystemScheduler.h"
#include "zzyl/core/World.h"

namespace zzyl {

// 注册所有资源 / 应用层系统 / 三个业务模块（外壳、登录页、看板页）
// emptyData=true 时业务种子服务返回空集合（仅用于 --empty-data 空态测试钩子）
void RegisterAllModules(World& world, SystemScheduler& scheduler, bool emptyData = false);

}  // namespace zzyl
