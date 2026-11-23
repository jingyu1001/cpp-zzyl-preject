#pragma once

// =============================================================================
// 应用层系统（core / 应用层负责，不分配给业务模块）
//  - LayoutSystem：由窗口尺寸重算 ShellLayout + 解算所有 AnchorLayout
//  - RouteSystem：消费 CommandQueue 中的 Navigate 命令，切换路由
//  - DataBootstrapSystem：启动时通过数据服务加载看板快照到资源
// =============================================================================

#include <memory>

#include "zzyl/core/System.h"

namespace zzyl {

std::unique_ptr<ISystem> CreateLayoutSystem();
std::unique_ptr<ISystem> CreateRouteSystem();
std::unique_ptr<ISystem> CreateDataBootstrapSystem();

}  // namespace zzyl
