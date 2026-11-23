#include "zzyl/data/MockServices.h"

#include <ctime>

namespace zzyl::data {
namespace {

std::string NowText() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local);
    return std::string(buffer);
}

std::string HourCategories() {
    // 24 小时刻度（与 Web constants.js 的 TODAY_TIME 一致）
    std::string result;
    for (int hour = 0; hour < 24; ++hour) {
        if (hour > 0) {
            result += "|";
        }
        result += std::to_string(hour) + ":00";
    }
    return result;
}

std::vector<std::string> SplitCategories(const std::string& packed) {
    std::vector<std::string> categories;
    std::string current;
    for (char ch : packed) {
        if (ch == '|') {
            categories.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    categories.push_back(current);
    return categories;
}

}  // namespace

LoginResult MockAuthService::Login(const LoginRequest& request) {
    LoginResult result;
    if (request.username.empty()) {
        result.message = "请输入您的账号";
        return result;
    }
    if (request.password.empty()) {
        result.message = "请输入您的密码";
        return result;
    }
    if (request.code.empty()) {
        result.message = "请输入验证码（本迭代验证码仅占位，任意非空即可）";
        return result;
    }
    if (request.username != "admin" || request.password != "admin123") {
        result.message = "账号或密码错误（演示账号：admin / admin123）";
        return result;
    }

    result.ok = true;
    result.message = "登录成功";
    result.session.userId = "1";
    result.session.userName = "admin";
    result.session.nickName = "管理员";
    result.session.roleGroup = "超级管理员";
    result.session.postGroup = "中州养老院 / 研发部";
    result.session.deptName = "中州养老院";
    result.session.avatarText = "管";
    return result;
}

void MockAuthService::Logout() {}

DashboardSnapshot MockDashboardService::LoadDashboard(const UserSession& session) {
    DashboardSnapshot snapshot;
    snapshot.welcome = "Hello！" + (session.nickName.empty() ? std::string("管理员") : session.nickName);
    snapshot.subtitle = "今天也是元气满满的一天！";
    snapshot.updateTime = NowText();

    // ---- 统计卡：老人 / 床位 / 服务单 / 员工 / 收入（Web UI 规格 §8.3 B 套，2026-11-23 实测） ----
    snapshot.stats.push_back(StatCardData{"老人数量", "655", "人", "在院 644 · 外出 11", 644.0f / 655.0f, 0});
    snapshot.stats.push_back(StatCardData{"床位数量", "812", "个", "入住 655 · 空闲 157", 655.0f / 812.0f, 1});
    snapshot.stats.push_back(StatCardData{"服务单数量", "100.35", "w笔", "计划内 94.18w · 计划外 6.17w", 94.18f / 100.35f, 2});
    snapshot.stats.push_back(StatCardData{"员工数量", "204", "人", "普通员工 186 · 管理层 18", 186.0f / 204.0f, 3});
    snapshot.stats.push_back(StatCardData{"收入金额", "410.81", "w元", "服务费用 92.85w · 月度费用 317.96w", 317.96f / 410.81f, 4});

    // ---- 图表 1：收益情况（本周，折线，B 套 WEEK_NUM_B） ----
    {
        ChartPanelData chart;
        chart.title = "收益情况";
        chart.subtitle = "本周";
        chart.kind = ChartKind::Line;
        chart.categories = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
        chart.series.push_back(ChartSeries{"收入金额", {13785.0f, 14634.0f, 13636.0f, 13846.0f, 14038.0f, 14845.0f, 14607.0f}});
        chart.series.push_back(ChartSeries{"退款金额", {2981.0f, 3033.0f, 3105.0f, 3311.0f, 3326.0f, 3141.0f, 3302.0f}});
        snapshot.charts.push_back(std::move(chart));
    }

    // ---- 图表 2：入退情况（今日 24h，分组柱状，B 套 TODAY_EARNING_NUM_B） ----
    {
        ChartPanelData chart;
        chart.title = "入退情况";
        chart.subtitle = "今日";
        chart.kind = ChartKind::Bar;
        chart.categories = SplitCategories(HourCategories());
        chart.series.push_back(ChartSeries{
            "入住人数", {0, 0, 0, 0, 0, 0, 1, 2, 4, 3, 0, 1, 3, 3, 3, 4, 1, 2, 1, 1, 0, 0, 0, 0}});
        chart.series.push_back(ChartSeries{
            "退住人数", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 1, 1, 2, 1, 1, 1, 3, 1, 1, 0, 0, 0}});
        snapshot.charts.push_back(std::move(chart));
    }

    // ---- 图表 3：服务情况（今日 24h，折线，B 套 TODAY_SERVE_NUM_B） ----
    {
        ChartPanelData chart;
        chart.title = "服务情况";
        chart.subtitle = "今日";
        chart.kind = ChartKind::Line;
        chart.categories = SplitCategories(HourCategories());
        chart.series.push_back(ChartSeries{
            "服务次数", {74, 56, 34, 44, 62, 67, 110, 128, 137, 176, 140, 139, 157, 108, 121, 123, 117, 143, 141, 163, 102, 61, 61, 52}});
        snapshot.charts.push_back(std::move(chart));
    }

    // ---- 图表 4：老人年龄分布（堆叠柱状，B 套 ELDER_AGE_DATA_B） ----
    {
        ChartPanelData chart;
        chart.title = "老人年龄分布";
        chart.subtitle = "数据更新：" + snapshot.updateTime;
        chart.kind = ChartKind::StackedBar;
        chart.categories = {"60岁以下", "60-70岁", "71-80岁", "81-90岁", "90岁以上"};
        chart.series.push_back(ChartSeries{"男", {19, 54, 128, 98, 8}});
        chart.series.push_back(ChartSeries{"女", {25, 40, 164, 102, 17}});
        snapshot.charts.push_back(std::move(chart));
    }

    // ---- 排行列表：老人等级分布（B 套 ELDER_RANK_DATA_B：42/91/191/331，合计 655） ----
    const int total = 42 + 91 + 191 + 331;
    snapshot.ranks.push_back(RankRowData{1, "三级护理等级", "331 人", "占比 50.5%", 331.0f / static_cast<float>(total)});
    snapshot.ranks.push_back(RankRowData{2, "二级护理等级", "191 人", "占比 29.2%", 191.0f / static_cast<float>(total)});
    snapshot.ranks.push_back(RankRowData{3, "一级护理等级", "91 人", "占比 13.9%", 91.0f / static_cast<float>(total)});
    snapshot.ranks.push_back(RankRowData{4, "特级护理等级", "42 人", "占比 6.4%", 42.0f / static_cast<float>(total)});

    return snapshot;
}

}  // namespace zzyl::data
