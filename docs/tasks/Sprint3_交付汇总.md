# Sprint 3 交付汇总（2026-11-24 · 待 lead 最终验收/GitHub 提交）

> 迭代：Sprint 3（19:55 开工）——M4 入退管理-入住办理 + SQLite3 本地持久化 + 房型图片接入（+ CSV 导出）
> 架构基线：`docs/architecture/Cpp架构设计_v0.3.md`；测试报告：`docs/test/Sprint3_测试报告.md`（tester 首轮 v1）
> **结论：✅ 可交付（tester 首轮通过）** —— 用例 72 / PASS 51 + PASS* 21 / **FAIL 0** / **P0·P1=0**；无确认缺陷、无阻塞项。
> 未 commit（按约定 lead 统一提交）；本文件供 lead 最终验收与 GitHub 提交使用。

## 1. 交付范围与状态
| 模块 | 内容 | 状态 | 关键证据 |
|---|---|---|---|
| **M4 列表** | `/enterQuit/checkIn`：2 搜索项 + 发起入住申请 + 9 列表格（序号/姓名/身份证/床位/护理等级/入住期限/状态/创建时间/操作查看）+ 分页（共 5 条）+ 空态 | ✅ | `artifacts/selftest_s3_integration/09/12/13_*`、dev1 证据、tester `liveA` |
| **M4 办理入住表单** | `/enterQuit/checkInForm`：4 分区 30 字段（基本信息 10 / 家属（子表）/ 入住配置 10 / 签约办理 5），双列布局、页面纵向滚动、必填/18 位身份证/日期/手机校验、护理等级字典带出护理费、床位费映射、押金默认 3000、提交 `Add` 入库+Toast+回列表 | ✅ | `10/14/15_*`、dev2 `d1/e2/e5`、tester `C01`（空提交 4 处红框+红字）、`e5_after_submit` |
| **M4 入住详情** | `/enterQuit/checkInInfo`：`CheckInSelection.id`→`Get`，4 分区只读+编辑保存 `Update`+Toast；未选中回退提示 | ✅ | `11/16/17_*`、dev2 `h4/i0/i1`、tester 实测 |
| **SQLite 持久化** | 建库/建表/灌种子（用户7/房型6/护理项目9/入住5）；CRUD 写库；默认路径 `<exe>/data/zzyl.db`；`--db`/`ZZYL_DB_PATH` 覆盖；`--empty-data` 保留；v4 迁移（24 列 + `check_in_family` + `nursing_levels`） | ✅ | dev3 `s3_logs`、架构 `logs_supplement`、tester 4 隔离库（40,960B + 7/6/9/5） |
| **房型图片** | 5 张真实图（`assets/roomtypes/`，1.7MB）+ `标准双人间` 无图占位；列表缩略图 + 弹窗 148×148 预览 + 素材库 5 选 1 + 保存/编辑保留 `photo_path` | ✅ | dev3 `s3_ui_*`、`06_room_type`、tester 1280/1024 抽检 |
| **CSV 导出（可选）** | 房型导出 UTF-8 BOM + RFC4180 转义 → `exports/room_types_<ts>.csv`（6 行，中文/photo_path 正确） | ✅（tester 静态核验） | dev3 `room_types_20261124_202421.csv`、`s3_ui_c_03_export_toast` |
| **回归** | Sprint1/2（登录/看板/M0/M1/M2/M3/外壳）+ 1280/1024/1920 自适应 + 中文 + 命令队列时序 + WIC | ✅ | 17 帧 selftest、tester 回归组 |

## 2. 测试结论（cpp-tester 首轮 v1，20:32）
- 用例 **72**：PASS **51**（实机+自跑确定性帧） / PASS* **21**（采信 dev/arch 证据 + 抽检/静态核验） / **FAIL 0** / **P0·P1=0**；
- 6 段窗口（DQ-0012/13/14 等）执行完并已释放，实例清零；共享集成构建 20:26:24；
- 静态预检 4 项全部闭环：
  - EV-S3-01：`--db`/`ZZYL_DB_PATH` tester 独立实测（4 隔离库 40,960B + `[Data] SQLite 已启用` + 7/6/9/5）；
  - EV-S3-02：干净退出后 WAL checkpoint，主库 40,960B；以「重开同路径仍在」为持久化判据；
  - EV-S3-03：持久化按「重启截图 + 日志计数 + UI/DB 条数」三件套判定；
  - EV-S3-04：详情读取 tester 实测通过（张三四分区/配置/合同全对），提交/编辑持久化采信 dev2 三件套。
- 非缺陷记录：L-01 CSV 导出窗口未点到（dev3 证据+静态核验）；L-02 详情编辑重启、L-03 表单校验逐项采信 dev2 证据（如需 100% 独立复核 Sprint4 补 4 分钟窗口）；L-04 1024 两级滚动为 UI §3.3 允许；L-05 Toast 240 宽截断为 Sprint2 既有设计；L-06 F11 焦点竞争为环境限制（以确定性帧 13/15/17 为准）。

