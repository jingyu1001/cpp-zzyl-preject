// =============================================================================
// 首页看板（cpp-dev3 负责）
//
// 依据：
//  - docs/architecture/Cpp架构设计_v0.1.md（ECS / 渲染与数据分离 / 布局约束）
//  - docs/tasks/Sprint1_cpp-dev3_看板页.md
//  - docs/architecture/UI复刻规格_Cpp_1280x720_v0.1.md §2/§3/§8（本文件所有视觉口径来源）
//
// 结构（保持 ECS）：
//  - 7 个面板实体 = AnchorLayout（统一锚定 pageViewport）+ DashPanelView（面板种类）
//  - DashboardLayoutSystem（Layout 阶段，order 10）：按 Web 规格 §8.1 公式计算每个面板的
//    内容坐标矩形，写入 DashPanelView::frame；同时产出 DashboardLayoutState（内容总高/滚动信息）
//  - DashboardRenderSystem（Render 阶段，order 200）：只读 DashboardData + 布局结果，
//    用 ImDrawList 自绘卡片 / 环形统计 / 折线 / 柱状 / 堆叠柱 / 排行榜，不写任何业务数据
//
// 渲染与数据分离：
//  - 本文件不调用数据服务、不修改 DashboardData / RouteState / AuthSession；
//  - 面板数据全部来自只读资源 DashboardData（stats / charts / ranks / updateTime）；
//  - 布局比例来自 ShellLayout（pageViewport / LayoutTokens）与卡片矩形，无绝对元素坐标。
//
// 响应式口径（与 lead 确认）：
//  - 宽屏（窗口宽 ≥ 1200）：两栏 17:7、底部三栏 7:7:10，行高按 237:278:278 比例铺满
//    pageViewport —— 1280×720 一屏放下、无滚动；1920×1080 同样一屏。
//  - 窄屏（窗口宽 < 1200，Web 断点）：所有面板单列，卡片保持规格高度（237/278），
//    内容超出 pageViewport 时由 ImGui 子窗口提供纵向滚动（滚动条样式继承主题：6px、#F1F1F1/#C0C0C0）。
// =============================================================================

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"

#include "zzyl/app/AppState.h"
#include "zzyl/app/ModuleHooks.h"
#include "zzyl/core/System.h"
#include "zzyl/layout/Layout.h"
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"

namespace zzyl {
namespace {

using data::ChartKind;
using data::ChartPanelData;
using data::DashboardSnapshot;
using data::RankRowData;
using data::StatCardData;
using layout::Rect;

// -----------------------------------------------------------------------------
// 规格常量（Web UI 复刻规格 §8.1 / §3.2；属于"布局约束"，不是元素坐标）
// -----------------------------------------------------------------------------
constexpr float kRowWeightTop = 237.0f;      // 行1 设计高度权重
constexpr float kRowWeightChart = 278.0f;    // 行2 / 行3 设计高度权重
constexpr float kNarrowBreakpointWidth = 1200.0f;  // §3.2：窗口宽 <1200 → 单列
constexpr float kNarrowStatCardHeight = 237.0f;    // §3.2：单列时卡片高度固定 237 / 278
constexpr float kNarrowPanelHeight = 278.0f;
constexpr float kTwoColumnRatioA = 17.0f / 24.0f;  // §8.1 两栏 17:7
constexpr float kTwoColumnRatioB = 7.0f / 24.0f;
constexpr float kThreeColumnRatioA = 7.0f / 24.0f;   // §8.1 三栏 7:7:10
constexpr float kThreeColumnRatioC = 10.0f / 24.0f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;

// -----------------------------------------------------------------------------
// ECS 组件 / 资源（仅本页使用，定义在模块文件内，不改 AppState.h）
// -----------------------------------------------------------------------------

// 面板种类（7 个面板对应 Web 看板的 7 个区域）
enum class DashPanelKind : int {
    StatOverview = 0,   // 数据概览（5 个环形统计）
    RankList = 1,       // 老人等级排行列表（4 行）
    ChartEarnings = 2,  // 收益情况（双折线 + 今日/本周/本月 tab）
    ChartCheckIn = 3,   // 入退情况（分组柱状）
    ChartService = 4,   // 服务情况（单折线）
    ChartAge = 5,       // 老人年龄分布（堆叠柱）
    ElderLevelRing = 6, // 老人等级分布（环形 + 图例）
};

// 面板组件：kind 由注册时确定；frame 由 DashboardLayoutSystem 每帧写入（内容坐标系）
struct DashPanelView {
    DashPanelKind kind = DashPanelKind::StatOverview;
    layout::Rect frame;  // 内容坐标：页内容左上角为原点（Render 阶段叠加滚动偏移）
};

// 布局结果资源（Layout 阶段写，Render 阶段只读）
struct DashboardLayoutState {
    float contentWidth = 0.0f;   // 内容宽度（已为滚动条预留空间）
    float contentHeight = 0.0f;  // 内容总高
    bool singleColumn = false;   // 是否单列
    bool scrollable = false;     // 内容是否超出可视高度（需要滚动）
};

// -----------------------------------------------------------------------------
// 小工具：数学 / 文本 / 数字格式
// -----------------------------------------------------------------------------

float ClampF(float value, float low, float high) { return std::max(low, std::min(high, value)); }

float TextWidth(const char* text) { return ImGui::CalcTextSize(text).x; }

// UTF-8 首字节 → 字符字节数（中文 3 字节，截断时必须整字截断）
std::size_t Utf8CharLength(unsigned char lead) {
    if ((lead & 0x80u) == 0u) {
        return 1;
    }
    if ((lead & 0xE0u) == 0xC0u) {
        return 2;
    }
    if ((lead & 0xF0u) == 0xE0u) {
        return 3;
    }
    if ((lead & 0xF8u) == 0xF0u) {
        return 4;
    }
    return 1;
}

// 按可用宽度截断文本并追加省略号（调用前需已切换到目标字体/字号）
std::string FitText(const std::string& text, float maxWidth) {
    if (text.empty() || maxWidth <= 0.0f) {
        return std::string();
    }
    if (TextWidth(text.c_str()) <= maxWidth) {
        return text;
    }
    const float ellipsisWidth = TextWidth("…");
    std::string result;
    std::size_t index = 0;
    while (index < text.size()) {
        const std::size_t length = Utf8CharLength(static_cast<unsigned char>(text[index]));
        if (index + length > text.size()) {
            break;
        }
        std::string candidate = result + text.substr(index, length);
        if (TextWidth((candidate + "…").c_str()) > maxWidth) {
            break;
        }
        result = std::move(candidate);
        index += length;
    }
    (void)ellipsisWidth;
    return result + "…";
}

// 取字符串开头的整数（用于 "319 人" 这类展示文本求总和；解析失败返回 0）
int ParseLeadingInt(const std::string& text) {
    int value = 0;
    bool found = false;
    for (char ch : text) {
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + (ch - '0');
            found = true;
        } else if (found) {
            break;
        } else if (ch != ' ') {
            break;
        }
    }
    return found ? value : 0;
}

int SumRankValues(const std::vector<RankRowData>& ranks) {
    int total = 0;
    for (const RankRowData& rank : ranks) {
        total += ParseLeadingInt(rank.valueText);
    }
    return total;
}

// 显示日期：截取 "YYYY-MM-DD HH:MM" 的日期部分
std::string DisplayDate(const std::string& timestamp) {
    if (timestamp.size() >= 10) {
        return timestamp.substr(0, 10);
    }
    return timestamp;
}

// 计算"今日 / 本周 / 本月"对应的日期区间文案（Web D-07：C++ 按正确逻辑显示区间）
std::string CurrentDateRange(const std::string& period, const std::string& timestamp) {
    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(timestamp.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return period;
    }
    std::tm date{};
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_mday = day;
    date.tm_hour = 12;
    if (std::mktime(&date) == static_cast<std::time_t>(-1)) {
        return period;
    }

    std::tm first = date;
    std::tm last = date;
    if (period == "本周") {
        const int offset = (date.tm_wday + 6) % 7;  // 周一 = 0
        first.tm_mday -= offset;
        last = first;
        last.tm_mday += 6;
    } else if (period == "本月") {
        first.tm_mday = 1;
        last.tm_mday = 1;
        last.tm_mon += 1;
        last.tm_mday = 0;  // 归一到本月最后一天
    } else if (period != "今日") {
        return period;
    }
    std::mktime(&first);
    std::mktime(&last);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d 至 %04d-%02d-%02d",
                  first.tm_year + 1900, first.tm_mon + 1, first.tm_mday, last.tm_year + 1900,
                  last.tm_mon + 1, last.tm_mday);
    return std::string(buffer);
}

