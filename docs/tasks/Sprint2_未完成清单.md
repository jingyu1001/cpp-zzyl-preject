# Sprint 2 未完成清单（2026-11-24 晚 · 遗留三项已清零归档）

> **状态：Sprint 2 遗留收尾三项全部清零 ✅**（tester **v5 终固报告**：用例 65 / PASS 65 / FAIL 0 / 待补证 0 / 未覆盖 0 / P0·P1=0；结论 **可交付**）。
> 本清单原记录 14:00 硬截止时的证据/覆盖缺口；**第 1~3 项已于本晚闭环**，第 4~8 项转为 Sprint 3 输入（非本次收尾项）。
> 报告：`docs/test/Sprint2_测试报告.md`（v5）；证据：`artifacts/test_tester_s2/closeout/`、`artifacts/selftest_s2_empty/`、`artifacts/test_tester_s2/fix_verify/`。

## 一、遗留三项收尾（全部 ✅ 已清零）

| # | 事项 | 最终状态 | 证据 | 说明 |
|---|---|---|---|---|
| 1 | **EV-01 M2/M3 删除流程** | ✅ **PASS（清零）**：M2 房型 6 条 → 删除行1 → 确认框→确定→Toast → **5 条**；M3 护理 9 条 → 横滚→删除行1→确认→Toast → **8 条**（首行变原行2） | `artifacts/test_tester_s2/closeout/m2_00~m2_02.png`、`m3_00~m3_03.png` | tester 实机补证（DQ-0005）；此前 v4 失败为采集/定位问题（坐标偏差/绿色阈值误判照片），**非代码缺陷** |
| 2 | **EV-02 真 1920×1080** | ✅ **PASS（清零）**：M2 列表/弹窗、M3 列表/弹窗均 1920×1080、SHA 不同、F11 真全屏（无标题栏/F1 浮层）；M3 1920 下 10 列全可见、弹窗 500 居中不超屏 | `artifacts/test_tester_s2/fix_verify/v04/v06/v11/v13`、`sheet_1920_review.png` | tester v5 逐页复核 |
| 3 | **M0 表格空态** | ✅ **PASS（清零）**：新增测试钩子 `--empty-data`（默认关闭、行为不变）；三页空态=表头完整 + 居中「暂无数据」+ 分页「共 0 条」；默认行为未变（对照 `selftest_s2_closeout`） | `artifacts/selftest_s2_empty/05/06/07_*.png`、`metrics.json`（`emptyData=true`）；`artifacts/selftest_s2_closeout/`（对照） | 实现：`App.h/main.cpp` 解析 → `ModuleRegistry` 传参 → `SeedServices` 空集合；Release 构建通过、隐藏自测 8 帧 |

### 收尾实现记录（架构范围改动，供审计）
- 改动文件：`include/zzyl/app/App.h`（Options 增加 `emptyData`）、`src/main.cpp`（`--empty-data` 解析）、`include/zzyl/app/ModuleRegistry.h` + `src/app/ModuleRegistry.cpp`（传参）、`include/zzyl/data/SeedServices.h` + `src/data/SeedServices.cpp`（空集合模式）、`src/app/App.cpp`（启动日志/metrics 增加 `emptyData` 标记）；
- 构建/验证命令：
  ```bat
  cmake --build build --config Release --parallel
  build\Release\zzyl_desktop.exe --selftest artifacts\selftest_s2_closeout
  build\Release\zzyl_desktop.exe --selftest artifacts\selftest_s2_empty --empty-data
  build\Release\zzyl_desktop.exe --auto-login --empty-data   :: 组合兼容
  ```
- 边界：未改 dev 页面文件/Widgets/CMake 逻辑；未 commit；未占用前台（隐藏 selftest）。

## 二、Sprint 3 输入（非本次收尾项，仅保留决策/规划）

| # | 事项 | 当前状态 | 建议 |
|---|---|---|---|
| 4 | **Sprint 3 范围决策**（M4 入住办理 / 业务页扩展） | ⚪ 待 lead 决策 | 建议优先 M4 入住办理（列表+详情多分区）；范围见 `docs/plan/Sprint2_预备事项清单.md` |
| 5 | **SQLite / MySQL 接入** | ⚪ 未接（本迭代种子数据） | 需 lead 批准安装：SQLite amalgamation 放 `third_party/`；MySQL 优先走 HTTP（WinHTTP 零依赖）或 Connector 安装；依赖清单见预备事项清单 |
| 6 | **图片上传 / 素材库完善 / 房型图片** | ⚪ 本期裁剪（D-23） | WIC 已就绪，无需第三方库；本地文件选择 + 资源进 `assets/` |
| 7 | **导出/导入/列设置/批量操作** | ⚪ 本期置灰/Toast（D-24/D-25/D-30） | 导出 CSV 优先 |
| 8 | 后端缺 Controller/表（外部依赖） | ⚪ 外部阻塞 | 老师/后端补 SQL 与接口；C++ 端按预备事项清单接 HTTP |
| 9 | 测试工具化建议（tester §6） | ⚪ 非阻塞 | 1920 基线快照工具化、按钮定位工具化、SQLite 后回归删除与空态持久化 |

## 三、归档说明

- 本清单原第 1~3 项（EV-01/EV-02/M0 空态）**已全部 PASS 并归档**，Sprint 2 遗留清零；
- 架构 v0.2 §11.3、测试报告 v5 §2/§5.1/§5.3 已同步；
- Sprint 3 启动时，本清单「二、Sprint 3 输入」作为范围/依赖决策的输入即可。