## 3. 变更清单（按负责人，供 GitHub 提交参考）
| 负责人 | 文件/范围 | 说明 |
|---|---|---|
| cpp-arch | `CMakeLists.txt`（sqlite3 静态库 target、新源文件登记）、`include/zzyl/app/{App.h,AppState.h,ModuleRegistry.h,Routes.h,ModuleHooks.h}`、`src/app/{App.cpp,ModuleRegistry.cpp,Routes.cpp}`、`src/data/{SqliteData.h 新增,SqliteData.cpp 新增,SeedServices.*,BusinessServices.h}`、`include/zzyl/ui/Widgets.h`（`rowHeight`）、`src/utils/CsvExport.*` 骨架、`assets/roomtypes/`（5 图 1.7MB）、`.gitignore`、docs（v0.3/集成验证包/任务单/本汇总） | 数据层 v4、M4 路由/骨架/契约、图片资源、集成验证 |
| cpp-dev1 | `src/pages/CheckInListModule.cpp`（新增实现）、`src/ui/Widgets.cpp`（StatusTag 自定义文字 + rowHeight）、`src/shell/ShellModule.cpp`（M4 路由/面包屑/标签修复） | M4 列表 + 外壳 + 组件增强 |
| cpp-dev2 | `src/pages/CheckInFormModule.cpp`、`src/pages/CheckInDetailModule.cpp`（新增实现） | M4 表单/详情 |
| cpp-dev3 | `src/pages/RoomTypeModule.cpp`（图片接入/预览/素材库/导出）、`src/data/SqliteData.cpp`（事务/PRAGMA/日志收尾）、`src/utils/CsvExport.cpp`（实现） | 图片 + 数据层收尾 + CSV |
| cpp-tester | `docs/test/Sprint3_测试报告.md`、`artifacts/test_tester_s3/` | 测试 |

## 4. 架构决策与已接受差异（Sprint 3）
| 编号 | 内容 | 口径 |
|---|---|---|
| **A-01** | 家属信息存储：独立子表 `check_in_family`（非 Web 的 remark JSON） | 架构决策：免 JSON 解析、支持增删/结构化校验；语义等价 |
| **A-02** | 护理等级：Web `/nursing/level/all` 缺失 → 内置字典 3 条（76/77/78 → 1500/2000/3000） | 本地化 |
| **A-03** | 详情数据：Web 无 detail 接口 → `ICheckInService::Get` 本地 SQLite 组装 | 本地化 |
| **A-04** | 床位级联：本地无 floor/room/bed 表 → 本期文本输入床位号 | 简化，Sprint 4 接表后升级 |
| **A-05** | 照片/合同 PDF：本期不做真实上传，按钮占位「二期」；种子路径空→占位 | 范围裁剪 |
| **A-06** | 1920 证据：dev2 原 F11 受焦点影响失败 → 架构集成帧 13/15/17 真全屏补齐 | 证据补齐 |
| **A-07** | WAL 持久化判据：以「重启同路径仍在」为准，不以主库文件大小判定 | 测试口径 |
| **A-08** | Toast 240 宽文案截断、1024 列表内部横向滚动 | Sprint2/UI §3.3 既有设计，非回归 |

## 5. 证据索引
| 内容 | 路径 |
|---|---|
| 集成验证包（说明 + 帧 + 日志 + dev 证据 + bug 归属） | `docs/test/Sprint3_集成验证包.md`、`artifacts/sprint3_integration/` |
| 架构集成 selftest（17 帧 / 0 告警 / metrics） | `artifacts/selftest_s3_integration/` |
| 正式测试报告 + 证据 | `docs/test/Sprint3_测试报告.md`、`artifacts/test_tester_s3/` |
| M4 端到端（dev2） | `artifacts/verify_dev2/s3/` |
| 房型图片/持久化/CSV（dev3） | `artifacts/_verify_dev3/s3_ui/`、`s3_logs/`、`build-dev3/Release/exports/` |
| 数据层 v4/迁移/种子 | 启动日志 `[Data] 数据源=SQLite ...`、`build/Release/data/zzyl.db`（本地库，gitignore 排除） |

## 6. 非阻塞遗留 / Sprint 4 输入
1. 表单校验逐项/详情编辑重启 100% 独立复核（如需）：Sprint 4 补 4 分钟窗口（tester L-02/L-03）；
2. 床位级联升级为 floor→room→bed 联动（新增床表 + 迁移 v5）；
3. 照片/合同 PDF 本地上传与预览；
4. CSV 导出扩展至用户/护理项目等列表（复用 `CsvExport` 契约）；
5. 图片批量/素材库管理、空态注入扩展；
6. 后端 Controller/表补齐（外部依赖，plan 中原有清单）。

## 7. 合规说明
- 全程遵守 cpp-control 调试资源调度（可见窗口/模拟输入/抓屏均申请窗口、结束释放；构建/隐藏 selftest 免申请）；
- 未执行 git commit / 未改 git 历史；仓库改动由 lead 统一提交；
- 中文文案/1280×720 默认/1024×640 最小/1920 F11/ECS/渲染数据分离等硬约束保持。

> **待 lead**：最终验收确认 → GitHub 提交（建议提交信息：`feat(sprint3): M4 入住办理 + SQLite 本地持久化 + 房型图片 + CSV`），提交后可回填 commit hash 到本文件。
