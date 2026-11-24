#pragma once

#include <string>

#include "zzyl/core/SystemScheduler.h"
#include "zzyl/core/World.h"

namespace zzyl {

// 注册所有资源 / 应用层系统 / 业务模块
// emptyData=true 时业务服务返回空集合（仅 --empty-data 空态测试钩子）
// dbPath 为空时使用 <exe 目录>/data/zzyl.db；非空（--db / ZZYL_DB_PATH）时覆盖
void RegisterAllModules(World& world, SystemScheduler& scheduler, bool emptyData = false,
                        const std::string& dbPath = "");

}  // namespace zzyl
