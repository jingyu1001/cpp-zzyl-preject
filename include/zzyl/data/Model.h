#pragma once

// =============================================================================
// 数据模型（纯数据，不依赖 ImGui）
//
// 渲染层只读这些结构；数据来源由 data/IAuthService / IDashboardService 抽象，
// 当前迭代为 Mock，后续接 MySQL（远程 RuoYi 后端）与 SQLite3（本地缓存）时
// 只需新增实现，渲染代码零改动。
// =============================================================================

#include <string>
#include <vector>

namespace zzyl::data {

// 登录后的用户会话（对应 Web 端 store/modules/user 的 getInfo 结果）
struct UserSession {
    std::string userId = "1";
    std::string userName = "admin";
    std::string nickName = "管理员";
    std::string roleGroup = "超级管理员";
    std::string postGroup = "研发部";
    std::string deptName = "中州养老院";
    std::string avatarText = "管";
};

// 统计卡（对应首页"数据概览"的 5 个环形指标：老人/床位/服务单/员工/收入）
struct StatCardData {
    std::string title;       // 老人数量
    std::string value;       // 643
    std::string unit;        // 人
    std::string subLabel;    // 在院 622 · 外出 21
    float ratio = 0.0f;      // 0..1，卡片内进度条比例（由分母/总数推导）
    int accentIndex = 0;     // 主题 chart[] 色板下标
};

enum class ChartKind {
    Line,        // 折线图（收益情况 / 服务情况）
    Bar,         // 分组柱状图（入退情况）
    StackedBar,  // 堆叠柱状图（老人年龄分布）
    Ring,        // 环形图（老人等级分布）
};

struct ChartSeries {
    std::string name;
    std::vector<float> values;
};

struct ChartPanelData {
    std::string title;       // 收益情况
    std::string subtitle;    // 本周
    ChartKind kind = ChartKind::Line;
    std::vector<std::string> categories;
    std::vector<ChartSeries> series;
};

// 排行列表行（对应 RankList.vue 的老人等级分布）
struct RankRowData {
    int rank = 0;
    std::string name;       // 三级护理等级
    std::string valueText;  // 319 人
    std::string subText;    // 占比 49.6%
    float ratio = 0.0f;     // 0..1，用于条形长度
};

// 首页看板快照（一次加载，渲染层只读）
struct DashboardSnapshot {
    std::string welcome;     // 下午好，管理员
    std::string subtitle;    // 今天也是元气满满的一天！
    std::string updateTime;  // 2025-11-23 22:00
    std::vector<StatCardData> stats;
    std::vector<ChartPanelData> charts;
    std::vector<RankRowData> ranks;
};

}  // namespace zzyl::data
