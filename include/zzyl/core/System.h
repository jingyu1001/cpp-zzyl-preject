#pragma once

// =============================================================================
// 系统接口与阶段划分
//
// 每帧执行顺序由 SystemPhase 决定：
//   Input(0) -> Simulation(100) -> Layout(200) -> Render(300) -> Overlay(400)
// 同阶段内按注册顺序（order 参数）执行，保证"先布局、后绘制"的确定性。
// =============================================================================

#include <cstdint>

#include "zzyl/core/World.h"

namespace zzyl {

// 每帧上下文（只读，由 App 注入）
struct FrameContext {
    float deltaTime = 0.0f;      // 秒
    double totalTime = 0.0;      // 启动以来秒数（glfwGetTime）
    std::uint64_t frameIndex = 0;
};

enum class SystemPhase : int {
    Input = 0,        // 输入采集（窗口/快捷键/命令入队）
    Simulation = 100, // 业务状态推进（路由、登录、数据加载）
    Layout = 200,     // 响应式布局计算（ShellLayout / ScreenRect）
    Render = 300,     // ImGui 绘制（只读状态，不修改业务数据）
    Overlay = 400,    // 调试浮层等最后绘制内容
};

class ISystem {
public:
    virtual ~ISystem() = default;

    // 系统名（调试统计用）
    virtual const char* Name() const = 0;
    virtual SystemPhase Phase() const { return SystemPhase::Simulation; }

    // 所有系统注册完成、主循环开始前调用一次
    virtual void OnStart(World& /*world*/) {}

    // 每帧调用
    virtual void Update(World& world, const FrameContext& frame) = 0;
};

}  // namespace zzyl
