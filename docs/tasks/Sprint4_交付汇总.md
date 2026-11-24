# Sprint 4 交付汇总（2026-11-24 · 待 lead 最终验收/GitHub 提交）

> 迭代：Sprint 4（21:30 开工）——M5 护理计划 + M6 护理等级 + M7 床位预览 + M4 数据链增强（floor/room/bed + 床位级联 + 图片/PDF 上传）+ 房型本地上传 +（可选）CSV
> 架构基线：`docs/architecture/Cpp架构设计_v0.4.md`；测试报告：`docs/test/Sprint4_测试报告.md`（tester 首轮 v1）；集成包：`docs/test/Sprint4_集成验证包.md`
> **结论：✅ 可交付（tester 首轮通过）** —— 用例 56：PASS 39 + PASS* 14 + 覆盖缺口 3 / **FAIL 0** / **P0·P1 = 0**；两个 P2 缺陷（BUG-S4-01/02）已修复并复测关闭；3 条覆盖缺口非阻塞。
> 未 commit（按约定 lead 统一提交）。

## 1. 交付范围与状态
| 模块 | 内容 | 状态 | 关键证据 |
|---|---|---|---|
| **M5 护理计划** | 列表 5 列（3 条种子，被引用行仅查看/禁用）+ 新增/编辑弹窗 1000×614（名称/状态/排序 + 项目子表 5 列/300 滚动/去重）+ 删除/启停 + CSV | ✅ | selftest 18；dev2 `w31~w34`；tester `liveA` |
| **M6 护理等级** | 列表 8 列（1500/2000/3000，挂计划 133/134/135）+ 弹窗 500×432 5 字段全必填 + CRUD/启停 | ✅ | selftest 19；tester `liveA/D`；BUG-S4-02 修复后 4 字段就地红字 |
| **M7 床位预览** | 11 楼层 Tab（#008D71+下划线/横向滚动）、三态图例、房间卡/床位卡（2 列，hover 操作）、点击床位只读详情；**CRUD 本轮 P1**（按钮外观+Toast「后续迭代接入」） | ✅ P0 | selftest 20；dev1 `s4_m7_qa/m7v_*`；tester `liveD` |
| **M4 数据链** | SQLite v5 floors 11/rooms 60/beds 89 行（有效 87）；入住表单楼层→房间→**只空闲床**级联 + 床位费带出；check_ins 级联快照 | ✅ | 数据日志 7/6/9/5/3/3/11/60/87；tester 渲染/合同 PASS；级联交互 GAP-01（非阻塞） |
| **上传** | 平台层应用内文件浏览器（原生对话框因本机 Shell 挂起废弃）：图片 5MB/2MB、PDF 2MB、中文路径、取消/Esc；入住 3 图+PDF、房型本地文件（素材库保留） | ✅ | tester BUG-S4-01 全链路 PASS（7 条重启、中文名）；`platform/` 证据 |
| **房型图片/CSV** | 5 张真实图+无图占位；房型导出 CSV（可选，tester GAP-04 未点） | ✅/可选 | dev3 证据 |
| **回归** | Sprint1~3 + 1280/1024/1920 + 中文 + WAL/空态 | ✅ | tester 回归组、21 帧 |

## 2. 测试结论（tester 首轮 v1，22:37）
- 用例 **56**：PASS **39**（实机/自跑）+ PASS* **14**（采信 dev/arch 证据+抽检）+ 覆盖缺口 3；**FAIL 0 / P0·P1=0**；
- 共享构建 **22:31:52**（2,626,560B；含 dev2 BUG-S4-02、arch FileDialog 编码修复、v5 数据层）；隔离库/空态/稳定性 6 连通过、stderr 0；
- 缺陷：BUG-S4-01（上传中文名编码）/BUG-S4-02（M6 校验红字位置）→ 均修复 + tester 复测 **PASS 关闭**；
- footer 疑点 → **P3 测试截图时序伪影**（真实窗口 0/300/900ms 三帧一致、按钮可点、取消可关闭），非缺陷不派单；
- 覆盖缺口（非阻塞，建议下一轮 5~8 分钟）：GAP-01 M4 级联交互联动、GAP-02 M6 CRUD/重启持久化、GAP-03 M5/M6 1024/1920 逐像素、GAP-04 可选 CSV 核验。

