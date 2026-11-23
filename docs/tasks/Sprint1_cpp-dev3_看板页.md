# Sprint 1 任务单 · cpp-dev3 · 首页看板（统计卡 + 图表自绘 + 排行列表）

> 负责人：cpp-dev3
> 唯一可写文件：`src/pages/DashboardModule.cpp`（可新增 `include/zzyl/pages/DashboardTypes.h` 头文件）
> 必读：`docs/architecture/Cpp架构设计_v0.1.md` 第 6/7/8 章；Web 端《UI复刻规格_Cpp_1280x720_v0.1.md》§8（首页看板规格）+ 截图 `assets/02/03/04/05/07_*.png`
> 骨架现状：ECS 已创建 9 个占位面板（4 统计卡 + 4 图表 + 1 排行列表），`AnchorLayout` 比例锚定到 `pageViewport`，`LayoutSystem` 每帧解算 `ScreenRect`，`DashboardRenderSystem` 用 `ui::PlaceholderPanel` 画占位。**你的任务：读 `DashboardData`，用 ImDrawList 把看板画成正式页面。**

## 0. 口径与优先级（lead 已拍板 2026-11-23）

| 级别 | 内容 | 说明 |
|---|---|---|
| **P1（优先，主口径）** | **Web 真实页面复刻**：5 个环形统计卡（老人/床位/服务单/员工/收入）+ 4 图表（收益折线、入退柱状、服务折线、年龄堆叠柱）+ 4 行等级排行 + **内容区纵向滚动**；数据驱动（stats 数量自适应，不写死 4/5） | 按 Web UI 规格 §8.3/§8.5/§8.7/§8.8 实现；本迭代必须交付 |
| P1.5（时间够再补） | 个人卡 / 常用功能 8 宫格 / 老人等级·年龄·预约总览三卡 | 不阻塞本迭代验收 |
| 数据 | `MockDashboardService` 已按 **B 套**（2026-11-23 实测）补齐 5 项统计，顺序与 Web 一致；直接读 `DashboardData`，不要写死数值 | 已完成（架构师） |

> 结论：按 Web 真实页面 P1 实现；环形数量跟随 `data->stats.size()` 自适应（当前 5 个）。

## 1. P1 目标（验收画面，1280×720）

- **5 个环形统计卡**（Web §8.3 B 套，顺序固定）：老人数量 `655 人`（在院 644 · 外出 11）、床位数量 `812 个`（入住 655 · 空闲 157）、服务单数量 `100.35 w笔`（计划内 94.18w · 计划外 6.17w）、员工数量 `204 人`（普通员工 186 · 管理层 18）、收入金额 `410.81 w元`（服务费用 92.85w · 月度费用 317.96w）；环形 65%~80% 空心、浅色轨道 #D7F1ED + 主弧 `ringMain[accentIndex]`、中心数值 18px + 名称 14px #909399；
- **4 图表**（全部 ImDrawList 自绘，配色用 `theme::Colors()` 的 ECharts 色板）：
  1. `收益情况`：双折线（收入 #4D8DFF / 退款 #00B8A0），x 轴 7 天，y 轴数值，浅灰虚线网格，轴文字 12px #595959；
  2. `入退情况`：分组柱状（入住 #4D8DFF / 退住 #00B8A0），x 轴 24 小时（标签抽稀），柱宽约 10；
  3. `服务情况`：单折线 #00B8A0，x 轴 24 小时；
  4. `老人年龄分布`：堆叠柱（男 #27C2AE / 女 #FE6E6E），5 个年龄段标签完整显示，图例在底部（6px 圆点 + 文字）；
- **4 行等级排行**（B 套）：三级 331 人 50.5%、二级 191 人 29.2%、一级 91 人 13.9%、特级 42 人 6.4%（名次徽标 + 名称 + 人数 + 占比条 + 占比文字）；
- **内容区纵向滚动**：1280×720 下卡片按 Web 规格行高（统计卡 237、图表 278、排行 278）排布、内容总高约 901，超出可视区可滚动；滚动条 6px（#C0C0C0/#F1F1F1），页面顶部/底部 padding 24/52；F11 与 1024×640 下同样可滚动、不重叠；
- 数值全部来自 `DashboardData`；1024×640 / 1920×1080 下卡片与图表按布局重排、无重叠、标签不溢出；中文正常。

## 2. 数据（只读资源 `DashboardData`）
```cpp
const data::DashboardSnapshot* data = world.TryResource<DashboardData>();
// data->welcome / subtitle / updateTime
// data->stats[i]  = { title, value, unit, subLabel, ratio(0..1), accentIndex(0..4) }
// data->charts[i] = { title, subtitle, kind, categories[], series[] }
//   kind: ChartKind::Line / Bar / StackedBar / Ring（当前 Mock 4 项：Line/Bar/Line/StackedBar）
// data->ranks[i]  = { rank, name, valueText, subText, ratio(0..1) }
```
不要直接调用数据服务、不要修改数据资源；数据由 `DataBootstrapSystem` 启动时加载。

