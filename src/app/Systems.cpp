#include "zzyl/app/Systems.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "zzyl/app/AppState.h"
#include "zzyl/app/Routes.h"

namespace zzyl {
namespace {

layout::Rect SpaceRect(const layout::ShellLayout& shell, AnchorSpace space) {
    switch (space) {
        case AnchorSpace::Viewport:
            return shell.viewport;
        case AnchorSpace::PageViewport:
            return shell.pageViewport;
        case AnchorSpace::Sidebar:
            return shell.sidebar;
        case AnchorSpace::SidebarMenu:
            return shell.sidebarMenu;
        case AnchorSpace::TopBar:
            return shell.topbar;
    }
    return shell.pageViewport;
}

// -----------------------------------------------------------------------------
// LayoutSystem：响应式布局解算
// 1) 由 WindowState 重算 ShellLayout（比例 + 上下限，无写死像素）；
// 2) 解算所有 AnchorLayout 实体 -> ScreenRect（按父链深度排序，父先子后）。
// -----------------------------------------------------------------------------
class LayoutSystem final : public ISystem {
public:
    const char* Name() const override { return "LayoutSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Layout; }

    void Update(World& world, const FrameContext& /*frame*/) override {
        const WindowState* window = world.TryResource<WindowState>();
        if (window == nullptr || window->width <= 0 || window->height <= 0) {
            return;
        }
        const UiState* ui = world.TryResource<UiState>();
        const bool collapsed = ui != nullptr && ui->sidebarCollapsed;

        const layout::ShellLayout shell = layout::ComputeShellLayout(
            static_cast<float>(window->width), static_cast<float>(window->height),
            window->contentScale, collapsed);
        world.SetResource(shell);

        // 收集锚定实体
        std::vector<Entity> entities;
        std::unordered_map<Entity, AnchorLayout> anchors;
        world.Each<AnchorLayout>([&entities, &anchors](Entity entity, AnchorLayout& anchor) {
            entities.push_back(entity);
            anchors.emplace(entity, anchor);
        });
        if (entities.empty()) {
            return;
        }

        // 计算父链深度（防环：最多 64 层）
        std::unordered_map<Entity, int> depths;
        const auto depthOf = [&depths, &anchors](Entity entity) {
            int depth = 0;
            Entity current = entity;
            for (int guard = 0; guard < 64; ++guard) {
                const auto it = anchors.find(current);
                if (it == anchors.end()) {
                    break;
                }
                const Entity parent = it->second.parent;
                if (parent == kInvalidEntity || parent == current || anchors.find(parent) == anchors.end()) {
                    break;
                }
                current = parent;
                ++depth;
            }
            depths[entity] = depth;
            return depth;
        };
        std::stable_sort(entities.begin(), entities.end(), [&depthOf](Entity lhs, Entity rhs) {
            return depthOf(lhs) < depthOf(rhs);
        });

        for (Entity entity : entities) {
            const AnchorLayout& anchor = anchors.at(entity);
            layout::Rect base = SpaceRect(shell, anchor.space);
            if (anchor.parent != kInvalidEntity && anchor.parent != entity) {
                const ScreenRect* parentRect = world.TryGet<ScreenRect>(anchor.parent);
                if (parentRect != nullptr) {
                    base = parentRect->rect;
                }
            }

            layout::Rect rect;
            rect.x = base.x + anchor.x * base.w + anchor.inset.left;
            rect.y = base.y + anchor.y * base.h + anchor.inset.top;
            rect.w = anchor.w * base.w - anchor.inset.left - anchor.inset.right;
            rect.h = anchor.h * base.h - anchor.inset.top - anchor.inset.bottom;
            if (anchor.minWidth > 0.0f || anchor.maxWidth < 1.0e8f) {
                rect.w = std::clamp(rect.w, anchor.minWidth, anchor.maxWidth);
            }
            if (anchor.minHeight > 0.0f || anchor.maxHeight < 1.0e8f) {
                rect.h = std::clamp(rect.h, anchor.minHeight, anchor.maxHeight);
            }
            if (ScreenRect* screenRect = world.TryGet<ScreenRect>(entity)) {
                screenRect->rect = rect;
            } else {
                world.Add<ScreenRect>(entity, ScreenRect{rect});
            }
        }
    }
};

// -----------------------------------------------------------------------------
// RouteSystem：消费 Navigate / Logout 命令（渲染层发命令，数据层执行）
// -----------------------------------------------------------------------------
class RouteSystem final : public ISystem {
public:
    const char* Name() const override { return "RouteSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Simulation; }

    void Update(World& world, const FrameContext& /*frame*/) override {
        auto* queue = world.TryResource<CommandQueue>();
        auto* routeState = world.TryResource<RouteState>();
        if (queue == nullptr || routeState == nullptr) {
            return;
        }
        for (const CommandQueue::Command& command : queue->commands) {
            switch (command.type) {
                case CommandQueue::Type::Navigate: {
                    Route route = Route::Dashboard;
                    if (TryRouteFromPath(command.arg0, route)) {
                        routeState->current = route;
                    }
                    break;
                }
                case CommandQueue::Type::Logout: {
                    if (auto* session = world.TryResource<AuthSession>()) {
                        session->loggedIn = false;
                    }
                    routeState->current = Route::Login;
                    break;
                }
                default:
                    break;
            }
        }
        // path 始终由 Route 归一化（菜单高亮 / 面包屑 / 标签栏唯一来源）
        routeState->path = RoutePath(routeState->current);
    }
};

// -----------------------------------------------------------------------------
// DataBootstrapSystem：启动时从数据服务加载看板快照（渲染层只读 DashboardData）
// -----------------------------------------------------------------------------
class DataBootstrapSystem final : public ISystem {
public:
    const char* Name() const override { return "DataBootstrapSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Simulation; }

    void OnStart(World& world) override {
        auto* services = world.TryResource<DataServices>();
        if (services == nullptr || !services->dashboard) {
            return;
        }
        data::UserSession session;
        if (auto* auth = world.TryResource<AuthSession>(); auth != nullptr && auth->loggedIn) {
            session = auth->user;
        }
        world.SetResource(services->dashboard->LoadDashboard(session));
    }

    void Update(World& /*world*/, const FrameContext& /*frame*/) override {}
};

}  // namespace

std::unique_ptr<ISystem> CreateLayoutSystem() { return std::make_unique<LayoutSystem>(); }
std::unique_ptr<ISystem> CreateRouteSystem() { return std::make_unique<RouteSystem>(); }
std::unique_ptr<ISystem> CreateDataBootstrapSystem() { return std::make_unique<DataBootstrapSystem>(); }

}  // namespace zzyl