// 图表 y 轴刻度文案（金额用 w 简写，贴近 ECharts 的紧凑风格）
void FormatAxisTick(char* buffer, std::size_t size, float value, float axisMax) {
    const float magnitude = std::fabs(value);
    if (axisMax >= 10000.0f) {
        const float scaled = value / 10000.0f;
        const float rounded = std::round(scaled);
        if (std::fabs(scaled - rounded) < 0.05f) {
            std::snprintf(buffer, size, "%.0fw", rounded);
        } else {
            std::snprintf(buffer, size, "%.1fw", scaled);
        }
        return;
    }
    if (magnitude >= 1000.0f || std::fabs(value - std::round(value)) < 0.05f) {
        std::snprintf(buffer, size, "%.0f", value);
    } else {
        std::snprintf(buffer, size, "%.1f", value);
    }
}

// -----------------------------------------------------------------------------
// 卡片度量（所有内边距/字号从卡片矩形推导，不写死坐标）
// -----------------------------------------------------------------------------
struct CardStyle {
    Rect card;
    float padding = 0.0f;
    float titleSize = 14.0f;
    float headerHeight = 0.0f;
    Rect body;  // 卡体区域（已扣除卡头与内边距）
};

CardStyle MakeCardStyle(const Rect& card) {
    CardStyle style;
    style.card = card;
    const float shortEdge = std::min(card.w, card.h);
    style.padding = ClampF(shortEdge * 0.065f, 8.0f, 20.0f);
    style.titleSize = ClampF(card.h * 0.095f, 12.5f, 17.0f);
    style.headerHeight = ClampF(card.h * 0.17f, style.titleSize + 6.0f, card.h * 0.32f);
    style.body = Rect{card.x + style.padding, card.y + style.headerHeight,
                      std::max(0.0f, card.w - style.padding * 2.0f),
                      std::max(0.0f, card.h - style.headerHeight - style.padding)};
    return style;
}

// 卡头：左标题（粗体）+ 右附注（次要文字）；附注空间不足时按宽度截断
void DrawCardHeader(ImDrawList* drawList, const CardStyle& style, const std::string& title,
                    const std::string& note, const theme::Palette& colors) {
    const Rect header{style.card.x + style.padding, style.card.y,
                      std::max(0.0f, style.card.w - style.padding * 2.0f), style.headerHeight};
    float titleWidth = 0.0f;
    {
        ui::ScopedBoldFont titleFont(style.titleSize);
        titleWidth = TextWidth(title.c_str());
        drawList->AddText(ImVec2(header.x, header.y + (header.h - style.titleSize) * 0.5f),
                          colors.textPrimary, title.c_str());
    }
    if (note.empty()) {
        return;
    }
    const float noteSize = ClampF(style.card.h * 0.068f, 9.0f, 11.5f);
    ui::ScopedFontSize noteFont(noteSize);
    const float maxWidth = std::max(0.0f, header.w - titleWidth - style.padding * 1.5f);
    const std::string fitted = FitText(note, maxWidth);
    if (fitted.empty()) {
        return;
    }
    const float width = TextWidth(fitted.c_str());
    drawList->AddText(ImVec2(header.Right() - width, header.y + (header.h - noteSize) * 0.5f),
                      colors.textMuted, fitted.c_str());
}

void DrawEmptyHint(ImDrawList* drawList, const Rect& rect, const char* text,
                   const theme::Palette& colors) {
    ui::ScopedFontSize font(12.0f);
    const float width = TextWidth(text);
    drawList->AddText(ImVec2(rect.CenterX() - width * 0.5f, rect.CenterY() - 6.0f),
                      colors.textMuted, text);
}

// -----------------------------------------------------------------------------
// 图例（圆点 + 名称，右对齐；字号由调用方按卡片尺寸推导）
// -----------------------------------------------------------------------------
void DrawLegend(ImDrawList* drawList, const Rect& area,
                const std::vector<std::pair<ImU32, std::string>>& items, float fontSize) {
    if (items.empty() || area.Empty()) {
        return;
    }
    ui::ScopedFontSize font(fontSize);
    const float dotRadius = fontSize * 0.26f;
    const float dotGap = fontSize * 0.45f;
    const float itemGap = fontSize * 1.1f;
    float totalWidth = 0.0f;
    for (const auto& [color, name] : items) {
        (void)color;
        totalWidth += dotRadius * 2.0f + dotGap + TextWidth(name.c_str());
    }
    totalWidth += itemGap * static_cast<float>(items.size() - 1);

    float x = area.Right() - totalWidth;
    if (x < area.x) {
        x = area.x;  // 空间不足时从左开始（裁剪保证不出卡片）
    }
    const float textY = area.y + (area.h - fontSize) * 0.5f;
    for (const auto& [color, name] : items) {
        const ImVec2 center(x + dotRadius, textY + fontSize * 0.5f);
        drawList->AddCircleFilled(center, dotRadius, color, 10);
        drawList->AddText(ImVec2(x + dotRadius * 2.0f + dotGap, textY), theme::Colors().textRegular,
                          name.c_str());
        x += dotRadius * 2.0f + dotGap + TextWidth(name.c_str()) + itemGap;
    }
}

// -----------------------------------------------------------------------------
// 图表：坐标轴 / 网格 / 折线 / 柱状 / 堆叠柱
// -----------------------------------------------------------------------------
struct AxisRange {
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float step = 0.25f;
};

float NiceNumber(float range, bool round) {
    if (range <= 0.0f) {
        return 1.0f;
    }
    const float exponent = std::floor(std::log10(range));
    const float fraction = range / std::pow(10.0f, exponent);
    float niceFraction = 1.0f;
    if (round) {
        if (fraction < 1.5f) {
            niceFraction = 1.0f;
        } else if (fraction < 3.0f) {
            niceFraction = 2.0f;
        } else if (fraction < 7.0f) {
            niceFraction = 5.0f;
        } else {
            niceFraction = 10.0f;
        }
    } else {
        if (fraction <= 1.0f) {
            niceFraction = 1.0f;
        } else if (fraction <= 2.0f) {
            niceFraction = 2.0f;
        } else if (fraction <= 5.0f) {
            niceFraction = 5.0f;
        } else {
            niceFraction = 10.0f;
        }
    }
    return niceFraction * std::pow(10.0f, exponent);
}

AxisRange MakeAxisRange(float dataMin, float dataMax, bool includeZero) {
    AxisRange axis;
    const float low = includeZero ? std::min(0.0f, dataMin) : dataMin;
    float high = dataMax;
    if (high <= low) {
        high = low + 1.0f;
    }
    const float niceRange = NiceNumber(high - low, false);
    axis.step = NiceNumber(niceRange / 4.0f, true);
    if (axis.step <= 0.0f) {
        axis.step = 1.0f;
    }
    axis.minValue = std::floor(low / axis.step) * axis.step;
    axis.maxValue = std::ceil(high / axis.step) * axis.step;
    if (axis.maxValue <= axis.minValue) {
        axis.maxValue = axis.minValue + axis.step;
    }
    return axis;
}

int AxisIntervalCount(const AxisRange& axis) {
    const float span = axis.maxValue - axis.minValue;
    if (axis.step <= 0.0f || span <= 0.0f) {
        return 1;
    }
    const int intervals = static_cast<int>(std::lround(span / axis.step));
    return std::max(1, std::min(intervals, 10));
}