## 3. 实现方式（在骨架文件里改）

- 保留 ECS 结构：每个面板 = 实体 + `AnchorLayout` + 自己定义的面板组件（`StatCardView{int dataIndex;}`、`ChartPanelView{int dataIndex;}`、`RankListView{}`）。比例锚定沿用骨架（4 卡一行 0.25 宽 / 4 图 2×2 / 排行右列）；可微调比例，但**不要改成绝对坐标**。
- `DashboardRenderSystem`（Render，order 200）：`RouteState != Dashboard` 直接 return；遍历 `组件 + ScreenRect` 绘制。
- 卡片底板：`ui::Card(drawList, rect, shell->tokens.cardRadius)`（4px 圆角 + 极浅阴影）；卡头标题 16 粗体 #303133；所有内容 `PushClipRect` 卡内裁剪。
- 图表自绘建议（实现在本文件内）：
  - 坐标映射：`plot` = 卡片内去掉标题/图例后的区域；`y = plot.Bottom() - (v - min)/(max - min) * plot.Height`；`max` 取 nice 值（1/2/5×10^n 向上取整），`max==min` 时保护；
  - 折线：`AddPolyline` 2px，5 条水平虚线网格 `AddLine`（#DCDCDC，alpha 0.8），x 轴标签抽稀；
  - 分组柱：每类目 2 根，柱宽 = 类目宽×0.32，圆角 2；
  - 堆叠柱：图例在底部，男/女上下堆叠；
  - 环形（如果做 P1）：`PathArcTo` 分段弧 + 中心两行文字；P0 的“环形”由统计卡进度环/排行占比条覆盖也可（P0 无 Ring 数据）。
- 统计卡：标题 14 #303133，数值 28 粗体（主色或文字色）、单位 14、副标签 12 #999；进度条高 6、圆角 3，背景 #F0F0F0、前景 `ringMain[accentIndex]` 或 `chartPalette`。
- 排行：行高按卡片高比例；名次 1~3 徽标用 `ringMain[0..2]`，4 用灰；占比条背景 `borderLight`、前景 `ringMain[0]`；文字 12~14。
- 字体作用域用 `ui::ScopedFontSize/ScopedBoldFont`，在 `EndFixedWindow` 之前结束（本项目用背景 DrawList 绘制，无窗口，也仍需配对）。

## 4. 硬性约束

- 只改 `src/pages/DashboardModule.cpp`（新增头文件仅限 `include/zzyl/pages/DashboardTypes.h`）。
- 渲染层只读 `DashboardData`；不调用数据服务、不改 `AuthSession/RouteState`；不修改 `AppState.h`。
- 不写死 1280/720 或卡片绝对坐标：比例锚定 + 从卡片矩形推导；圆角/间距用 tokens（`shell->tokens.cardRadius` 等）。
- 颜色用 `theme::Colors()`（`chartLineBlue/chartLineTeal/ringTrack/ringMain/elderLevel/ageMale/ageFemale/textMuted/borderAxis`），不写裸色值。
- PushClipRect/PopClipRect 必须配对；所有字体作用域在 ImGui 帧内结束。
- 骨架的 `DashboardPanelView`/`ui::PlaceholderPanel` 可替换/删除；`ui::PlaceholderPanel` 是架构层工具，保留不动即可。

## 5. 自测与验收

```bat
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject\build" --config Release --parallel
build\Release\zzyl_desktop.exe --auto-login          :: 查看交互/视觉
build\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev3
```
- 检查 4 卡数值/副标签；4 图类型与配色；排行 4 行与占比；
- 1024×640 / F11 全屏：卡片不重叠、x 轴标签不糊、图例不溢出；
- 截图 `artifacts\selftest_dev3\02_dashboard_1280x720.png`（BMP 转 PNG 见 README），与 Web `assets/02_首页看板_1280x720.png` 对比（P0 允许结构差异：总控范围的 4+4+排行 vs Web 5+1+1+3；配色/卡片风格需一致）。
- 汇报 lead：完成内容、改动文件、编译/自测、截图路径、P1 是否已具备条件。

## 6. 提示 / 常见坑

- `StatCardData.value` 已是展示文本（如 "100.05"），不要自己四舍五入。
- 24 小时类目多，x 轴标签必须抽稀（如每 3 个或 4 个显示一个），否则糊成一片。
- 金额 y 轴可简写（`1.4w`），可选；坐标轴数字 12px。
- 折线 7 个点要平滑可省略（直线连接即可），但点圆点/线宽要清晰（2px）。
- 环形图（P1）记得 `PathArcTo` 前 `PathClear`，角度从 225° 起始（Web 规格）。
- 若数据字段不够（P1 需要 5 环形/个人卡/预约数据），先汇报给 lead/架构师，不要自己改 Mock 数据结构。
