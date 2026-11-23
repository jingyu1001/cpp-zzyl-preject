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

// cpp-dev1：应用外壳（顶栏 / 侧边栏 / 内容区 / 导航切换）
void RegisterShellModule(World& world, SystemScheduler& scheduler);

// cpp-dev2：登录页（表单 / 验证码占位 / 模拟登录 / 跳转）
void RegisterLoginModule(World& world, SystemScheduler& scheduler);

// cpp-dev3：首页看板（统计卡 / 4 个图表 / 排行列表）
void RegisterDashboardModule(World& world, SystemScheduler& scheduler);

}  // namespace zzyl