float ValueToY(const Rect& plot, const AxisRange& axis, float value) {
    const float span = axis.maxValue - axis.minValue;
    if (span <= 0.0f) {
        return plot.Bottom();
    }
    const float normalized = (value - axis.minValue) / span;
    return plot.Bottom() - normalized * plot.h;
}

void DashedHLine(ImDrawList* drawList, float x0, float x1, float y, ImU32 color, float thickness,
                 float dash, float gap) {
    if (x1 <= x0 || dash <= 0.0f) {
        return;
    }
    for (float x = x0; x < x1; x += dash + gap) {
        const float end = std::min(x + dash, x1);
        drawList->AddLine(ImVec2(x, y), ImVec2(end, y), color, thickness);
    }
}

// y 轴网格（浅色虚线）+ 刻度文字 + x 轴实线
void DrawYGridAndLabels(ImDrawList* drawList, const Rect& plot, const AxisRange& axis,
                        float fontSize, ImU32 gridColor, ImU32 axisColor, ImU32 textColor) {
    const int intervals = AxisIntervalCount(axis);
    ui::ScopedFontSize font(fontSize);
    char buffer[32];
    for (int index = 0; index <= intervals; ++index) {
        const float value = axis.minValue + axis.step * static_cast<float>(index);
        const float y = ValueToY(plot, axis, value);
        if (index == 0) {
            drawList->AddLine(ImVec2(plot.x, y), ImVec2(plot.Right(), y), axisColor, 1.0f);
        } else {
            DashedHLine(drawList, plot.x, plot.Right(), y, gridColor, 1.0f, 4.0f, 4.0f);
        }
        FormatAxisTick(buffer, sizeof(buffer), value, axis.maxValue);
        const float width = TextWidth(buffer);
        drawList->AddText(ImVec2(plot.x - 6.0f - width, y - fontSize * 0.5f), textColor, buffer);
    }
}

// x 轴类目标签：按可用宽度自动抽稀（24 小时不会糊成一团）
void DrawCategoryLabels(ImDrawList* drawList, const Rect& plot,
                        const std::vector<std::string>& categories, bool boundaryGap,
                        float fontSize, ImU32 color) {
    if (categories.empty() || plot.w <= 0.0f) {
        return;
    }
    ui::ScopedFontSize font(fontSize);
    const int count = static_cast<int>(categories.size());
    const float categoryWidth = plot.w / static_cast<float>(count);
    const float gap = fontSize * 0.9f;
    const float labelY = plot.Bottom() + 2.0f;
    float lastRight = -1.0e9f;
    for (int index = 0; index < count; ++index) {
        const char* text = categories[static_cast<std::size_t>(index)].c_str();
        if (text == nullptr || text[0] == '\0') {
            continue;
        }
        const float width = TextWidth(text);
        float centerX = 0.0f;
        if (boundaryGap) {
            centerX = plot.x + (static_cast<float>(index) + 0.5f) * categoryWidth;
        } else {
            centerX = count > 1
                          ? plot.x + static_cast<float>(index) * plot.w / static_cast<float>(count - 1)
                          : plot.CenterX();
        }
        const float left = centerX - width * 0.5f;
        if (left < lastRight + gap) {
            continue;
        }
        drawList->AddText(ImVec2(left, labelY), color, text);
        lastRight = left + width;
    }
}