## 3. 变更清单（按负责人，供提交参考）
| 负责人 | 文件/范围 |
|---|---|
| cpp-arch | `CMakeLists.txt`；`src/data/SqliteData.cpp`（v5 迁移/种子/服务/checkpoint）、`src/data/Sprint4SeedData.inc`（Web MySQL 只读导出 172 条）、`include/zzyl/data/{BusinessServices.h,SqliteData.h,SeedServices.h}`、`src/data/SeedServices.cpp`；`include/zzyl/platform/FileDialog.h` + `src/platform/FileDialog.cpp`（应用内浏览器）；`src/main.cpp`、`src/app/{App.cpp,AppState.h,ModuleRegistry.*,Routes.*,ModuleHooks.h}`；`include/zzyl/ui/Widgets.h`（rowHeight）；`src/pages/{NursingPlanModule,NursingLevelModule,BedPreviewModule}.cpp` 占位→文档；`src/utils/CsvExport`；`.gitignore`；docs（v0.4/集成包/交付汇总/任务单） |
| cpp-dev1 | `src/pages/BedPreviewModule.cpp`（M7 P0 完整实现）、`src/ui/Widgets.cpp`（StatusTag/rowHeight）、`src/shell/ShellModule.cpp`（路由/面包屑适配） |
| cpp-dev2 | `src/pages/NursingPlanModule.cpp`、`src/pages/NursingLevelModule.cpp`（M5/M6 完整实现 + 校验红字修复）、`src/pages/CheckInFormModule.cpp`（床位级联/上传/100×100 预览/PDF 查看） |
| cpp-dev3 | `src/pages/RoomTypeModule.cpp`（本地上传+素材库）、`src/utils/CsvExport.cpp`（实现）、`src/data/SqliteData.cpp`（事务/日志收尾，协调） |
| cpp-tester | `docs/test/Sprint4_测试报告.md`、`artifacts/test_tester_s4/` |

## 4. 架构决策 / 已接受差异
| 编号 | 内容 | 口径 |
|---|---|---|
| **D-46** | M7 CRUD（楼层/房间/床位）本轮 P1 暂缓：按钮保留外观、点击 Toast「后续迭代接入」，写接口 Sprint 5 | lead 已裁定 |
| **D-47** | 原生文件对话框因本机 Shell 初始化挂起 → 改为应用内 Win32 文件浏览器（页面 API 不变） | 平台受限方案，后续可替换 |
| **D-48** | M5/M6 状态以 DB 真值 0 禁用/1 启用（Web 字典禁用=2 为历史值） | 数据口径 |
| **D-49** | M7 显示全部床位状态（空闲/已入住/请假）；M4 级联只取 status=0 | lead 已确认 |
| **D-50** | 15 间房型不在 room_type → 显示原值、price=0；2 孤儿床位保留 89 行、布局显示 87；9/10 楼无房、floor.code 重复保留 | 数据清洗/已知数据问题 |
| **D-51** | M7 床位状态 2 明确映射「请假中」（Web D-44 未实现，C++ 修正）；M7 P0 点击详情按 dev1 实现（弹层：床位号/楼层/房间/房型/状态/老人姓名） | 验收口径 |
| **D-52** | footer 校验后影子截图缺失判 P3（AutoResize 当帧 clip 时序），不修 Widgets | tester 复测确认 |

## 5. 证据索引
| 内容 | 路径 |
|---|---|
| 测试报告 / 证据 | `docs/test/Sprint4_测试报告.md`、`artifacts/test_tester_s4/` |
| 集成验证包 / 21 帧 | `docs/test/Sprint4_集成验证包.md`、`artifacts/sprint4_integration/`、`artifacts/selftest_s4_final/` |
| 架构 v0.4 / 规格 | `docs/architecture/Cpp架构设计_v0.4.md`、`web-zzyl-preject/docs/architecture/Sprint4_功能规格_v0.1.md` |
| dev 证据 | `artifacts/verify_dev2/s4*`、`artifacts/s4_m7_qa/`、`artifacts/_verify_dev3/`、`artifacts/picker_visible_test/` |
| 数据层 | 启动日志 `[Data] 数据源=SQLite | 用户=7 房型=6 护理项目=9 入住=5 护理计划=3 护理等级=3 楼层=11 房间=60 床位=87`；`build/Release/data/zzyl.db`（本地，gitignore） |

## 6. 非阻塞遗留 / Sprint 5 输入
1. GAP-01 M4 级联交互联动实机确认（5 分钟窗口，两阶段滚动法）；
2. GAP-02 M6 CRUD/重启持久化实机复核；
3. GAP-03 M5/M6 1024/1920 逐像素（selftest 增帧或窗口补图）；
4. GAP-04 CSV 导出核验（可选）；
5. M7 CRUD 写接口 + 房间编辑弹窗（Sprint 5，含楼层/房间/床位唯一性与引用校验）；
6. 原生文件对话框可替换性评估（本机 Shell 修复后）或继续应用内浏览器；
7. 15 间房型不匹配的数据修复/映射策略；9/10 楼无房与 floor.code 清洗（数据侧）。

## 7. 合规
- cpp-control 调度：可见窗口/模拟输入/抓屏均申请并释放；构建/隐藏 selftest 免申请；未 commit、未改 git 历史；
- 硬约束保持：1280×720/1024×640/F11/中文/ECS/渲染数据分离；M7 P0 与 M4/M5/M6 页面均按 pageViewport 自适应。

> **待 lead**：最终验收 → GitHub 提交（建议信息：`feat(sprint4): M5/M6/M7 页面 + M4 床位级联/上传 + SQLite v5`）；提交后回填 commit hash。