// Catmull-Rom 平滑（贴近 ECharts 的 smooth 折线；对越界 y 做钳制）
void BuildSmoothPolyline(const std::vector<ImVec2>& points, std::vector<ImVec2>& out,
                         float clampTop, float clampBottom, int samples) {
    out.clear();
    if (points.size() < 3 || samples < 2) {
        out = points;
        return;
    }
    out.reserve(points.size() * static_cast<std::size_t>(samples));
    for (std::size_t index = 0; index + 1 < points.size(); ++index) {
        const ImVec2 p0 = points[index == 0 ? 0 : index - 1];
        const ImVec2 p1 = points[index];
        const ImVec2 p2 = points[index + 1];
        const ImVec2 p3 = points[(index + 2 < points.size()) ? index + 2 : points.size() - 1];
        for (int step = 0; step < samples; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(samples);
            const float t2 = t * t;
            const float t3 = t2 * t;
            ImVec2 point;
            point.x = 0.5f * (2.0f * p1.x + (-p0.x + p2.x) * t +
                              (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                              (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
            point.y = 0.5f * (2.0f * p1.y + (-p0.y + p2.y) * t +
                              (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                              (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
            point.x = ClampF(point.x, p1.x, p2.x);
            point.y = ClampF(point.y, clampTop, clampBottom);
            out.push_back(point);
        }
    }
    out.push_back(points.back());
}

// series → 颜色（与 Web 端 index1.js 的取色一致；单 series 用青色）
ImU32 SeriesColor(const ChartPanelData& chart, std::size_t index, const theme::Palette& colors) {
    if (chart.kind == ChartKind::StackedBar) {
        return index == 0 ? colors.ageMale : colors.ageFemale;
    }
    if (chart.series.size() == 1) {
        return colors.chartLineTeal;
    }
    return index == 0 ? colors.chartLineBlue : colors.chartLineTeal;
}

// 折线图（含首尾点圆点，类目多时不画点）
void DrawLineSeries(ImDrawList* drawList, const Rect& plot, const ChartPanelData& chart,
                    const AxisRange& axis, const theme::Palette& colors, float lineThickness) {
    const std::size_t seriesCount = chart.series.size();
    std::vector<ImVec2> points;
    std::vector<ImVec2> smooth;
    for (std::size_t seriesIndex = 0; seriesIndex < seriesCount; ++seriesIndex) {
        const auto& series = chart.series[seriesIndex];
        const int count = static_cast<int>(series.values.size());
        if (count <= 0) {
            continue;
        }
        points.clear();
        points.reserve(static_cast<std::size_t>(count));
        const float stepX =
            count > 1 ? plot.w / static_cast<float>(count - 1) : 0.0f;
        for (int index = 0; index < count; ++index) {
            const float x = plot.x + stepX * static_cast<float>(index);
            const float y =
                ClampF(ValueToY(plot, axis, series.values[static_cast<std::size_t>(index)]),
                       plot.y, plot.Bottom());
            points.push_back(ImVec2(x, y));
        }
        BuildSmoothPolyline(points, smooth, plot.y, plot.Bottom(), 10);
        const ImU32 color = SeriesColor(chart, seriesIndex, colors);
        if (smooth.size() >= 2) {
            drawList->AddPolyline(smooth.data(), static_cast<int>(smooth.size()), color, 0,
                                  lineThickness);
        }
        if (count <= 10) {
            const float radius = ClampF(lineThickness * 1.5f, 2.6f, 4.0f);
            for (const ImVec2& point : points) {
                drawList->AddCircleFilled(point, radius, colors.panel, 12);
                drawList->AddCircle(point, radius, color, 12, ClampF(lineThickness * 0.7f, 1.0f, 1.8f));
            }
        }
    }
}

// 分组柱状图（入退情况：每类目 2 根柱）
void DrawGroupedBars(ImDrawList* drawList, const Rect& plot, const ChartPanelData& chart,
                     const AxisRange& axis, const theme::Palette& colors) {
    const int categoryCount = static_cast<int>(chart.categories.size());
    const int seriesCount = static_cast<int>(chart.series.size());
    if (categoryCount <= 0 || seriesCount <= 0) {
        return;
    }
    const float categoryWidth = plot.w / static_cast<float>(categoryCount);
    const float groupWidth = categoryWidth * (seriesCount > 1 ? 0.62f : 0.46f);
    const float barGap = categoryWidth * 0.03f;
    const float barWidth = std::max(1.0f, (groupWidth - barGap * static_cast<float>(seriesCount - 1)) /
                                             static_cast<float>(seriesCount));
    const float baseline = ClampF(0.0f, axis.minValue, axis.maxValue);
    const float baselineY = ValueToY(plot, axis, baseline);
    const float radius = ClampF(barWidth * 0.22f, 1.0f, 3.0f);
    for (int categoryIndex = 0; categoryIndex < categoryCount; ++categoryIndex) {
        for (int seriesIndex = 0; seriesIndex < seriesCount; ++seriesIndex) {
            const auto& series = chart.series[static_cast<std::size_t>(seriesIndex)];
            if (static_cast<std::size_t>(categoryIndex) >= series.values.size()) {
                continue;
            }
            const float value = series.values[static_cast<std::size_t>(categoryIndex)];
            if (value <= 0.0f) {
                continue;
            }
            const float top = ClampF(ValueToY(plot, axis, value), plot.y, plot.Bottom());
            const float bottom = ClampF(baselineY, plot.y, plot.Bottom());
            const float x = plot.x + (static_cast<float>(categoryIndex) + 0.5f) * categoryWidth -
                            groupWidth * 0.5f +
                            static_cast<float>(seriesIndex) * (barWidth + barGap);
            const Rect bar{x, std::min(top, bottom), barWidth, std::fabs(bottom - top)};
            if (bar.h < 0.5f) {
                continue;
            }
            ui::FillRounded(drawList, bar, SeriesColor(chart, static_cast<std::size_t>(seriesIndex), colors),
                            radius);
        }
    }
}

// 堆叠柱状图（老人年龄分布：男/女上下堆叠）
void DrawStackedBars(ImDrawList* drawList, const Rect& plot, const ChartPanelData& chart,
                     const AxisRange& axis, const theme::Palette& colors) {
    const int categoryCount = static_cast<int>(chart.categories.size());
    const int seriesCount = static_cast<int>(chart.series.size());
    if (categoryCount <= 0 || seriesCount <= 0) {
        return;
    }
    const float categoryWidth = plot.w / static_cast<float>(categoryCount);
    const float barWidth = std::max(1.0f, categoryWidth * 0.42f);
    const float radius = ClampF(barWidth * 0.18f, 1.0f, 3.0f);
    for (int categoryIndex = 0; categoryIndex < categoryCount; ++categoryIndex) {
        float cumulative = 0.0f;
        for (int seriesIndex = 0; seriesIndex < seriesCount; ++seriesIndex) {
            const auto& series = chart.series[static_cast<std::size_t>(seriesIndex)];
            if (static_cast<std::size_t>(categoryIndex) >= series.values.size()) {
                continue;
            }
            const float value = series.values[static_cast<std::size_t>(categoryIndex)];
            if (value <= 0.0f) {
                continue;
            }
            const float lowY = ClampF(ValueToY(plot, axis, cumulative), plot.y, plot.Bottom());
            const float highY = ClampF(ValueToY(plot, axis, cumulative + value), plot.y, plot.Bottom());
            const float x = plot.x + (static_cast<float>(categoryIndex) + 0.5f) * categoryWidth -
                            barWidth * 0.5f;
            const Rect segment{x, highY, barWidth, std::fabs(lowY - highY)};
            if (segment.h >= 0.5f) {
                ui::FillRounded(drawList, segment,
                                SeriesColor(chart, static_cast<std::size_t>(seriesIndex), colors), radius);
            }
            cumulative += value;
        }
    }
}

// 无时间 tab 的卡头附注（右对齐副标题，如「今日」）
void DrawCardHeaderNoteOnly(ImDrawList* drawList, const CardStyle& style, const std::string& note,
                            const theme::Palette& colors, float titleWidth) {
    if (note.empty()) {
        return;
    }
    const float noteSize = ClampF(style.card.h * 0.068f, 9.0f, 11.5f);
    ui::ScopedFontSize noteFont(noteSize);
    const Rect header{style.card.x + style.padding, style.card.y,
                      std::max(0.0f, style.card.w - style.padding * 2.0f), style.headerHeight};
    const float maxWidth = std::max(0.0f, header.w - titleWidth - style.padding * 1.5f);
    const std::string fitted = FitText(note, maxWidth);
    if (fitted.empty()) {
        return;
    }
    const float width = TextWidth(fitted.c_str());
    drawList->AddText(ImVec2(header.Right() - width, header.y + (header.h - noteSize) * 0.5f),
                      colors.textMuted, fitted.c_str());
}

// -----------------------------------------------------------------------------
// 图表面板通用绘制：卡头 / 时间 tab / 图例 / 坐标轴 / 图形
// -----------------------------------------------------------------------------
void DrawChartPanel(ImDrawList* drawList, const Rect& card, const ChartPanelData& chart,
                    const layout::LayoutTokens& tokens, const theme::Palette& colors,
                    const std::string& updateTime, bool showTimeTabs) {
    const CardStyle style = MakeCardStyle(card);
    const std::string period = chart.subtitle.empty() ? std::string("本周") : chart.subtitle;

    // ---- 卡头：标题 ----
    float titleWidth = 0.0f;
    {
        ui::ScopedBoldFont titleFont(style.titleSize);
        titleWidth = TextWidth(chart.title.c_str());
        drawList->AddText(ImVec2(style.card.x + style.padding,
                                 style.card.y + (style.headerHeight - style.titleSize) * 0.5f),
                          colors.textPrimary, chart.title.c_str());
    }

    if (showTimeTabs) {
        // 右侧日期区间框（浅灰底、圆角 2，Web §8.5）
        const float noteSize = ClampF(style.card.h * 0.065f, 9.0f, 11.0f);
        const std::string rangeText = CurrentDateRange(period, updateTime);
        float rangeWidth = 0.0f;
        {
            ui::ScopedFontSize rangeFont(noteSize);
            rangeWidth = TextWidth(rangeText.c_str());
        }
        const float boxHeight = ClampF(style.headerHeight * 0.62f, 14.0f, 26.0f);
        const float boxWidth = rangeWidth + style.padding * 1.1f;
        const Rect dateBox{style.card.Right() - style.padding - boxWidth,
                           style.card.y + (style.headerHeight - boxHeight) * 0.5f, boxWidth,
                           boxHeight};
        ui::FillRounded(drawList, dateBox, colors.lightBlock, 2.0f);
        {
            ui::ScopedFontSize rangeFont(noteSize);
            ui::TextCentered(drawList, dateBox, colors.textRegular, rangeText.c_str());
        }

        // 今日 / 本周 / 本月（Web §8.5：tab 在左侧紧跟标题，时间区间框在右侧；选中态青绿指示条）
        const char* periods[3] = {"今日", "本周", "本月"};
        const float tabFontSize = ClampF(style.titleSize * 0.78f, 10.0f, 12.5f);
        const float tabGap = ClampF(style.padding * 0.9f, 6.0f, 16.0f);
        const float tabTextY = style.card.y + (style.headerHeight - tabFontSize) * 0.5f;
        float tabsWidth = 0.0f;
        {
            ui::ScopedFontSize measureFont(tabFontSize);
            for (const char* text : periods) {
                tabsWidth += TextWidth(text);
            }
            tabsWidth += tabGap * 2.0f;
        }
        float tabsX = style.card.x + style.padding + titleWidth + style.padding * 1.2f;
        const float tabsLimit = dateBox.x - style.padding - tabsWidth;
        if (tabsX > tabsLimit) {
            tabsX = std::max(style.card.x + style.padding + titleWidth + style.padding, tabsLimit);
        }
        const float indicatorHeight = ClampF(style.card.h * 0.018f, 2.0f, 4.0f);
        for (const char* text : periods) {
            const bool selected = (period == text);
            float width = 0.0f;
            if (selected) {
                ui::ScopedBoldFont selectedFont(tabFontSize);
                width = TextWidth(text);
                drawList->AddText(ImVec2(tabsX, tabTextY), colors.textTitle, text);
                const float indicatorWidth = width * 1.06f;
                const Rect indicator{tabsX - (indicatorWidth - width) * 0.5f,
                                     tabTextY + tabFontSize + 2.0f, indicatorWidth,
                                     indicatorHeight};
                if (indicator.Bottom() < style.card.y + style.headerHeight) {
                    ui::FillRounded(drawList, indicator, colors.primary, indicatorHeight * 0.5f);
                }
            } else {
                ui::ScopedFontSize tabFont(tabFontSize);
                width = TextWidth(text);
                drawList->AddText(ImVec2(tabsX, tabTextY), colors.textRegular, text);
            }
            tabsX += width + tabGap;
        }
    } else {
        // 副标题附注：形如「今日」直接显示；"数据更新：<完整时间>" 只保留日期，避免小卡片拥挤
        std::string note = period;
        if (note.find("数据更新") != std::string::npos) {
            note = "数据更新：" + DisplayDate(updateTime);
        }
        DrawCardHeaderNoteOnly(drawList, style, note, colors, titleWidth);
    }

    // ---- 图例（右上，圆点 + 名称）----
    const float legendFontSize = ClampF(style.body.h * 0.085f, 9.0f, 12.0f);
    const float legendHeight = legendFontSize * 1.55f;
    std::vector<std::pair<ImU32, std::string>> legendItems;
    legendItems.reserve(chart.series.size());
    for (std::size_t index = 0; index < chart.series.size(); ++index) {
        legendItems.emplace_back(SeriesColor(chart, index, colors), chart.series[index].name);
    }
    DrawLegend(drawList, Rect{style.body.x, style.body.y, style.body.w, legendHeight}, legendItems,
               legendFontSize);

    // ---- y 轴范围（数值归一化）----
    AxisRange axis;
    const bool stacked = chart.kind == ChartKind::StackedBar;
    if (stacked) {
        float maxStack = 0.0f;
        const std::size_t categoryCount = chart.categories.size();
        for (std::size_t category = 0; category < categoryCount; ++category) {
            float sum = 0.0f;
            for (const auto& series : chart.series) {
                if (category < series.values.size()) {
                    sum += series.values[category];
                }
            }
            maxStack = std::max(maxStack, sum);
        }
        axis = MakeAxisRange(0.0f, maxStack, true);
    } else if (chart.kind == ChartKind::Bar) {
        float maxValue = 0.0f;
        for (const auto& series : chart.series) {
            for (float value : series.values) {
                maxValue = std::max(maxValue, value);
            }
        }
        axis = MakeAxisRange(0.0f, maxValue, true);
    } else {
        float minValue = 0.0f;
        float maxValue = 1.0f;
        bool first = true;
        for (const auto& series : chart.series) {
            for (float value : series.values) {
                if (first) {
                    minValue = value;
                    maxValue = value;
                    first = false;
                } else {
                    minValue = std::min(minValue, value);
                    maxValue = std::max(maxValue, value);
                }
            }
        }
        axis = MakeAxisRange(minValue, maxValue, false);
    }

    // ---- 绘图区（扣除 y 轴刻度宽 / 图例高 / x 轴标签高）----
    const float axisFontSize = ClampF(style.body.h * 0.075f, 9.0f, 12.0f);
    float yLabelWidth = 0.0f;
    {
        ui::ScopedFontSize axisFont(axisFontSize);
        char buffer[32];
        const int intervals = AxisIntervalCount(axis);
        for (int index = 0; index <= intervals; ++index) {
            const float value = axis.minValue + axis.step * static_cast<float>(index);
            FormatAxisTick(buffer, sizeof(buffer), value, axis.maxValue);
            yLabelWidth = std::max(yLabelWidth, TextWidth(buffer));
        }
    }
    yLabelWidth = ClampF(yLabelWidth + 7.0f, 16.0f, style.body.w * 0.24f);
    const float xLabelHeight = axisFontSize + 4.0f;
    const float topValuePad = axisFontSize * 0.55f;
    const Rect plot{style.body.x + yLabelWidth,
                    style.body.y + legendHeight + topValuePad,
                    std::max(0.0f, style.body.w - yLabelWidth),
                    std::max(0.0f, style.body.h - legendHeight - topValuePad - xLabelHeight)};
    if (plot.w <= 12.0f || plot.h <= 12.0f) {
        return;
    }

    const float lineThickness = ClampF(plot.h * 0.016f, 1.5f, 2.6f);
    DrawYGridAndLabels(drawList, plot, axis, axisFontSize, colors.borderAxis, colors.borderAxis,
                       colors.textRegular);
    DrawCategoryLabels(drawList, plot, chart.categories, chart.kind == ChartKind::Bar ||
                                                               chart.kind == ChartKind::StackedBar,
                       axisFontSize, colors.textRegular);

    switch (chart.kind) {
        case ChartKind::Line:
            DrawLineSeries(drawList, plot, chart, axis, colors, lineThickness);
            break;
        case ChartKind::Bar:
            DrawGroupedBars(drawList, plot, chart, axis, colors);
            break;
        case ChartKind::StackedBar:
        case ChartKind::Ring:
        default:
            DrawStackedBars(drawList, plot, chart, axis, colors);
            break;
    }
    (void)tokens;
}

// -----------------------------------------------------------------------------
// 面板 1：数据概览（环形统计卡）
// -----------------------------------------------------------------------------
void DrawStatOverview(ImDrawList* drawList, const Rect& card, const DashboardSnapshot& snapshot,
                      const theme::Palette& colors) {
    const CardStyle style = MakeCardStyle(card);
    DrawCardHeader(drawList, style, "数据概览", "数据更新：" + DisplayDate(snapshot.updateTime),
                   colors);
    if (snapshot.stats.empty()) {
        DrawEmptyHint(drawList, style.body, "暂无统计数据", colors);
        return;
    }

    const std::size_t count = snapshot.stats.size();
    const float cellWidth = style.body.w / static_cast<float>(count);
    const float nameSize = ClampF(style.body.h * 0.10f, 10.0f, 13.0f);
    const float subSize = ClampF(style.body.h * 0.082f, 8.5f, 11.0f);
    bool hasSubLabel = false;
    for (const StatCardData& stat : snapshot.stats) {
        if (!stat.subLabel.empty()) {
            hasSubLabel = true;
        }
    }
    const float textBlockHeight = nameSize + (hasSubLabel ? subSize + 2.0f : 0.0f) + 2.0f;
    const float ringAreaHeight = std::max(0.0f, style.body.h - textBlockHeight);
    const float containerSize = std::min(cellWidth * 0.86f, ringAreaHeight);
    if (containerSize < 10.0f) {
        return;
    }
    const float outerRadius = containerSize * 0.40f;
    const float thickness = containerSize * 0.075f;
    const float innerRadius = std::max(2.0f, outerRadius - thickness);

    for (std::size_t index = 0; index < count; ++index) {
        const StatCardData& stat = snapshot.stats[index];
        const float cellX = style.body.x + cellWidth * static_cast<float>(index);
        const ImVec2 center(cellX + cellWidth * 0.5f, style.body.y + ringAreaHeight * 0.5f);
        const std::size_t paletteIndex =
            static_cast<std::size_t>(std::abs(stat.accentIndex)) % 5u;
        const ImU32 accent = colors.ringMain[paletteIndex];

        // 浅色轨道 + 主色弧段（Web §8.3：环形 内径 65% / 外径 80%）
        drawList->AddCircleFilled(center, outerRadius, colors.ringTrack, 64);
        drawList->AddCircleFilled(center, innerRadius, colors.panel, 64);
        const float ratio = ClampF(stat.ratio, 0.0f, 1.0f);
        const float middleRadius = (outerRadius + innerRadius) * 0.5f;
        if (ratio >= 0.999f) {
            drawList->AddCircle(center, middleRadius, accent, 64, thickness);
        } else if (ratio > 0.002f) {
            drawList->PathArcTo(center, middleRadius, -kPi * 0.5f,
                                -kPi * 0.5f + kTwoPi * ratio, 64);
            drawList->PathStroke(accent, 0, thickness);
        }

        // 中心数值（含单位，按内圈宽度自适应字号）
        const std::string valueText = stat.value + stat.unit;
        float valueSize = ClampF(innerRadius * 0.62f, 9.0f, 22.0f);
        const float valueLimit = innerRadius * 1.8f;
        for (int guard = 0; guard < 24; ++guard) {
            ui::ScopedBoldFont probe(valueSize);
            if (TextWidth(valueText.c_str()) <= valueLimit || valueSize <= 8.5f) {
                break;
            }
            valueSize -= 0.5f;
        }
        {
            ui::ScopedBoldFont valueFont(valueSize);
            const float width = TextWidth(valueText.c_str());
            drawList->AddText(ImVec2(center.x - width * 0.5f, center.y - valueSize * 0.5f),
                              colors.textPrimary, valueText.c_str());
        }

        // 底部名称 + 副标签
        const float nameY = style.body.y + ringAreaHeight + 1.0f;
        {
            ui::ScopedBoldFont nameFont(nameSize);
            const std::string name = FitText(stat.title, cellWidth - 6.0f);
            const float width = TextWidth(name.c_str());
            drawList->AddText(ImVec2(cellX + (cellWidth - width) * 0.5f, nameY), colors.textPrimary,
                              name.c_str());
        }
        if (hasSubLabel && !stat.subLabel.empty()) {
            ui::ScopedFontSize subFont(subSize);
            const std::string sub = FitText(stat.subLabel, cellWidth - 6.0f);
            const float width = TextWidth(sub.c_str());
            drawList->AddText(ImVec2(cellX + (cellWidth - width) * 0.5f, nameY + nameSize + 1.5f),
                              colors.textMuted, sub.c_str());
        }
    }
}

// -----------------------------------------------------------------------------
// 面板 2：老人等级排行列表（名次徽标 + 名称 + 人数 + 占比条 + 占比文字）
// -----------------------------------------------------------------------------
ImU32 RankBadgeColor(int rank, const theme::Palette& colors) {
    switch (rank) {
        case 1:
            return colors.chartLineBlue;
        case 2:
            return colors.chartLineTeal;
        case 3:
            return colors.warning;
        default:
            return colors.textMuted;
    }
}

void DrawRankList(ImDrawList* drawList, const Rect& card, const DashboardSnapshot& snapshot,
                  const theme::Palette& colors) {
    const CardStyle style = MakeCardStyle(card);
    const int total = SumRankValues(snapshot.ranks);
    std::string note = "老人等级分布";
    if (total > 0) {
        note = "共 " + std::to_string(total) + " 人";
    }
    DrawCardHeader(drawList, style, "老人等级排行", note, colors);

    const std::vector<RankRowData>& ranks = snapshot.ranks;
    if (ranks.empty()) {
        DrawEmptyHint(drawList, style.body, "暂无排行数据", colors);
        return;
    }
    const std::size_t rowCount = ranks.size();
    const float rowHeight = style.body.h / static_cast<float>(rowCount);
    const float badgeDiameter = ClampF(rowHeight * 0.62f, 12.0f, 26.0f);
    const float nameFontSize = ClampF(rowHeight * 0.34f, 10.5f, 14.0f);
    const float smallFontSize = ClampF(nameFontSize * 0.86f, 9.0f, 12.0f);
    const float barHeight = ClampF(rowHeight * 0.16f, 3.0f, 7.0f);
    const float innerGap = ClampF(style.padding * 0.55f, 4.0f, 10.0f);

    for (std::size_t index = 0; index < rowCount; ++index) {
        const RankRowData& rank = ranks[index];
        const float rowY = style.body.y + rowHeight * static_cast<float>(index);
        const float centerY = rowY + rowHeight * 0.5f;
        const ImU32 badgeColor = RankBadgeColor(rank.rank, colors);

        // 名次徽标
        const ImVec2 badgeCenter(style.body.x + badgeDiameter * 0.5f, centerY);
        drawList->AddCircleFilled(badgeCenter, badgeDiameter * 0.5f, badgeColor, 24);
        {
            const float badgeFontSize = ClampF(badgeDiameter * 0.52f, 8.0f, 14.0f);
            ui::ScopedBoldFont badgeFont(badgeFontSize);
            char rankText[8];
            std::snprintf(rankText, sizeof(rankText), "%d", rank.rank);
            const float width = TextWidth(rankText);
            drawList->AddText(ImVec2(badgeCenter.x - width * 0.5f, centerY - badgeFontSize * 0.5f),
                              colors.textInverse, rankText);
        }

        // 名称（粗体）
        const float nameX = style.body.x + badgeDiameter + innerGap;
        float nameWidth = 0.0f;
        {
            ui::ScopedBoldFont nameFont(nameFontSize);
            const std::string name = FitText(rank.name, style.body.w * 0.34f);
            nameWidth = TextWidth(name.c_str());
            drawList->AddText(ImVec2(nameX, centerY - nameFontSize * 0.5f), colors.textPrimary,
                              name.c_str());
        }

        // 人数 + 占比文字（右侧对齐）；宽度不足时省略人数，保证进度条可读
        float percentWidth = 0.0f;
        float valueWidth = 0.0f;
        {
            ui::ScopedFontSize smallFont(smallFontSize);
            percentWidth = TextWidth(rank.subText.c_str());
            valueWidth = TextWidth(rank.valueText.c_str());
        }
        const float percentX = style.body.Right() - percentWidth;
        const float barStart = nameX + nameWidth + innerGap;
        const bool showValue =
            (percentX - valueWidth - innerGap * 2.0f - barStart) >= 44.0f;
        const float valueX = showValue ? (percentX - valueWidth - innerGap) : percentX;
        const float barEnd = showValue ? (valueX - innerGap) : (percentX - innerGap);
        const float barWidth = std::max(0.0f, barEnd - barStart);
        if (barWidth >= 24.0f) {
            const Rect barArea{barStart, centerY - barHeight * 0.5f, barWidth, barHeight};
            ui::FillRounded(drawList, barArea, colors.border, barHeight * 0.5f);
            const Rect fill{barArea.x, barArea.y, barArea.w * ClampF(rank.ratio, 0.0f, 1.0f),
                            barArea.h};
            if (fill.w >= 1.0f) {
                ui::FillRounded(drawList, fill, badgeColor, barHeight * 0.5f);
            }
        }
        if (showValue) {
            ui::ScopedFontSize smallFont(smallFontSize);
            drawList->AddText(ImVec2(valueX, centerY - smallFontSize * 0.5f), colors.textMuted,
                              rank.valueText.c_str());
        }
        {
            ui::ScopedFontSize smallFont(smallFontSize);
            drawList->AddText(ImVec2(percentX, centerY - smallFontSize * 0.5f), colors.textRegular,
                              rank.subText.c_str());
        }
    }
}

// 等级颜色（Web §8.7：特级 #47CBBA / 一级 #9891DF / 二级 #8EBEF3 / 三级 #FFB057）
ImU32 ElderLevelColor(int rank, const theme::Palette& colors) {
    const int index = std::max(0, std::min(3, 4 - rank));
    return colors.elderLevel[index];
}

// -----------------------------------------------------------------------------
// 面板 7：老人等级分布（环形 + 图例）
// -----------------------------------------------------------------------------
void DrawElderLevelRing(ImDrawList* drawList, const Rect& card, const DashboardSnapshot& snapshot,
                        const theme::Palette& colors) {
    const CardStyle style = MakeCardStyle(card);
    DrawCardHeader(drawList, style, "老人等级分布", "数据更新：" + DisplayDate(snapshot.updateTime),
                   colors);
    const std::vector<RankRowData>& ranks = snapshot.ranks;
    if (ranks.empty()) {
        DrawEmptyHint(drawList, style.body, "暂无等级数据", colors);
        return;
    }

    const Rect body = style.body;
    const float ringAreaWidth = body.w * 0.46f;
    const float container = std::min(ringAreaWidth, body.h) * 0.96f;
    if (container < 12.0f) {
        return;
    }
    const ImVec2 center(body.x + ringAreaWidth * 0.5f, body.CenterY());
    // Web §8.7：ECharts radius ['30%','60%']（相对 min(w,h)/2）→ 外径 60%、内径 30%
    const float outerRadius = container * 0.30f;
    const float innerRadius = container * 0.15f;
    const float thickness = std::max(2.0f, outerRadius - innerRadius);

    float ratioSum = 0.0f;
    for (const RankRowData& rank : ranks) {
        ratioSum += std::max(0.0f, rank.ratio);
    }
    if (ratioSum <= 0.0f) {
        ratioSum = static_cast<float>(ranks.size());
    }
    const float gapAngle = 0.012f;
    float startAngle = -kPi * 0.5f;
    for (const RankRowData& rank : ranks) {
        const float share = std::max(0.0f, rank.ratio) / ratioSum;
        const float sweep = share * kTwoPi;
        if (sweep <= 0.004f) {
            startAngle += sweep;
            continue;
        }
        const float angle0 = startAngle + gapAngle * 0.5f;
        const float angle1 = startAngle + sweep - gapAngle * 0.5f;
        if (angle1 > angle0) {
            drawList->PathArcTo(center, (outerRadius + innerRadius) * 0.5f, angle0, angle1, 40);
            drawList->PathStroke(ElderLevelColor(rank.rank, colors), 0, thickness);
        }
        startAngle += sweep;
    }

    // 中心总数（由各等级人数求和，不臆造数据）
    const int total = SumRankValues(ranks);
    if (total > 0 && innerRadius > 8.0f) {
        const std::string totalText = std::to_string(total);
        float valueSize = ClampF(innerRadius * 0.95f, 11.0f, 24.0f);
        for (int guard = 0; guard < 24; ++guard) {
            ui::ScopedBoldFont probe(valueSize);
            if (TextWidth(totalText.c_str()) <= innerRadius * 1.7f || valueSize <= 10.0f) {
                break;
            }
            valueSize -= 0.5f;
        }
        {
            ui::ScopedBoldFont valueFont(valueSize);
            const float width = TextWidth(totalText.c_str());
            drawList->AddText(ImVec2(center.x - width * 0.5f, center.y - valueSize * 0.5f - 4.0f),
                              colors.textPrimary, totalText.c_str());
        }
        ui::ScopedFontSize unitFont(ClampF(innerRadius * 0.35f, 8.0f, 11.0f));
        const float unitWidth = TextWidth("人");
        drawList->AddText(ImVec2(center.x - unitWidth * 0.5f, center.y + valueSize * 0.5f - 3.0f),
                          colors.textMuted, "人");
    }

    // 右侧图例（圆点 + 等级名 + 人数）
    const Rect legend{body.x + body.w * 0.50f, body.y, body.w * 0.50f, body.h};
    const std::size_t rowCount = ranks.size();
    const float rowHeight = legend.h / static_cast<float>(rowCount);
    const float nameFontSize = ClampF(rowHeight * 0.30f, 9.5f, 13.0f);
    const float dotRadius = ClampF(rowHeight * 0.075f, 2.0f, 4.5f);
    for (std::size_t index = 0; index < rowCount; ++index) {
        const RankRowData& rank = ranks[index];
        const float centerY = legend.y + rowHeight * (static_cast<float>(index) + 0.5f);
        const float dotX = legend.x + dotRadius;
        drawList->AddCircleFilled(ImVec2(dotX, centerY), dotRadius,
                                  ElderLevelColor(rank.rank, colors), 10);
        float valueWidth = 0.0f;
        {
            ui::ScopedFontSize valueFont(ClampF(nameFontSize * 0.88f, 8.5f, 11.5f));
            valueWidth = TextWidth(rank.valueText.c_str());
            drawList->AddText(ImVec2(legend.Right() - valueWidth, centerY - nameFontSize * 0.5f),
                              colors.textMuted, rank.valueText.c_str());
        }
        const float nameX = dotX + dotRadius + ClampF(dotRadius * 2.0f, 4.0f, 8.0f);
        const float maxNameWidth = std::max(0.0f, legend.Right() - valueWidth - nameX - 4.0f);
        ui::ScopedFontSize nameFont(nameFontSize);
        const std::string name = FitText(rank.name, maxNameWidth);
        drawList->AddText(ImVec2(nameX, centerY - nameFontSize * 0.5f), colors.textRegular,
                          name.c_str());
    }
}

// -----------------------------------------------------------------------------
// 面板分派（frame 为屏幕坐标：已叠加内容原点与滚动偏移）
// -----------------------------------------------------------------------------
void DrawPanelContent(ImDrawList* drawList, const DashboardSnapshot& snapshot,
                      DashPanelKind kind, const Rect& frame, const layout::LayoutTokens& tokens,
                      const theme::Palette& colors, const std::string& updateTime) {
    switch (kind) {
        case DashPanelKind::StatOverview:
            DrawStatOverview(drawList, frame, snapshot, colors);
            break;
        case DashPanelKind::RankList:
            DrawRankList(drawList, frame, snapshot, colors);
            break;
        case DashPanelKind::ChartEarnings:
            if (snapshot.charts.size() > 0) {
                DrawChartPanel(drawList, frame, snapshot.charts[0], tokens, colors, updateTime, true);
            }
            break;
        case DashPanelKind::ChartCheckIn:
            if (snapshot.charts.size() > 1) {
                DrawChartPanel(drawList, frame, snapshot.charts[1], tokens, colors, updateTime, false);
            }
            break;
        case DashPanelKind::ChartService:
            if (snapshot.charts.size() > 2) {
                DrawChartPanel(drawList, frame, snapshot.charts[2], tokens, colors, updateTime, false);
            }
            break;
        case DashPanelKind::ChartAge:
            if (snapshot.charts.size() > 3) {
                DrawChartPanel(drawList, frame, snapshot.charts[3], tokens, colors, updateTime, false);
            }
            break;
        case DashPanelKind::ElderLevelRing:
            DrawElderLevelRing(drawList, frame, snapshot, colors);
            break;
        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// 布局：按 Web §8.1 公式计算 7 个面板的内容坐标矩形
// -----------------------------------------------------------------------------
struct DashboardGrid {
    Rect statOverview;
    Rect rankList;
    Rect earnings;
    Rect checkIn;
    Rect service;
    Rect age;
    Rect elderRing;
    float contentWidth = 0.0f;
    float contentHeight = 0.0f;
    bool singleColumn = false;
};

DashboardGrid BuildWideGrid(const layout::ShellLayout& shell) {
    DashboardGrid grid;
    const layout::LayoutTokens& tokens = shell.tokens;
    const Rect viewport = shell.pageViewport;
    const float rowGap = tokens.rowGap;
    const float twoGap = tokens.twoColumnGap;
    const float threeGap = tokens.threeColumnGap;

    // 行高：237:278:278 比例铺满可视高度（1280×720 一屏放下、无滚动）
    const float usableHeight = std::max(0.0f, viewport.h - rowGap * 2.0f);
    const float weightSum = kRowWeightTop + kRowWeightChart * 2.0f;
    const float unit = weightSum > 0.0f ? usableHeight / weightSum : 0.0f;
    const float topHeight = unit * kRowWeightTop;
    const float chartHeight = unit * kRowWeightChart;

    // 两栏 17:7（Web §8.1：rowW2 = A + 24；left = rowW2*17/24 - 24；right = rowW2*7/24 - 24）
    const float rowWidth2 = viewport.w + twoGap;
    const float leftWidth = rowWidth2 * kTwoColumnRatioA - twoGap;
    const float rightWidth = rowWidth2 * kTwoColumnRatioB - twoGap;
    // 三栏 7:7:10（Web §8.1：rowW3 = A + 12；c1 = rowW3*7/24 - 12；c3 = rowW3*10/24 - 12）
    const float rowWidth3 = viewport.w + threeGap;
    const float column1 = std::max(0.0f, rowWidth3 * kThreeColumnRatioA - threeGap);
    const float column2 = column1;
    const float column3 = std::max(0.0f, rowWidth3 * kThreeColumnRatioC - threeGap);

    const float row2Y = topHeight + rowGap;
    const float row3Y = row2Y + chartHeight + rowGap;

    grid.statOverview = Rect{0.0f, 0.0f, leftWidth, topHeight};
    grid.rankList = Rect{leftWidth + twoGap, 0.0f, rightWidth, topHeight};
    grid.earnings = Rect{0.0f, row2Y, leftWidth, chartHeight};
    grid.checkIn = Rect{leftWidth + twoGap, row2Y, rightWidth, chartHeight};
    grid.service = Rect{0.0f, row3Y, column1, chartHeight};
    grid.age = Rect{column1 + threeGap, row3Y, column2, chartHeight};
    grid.elderRing = Rect{column1 + column2 + threeGap * 2.0f, row3Y, column3, chartHeight};
    grid.contentWidth = viewport.w;
    grid.contentHeight = row3Y + chartHeight;
    grid.singleColumn = false;
    return grid;
}

DashboardGrid BuildNarrowGrid(const layout::ShellLayout& shell, float contentWidth) {
    DashboardGrid grid;
    const layout::LayoutTokens& tokens = shell.tokens;
    const float rowGap = tokens.rowGap;
    float y = 0.0f;
    const auto place = [&y, rowGap, contentWidth](float height) {
        const Rect rect{0.0f, y, contentWidth, height};
        y += height + rowGap;
        return rect;
    };
    // 单列顺序：数据概览 → 等级排行 → 收益 → 入退 → 服务 → 年龄 → 等级分布
    grid.statOverview = place(kNarrowStatCardHeight);
    grid.rankList = place(kNarrowStatCardHeight);
    grid.earnings = place(kNarrowPanelHeight);
    grid.checkIn = place(kNarrowPanelHeight);
    grid.service = place(kNarrowPanelHeight);
    grid.age = place(kNarrowPanelHeight);
    grid.elderRing = place(kNarrowPanelHeight);
    grid.contentWidth = contentWidth;
    grid.contentHeight = std::max(0.0f, y - rowGap);
    grid.singleColumn = true;
    return grid;
}

DashboardGrid BuildGrid(const layout::ShellLayout& shell, bool singleColumn, float scrollbarWidth) {
    if (!singleColumn) {
        return BuildWideGrid(shell);
    }
    // 单列：内容高固定（237 + 6×278 + 间隔），必然超出可视区 → 预留滚动条宽度
    DashboardGrid grid = BuildNarrowGrid(shell, std::max(0.0f, shell.pageViewport.w - scrollbarWidth));
    return grid;
}

Rect FrameForKind(const DashboardGrid& grid, DashPanelKind kind) {
    switch (kind) {
        case DashPanelKind::StatOverview:
            return grid.statOverview;
        case DashPanelKind::RankList:
            return grid.rankList;
        case DashPanelKind::ChartEarnings:
            return grid.earnings;
        case DashPanelKind::ChartCheckIn:
            return grid.checkIn;
        case DashPanelKind::ChartService:
            return grid.service;
        case DashPanelKind::ChartAge:
            return grid.age;
        case DashPanelKind::ElderLevelRing:
            return grid.elderRing;
        default:
            return Rect{};
    }
}

// -----------------------------------------------------------------------------
// DashboardLayoutSystem（Layout 阶段，order 10）
// -----------------------------------------------------------------------------
class DashboardLayoutSystem final : public ISystem {
public:
    const char* Name() const override { return "DashboardLayoutSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Layout; }

    void Update(World& world, const FrameContext& /*frame*/) override {
        const RouteState* route = world.TryResource<RouteState>();
        if (route == nullptr || route->current != Route::Dashboard) {
            return;
        }
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        const WindowState* window = world.TryResource<WindowState>();
        DashboardLayoutState* state = world.TryResource<DashboardLayoutState>();
        if (shell == nullptr || window == nullptr || state == nullptr) {
            return;
        }

        const bool singleColumn =
            static_cast<float>(window->width) < kNarrowBreakpointWidth;
        const float scrollbarWidth = ImGui::GetStyle().ScrollbarSize;
        const DashboardGrid grid = BuildGrid(*shell, singleColumn, scrollbarWidth);

        state->contentWidth = grid.contentWidth;
        state->contentHeight = grid.contentHeight;
        state->singleColumn = grid.singleColumn;
        state->scrollable = grid.contentHeight > shell->pageViewport.h + 0.5f;

        world.Each<DashPanelView>([&grid](Entity /*entity*/, DashPanelView& view) {
            view.frame = FrameForKind(grid, view.kind);
        });
    }
};

// -----------------------------------------------------------------------------
// DashboardRenderSystem（Render 阶段，order 200）
// 只读 DashboardData + 布局结果；滚动由 ImGui 子窗口承担，绘制本身无任何状态写入。
// -----------------------------------------------------------------------------
class DashboardRenderSystem final : public ISystem {
public:
    const char* Name() const override { return "DashboardRenderSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& /*frame*/) override {
        const RouteState* route = world.TryResource<RouteState>();
        if (route == nullptr || route->current != Route::Dashboard) {
            return;
        }
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        const DashboardSnapshot* snapshot = world.TryResource<DashboardData>();
        const DashboardLayoutState* state = world.TryResource<DashboardLayoutState>();
        if (shell == nullptr || snapshot == nullptr || state == nullptr) {
            return;
        }
        const Rect page = shell->pageViewport;
        if (page.Empty()) {
            return;
        }
        const theme::Palette& colors = theme::Colors();
        const layout::LayoutTokens& tokens = shell->tokens;

        // 内容区固定窗口 + 子窗口纵向滚动容器（滚动条样式由主题统一：6px / #F1F1F1 / #C0C0C0）
        if (!ui::BeginFixedWindow("##dashboard_page", page)) {
            ui::EndFixedWindow();
            return;
        }
        ImGui::BeginChild("##dashboard_scroll", ImVec2(page.w, page.h), 0,
                          ImGuiWindowFlags_NoBackground);
        // 内容原点（已叠加子窗口滚动偏移）：屏幕坐标 = 原点 + 内容坐标
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(std::max(1.0f, state->contentWidth),
                            std::max(1.0f, state->contentHeight)));
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        world.Each<DashPanelView>([&](Entity /*entity*/, DashPanelView& view) {
            const Rect frame{origin.x + view.frame.x, origin.y + view.frame.y, view.frame.w,
                             view.frame.h};
            if (frame.Empty() || frame.Bottom() < page.y || frame.y > page.Bottom()) {
                return;
            }
            // 卡片底板（含投影）先画，再裁剪内容，防止文字/图形溢出卡片
            ui::Card(drawList, frame, tokens.cardRadius);
            drawList->PushClipRect(ui::Min(frame), ui::Max(frame), true);
            DrawPanelContent(drawList, *snapshot, view.kind, frame, tokens, colors,
                             snapshot->updateTime);
            drawList->PopClipRect();
        });

        ImGui::EndChild();
        ui::EndFixedWindow();
    }
};

}  // namespace

void RegisterDashboardModule(World& world, SystemScheduler& scheduler) {
    // 7 个面板实体：AnchorLayout 统一锚定 pageViewport（表达"面板属于页面内容区"），
    // 行/列细分由 DashboardLayoutSystem 按 Web §8.1 公式计算，不使用绝对坐标。
    const DashPanelKind kinds[] = {
        DashPanelKind::StatOverview, DashPanelKind::RankList,    DashPanelKind::ChartEarnings,
        DashPanelKind::ChartCheckIn, DashPanelKind::ChartService, DashPanelKind::ChartAge,
        DashPanelKind::ElderLevelRing,
    };
    for (DashPanelKind kind : kinds) {
        const Entity entity = world.CreateEntity();
        AnchorLayout anchor;
        anchor.space = AnchorSpace::PageViewport;
        anchor.x = 0.0f;
        anchor.y = 0.0f;
        anchor.w = 1.0f;
        anchor.h = 1.0f;
        world.Add<AnchorLayout>(entity, anchor);
        DashPanelView view;
        view.kind = kind;
        world.Add<DashPanelView>(entity, view);
    }

    world.EmplaceResource<DashboardLayoutState>();

    // 布局在架构 LayoutSystem（order 0）之后执行；渲染与外壳/登录页同阶段（order 200）
    scheduler.Add(std::make_unique<DashboardLayoutSystem>(), 10);
    scheduler.Add(std::make_unique<DashboardRenderSystem>(), 200);
}

}  // namespace zzyl
