# Sprint 3 集成验证包（M4 入住办理 + SQLite 持久化 + 房型图片 + CSV）

> 打包：cpp-arch｜时间：2026-11-24 20:xx｜基线：架构 v0.3、M4 规格 v0.1、Sprint3 总览
> 用途：交 **cpp-tester** 正式测试的入口材料（含构建/帧/数据层/开发者证据索引与 bug 归属映射）
> 结论：**构建通过、17 帧确定性集成帧通过、数据层 0 告警、无已知 P0/P1；可进入正式测试**

## 1. 构建与环境
- 工程：`C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject`；Release：`build\Release\zzyl_desktop.exe`（最新构建含 dev1/dev2/dev3 全部提交 + 架构 v4 数据层/修复）
- 依赖：VS2026 toolset v145 + CMake 4.3.1（"Visual Studio 18 2026"）、glfw/imgui/glad、**SQLite 3.54.1 amalgamation**（静态库 target `sqlite3`，绝对路径引用）
- 构建命令：`cmake -S . -B build -G "Visual Studio 18 2026" -A x64 && cmake --build build --config Release --parallel`
- 数据层：默认库 `<exe目录>/data/zzyl.db`（自动建目录）；`--db <path>` / `ZZYL_DB_PATH` 覆盖；`--empty-data` 内存空集合；schema v4（v3 旧库自动迁移）

## 2. 集成确定性帧（架构 selftest，17 帧）
目录：`artifacts/selftest_s3_integration/`（BMP+PNG+metrics.json）；`run: zzyl_desktop.exe --selftest artifacts\selftest_s3_integration`
- `stderr`：**0 字节**（`stderr_0_warnings.log`，无 SQLite 告警/无 ImGui 断言）
- metrics：`glyphsOk/commandQueueTimingOk/wicTextureOk = true`；截图 17；实体 17 / 系统 15

| 帧 | 页面 | 说明 |
|---|---|---|
| 01~08 | 登录/看板/1024/全屏/用户/房型(真实图)/护理项目/WIC | Sprint1/2 回归基准 |
| 09/10/11 | M4 列表/表单/详情 **1280×720** | 列表 9 列 5 条；表单四分区首屏；详情张三数据（selftest 选中 id=1） |
| 12/13 | M4 列表 **1024×640 / 1920×1080** | 1024 下表格内部横向滚动（UI v0.2 §3.3 允许）；1920 全列铺开 |
| 14/15 | M4 表单 **1024×640 / 1920×1080** | 页面纵向滚动，双列自适应不超屏 |
| 16/17 | M4 详情 **1024×640 / 1920×1080** | 基本信息/家属/入住配置分区，1024 单屏可读 |

> `1920` 均为 **F11 真全屏帧缓冲 1920×1080**（补 dev2 之前 F11 被抢焦点未生效的证据缺口）。

## 3. 数据层证据（0 告警 / 幂等 / 路径覆盖 / 持久化口径）
| 项 | 结果 | 证据 |
|---|---|---|
| 默认库首次 | SQLite 自动建库+种子：用户 7/房型 6/护理项目 9/入住 5 | 本轮启动日志；`refs_dev3/s3_logs/01_default_first.log` |
| 二次启动 | 计数仍 7/6/9/5（种子不重复） | `refs_dev3/s3_logs/02_default_second.log` |
| `--db` | 自定义库落盘 40KB，计数 7/6/9/5；stdout 3841B / stderr 0 | **架构补充日志**：`logs_supplement/arch_db_flag.stdout.log` (+ `.stderr.log` 0B)、`logs_supplement/custom/zzyl.db` |
| `ZZYL_DB_PATH` | 环境变量库落盘，计数 7/6/9/5；stdout 3883B / stderr 0 | `logs_supplement/arch_env.stdout.log`、`logs_supplement/env/zzyl.db` |
| `--empty-data` | `数据源=内存(空) 用户=0 房型=0 护理项目=0 入住办理=0`，11→17 帧正常 | `refs_dev3/s3_logs/05_empty_data.log` |
| v4 迁移 | 旧库自动 ALTER 24 列 + `check_in_family`(6)/`nursing_levels`(3) + 5 条清洗种子；schema_version=4；样例 `id=1 张三 104-1 状态0 家属1` | 启动日志 |

**WAL 持久化口径（EV-S3-02）**：SQLite 开启 WAL 时，提交数据先写 `-wal`，主库文件可能仍是 4KB；**判定持久化以「关闭进程后重开同一 DB 路径，数据仍在」为准**（dev3 实测：房型新增→重启 7 条+图片保留；删除→重启 6 条）。测试若需复制/备份 DB，须连 `-wal`/`-shm` 一起复制，或先执行 `PRAGMA wal_checkpoint(TRUNCATE)`；不得仅以主库文件大小判定。

## 4. 开发者 UI 证据索引
### 4.1 M4 端到端（dev2，`refs_dev2/`）
- 提交链路：`d1_basic_filled`（基本信息）→ `e2_config_filled`（入住配置，护理等级→护理费、床位费带出）→ **`e5_after_submit`：Toast「提交成功」+ 列表共 6 条**
- 持久化：关闭重开 `f0_list_after_restart` 仍 6 条；新记录详情 `f1_detail_new_record` 字段正确
- 详情编辑：`h4_saved_toast`（联系方式改 13900139000 保存成功）→ 重开 `i0_detail_top_after_restart` 仍为新值；`i1_detail_bottom_after_restart` 配置/合同回显
- 1024：`g1_detail_1024x640`；1920 用本包 15/17（dev2 原 F11 环境失败已补）
- 注：空表单逐项校验红字 dev2 未截到（窗口时间不足），**由 tester 补测**（必填/身份证18位/日期格式顺序/手机11位）。

### 4.2 房型图片 / UI 持久化 / CSV（dev3，`refs_dev3/`）
- 图片弹窗：`s3_ui_d_02_edit_dialog_existing_image`（148×148 预览+素材库 5 图+无图+当前选中）、`s3_ui_a_03_thumb_selected`
- 持久化：`s3_ui_b_02_after_restart_list`（新增后重开 7 条+图片保留）、`s3_ui_c_02_after_restart_deleted_persisted`（删除后重开 6 条）
- CSV：`s3_ui_c_03_export_toast`、`room_types_20261124_202421.csv`（UTF-8 BOM、表头、6 行、中文/photo_path 正确）
- **同哈希截图佐证口径（EV-S3-03）**：截图哈希相同只说明「画面未变化」，**不能单独作为持久化证据**；必须搭配 ①进程重启后再次采集 ②启动日志 `[Data] 数据源=SQLite | 计数` ③DB 路径/行数（或 UI「共 N 条」）。dev3 的 7→6 条证据已同时具备重启截图+日志+DB 文件；tester 复测时请按此三件套记录。

## 5. Bug 归属映射（tester 报告直接按此派单）
| 范围 | 归属 | 文件 |
|---|---|---|
| M4 列表 / 外壳菜单·路由·标签·面包屑 | **cpp-dev1** | `src/pages/CheckInListModule.cpp`、`src/shell/ShellModule.cpp`、`src/ui/Widgets.cpp`（通用组件维护） |
| M4 办理入住表单 / 入住详情 | **cpp-dev2** | `src/pages/CheckInFormModule.cpp`、`src/pages/CheckInDetailModule.cpp` |
| 房型图片接入 / SqliteData 事务·日志 / CSV 导出 | **cpp-dev3** | `src/pages/RoomTypeModule.cpp`、`src/data/SqliteData.cpp`（协调）、`src/utils/CsvExport.*` |
| 数据契约/表结构/迁移/种子 / 路由表 / 纹理 / 构建 / 资源 / 文档 | **cpp-arch** | `include/zzyl/**`、`src/{main,core,platform,layout,render,data,app}/**`、CMake、assets、docs |
| 测试报告 / 测试产物 | cpp-tester | `docs/test/**`、`artifacts/**` |

## 6. 静态预检 4 项口径（EV-S3-01~04，逐条回复）
1. **EV-S3-01（dev3 `03_db_flag`/`04_db_env` 日志 0 字节）**：不需要 dev3 返工；这两次 stdout 重定向配置问题，**架构已补跑并落盘**：`logs_supplement/arch_db_flag.stdout.log`（3841B）、`arch_env.stdout.log`（3883B），stderr 均 0B，含 `[Data] SQLite 已启用/数据源` 行；tester 可直接采用，或在自己的隔离库实机重跑 `--db`/`ZZYL_DB_PATH` 复核（推荐，纳入正式用例）。
2. **EV-S3-02（ui.db 4KB+WAL 366KB 持久化口径）**：以「关闭进程→重开同路径仍在」为判定（见 §3 WAL 口径）；不得仅看主库大小。tester 的隔离库策略可保留，复制库须带 WAL 或先 checkpoint。
3. **EV-S3-03（同哈希截图）**：按 §4.2 三件套（重启截图+启动日志计数+UI/DB 行数）判定；单独同哈希截图不构成持久化证据。
4. **EV-S3-04（dev2 M4 端到端原标 🟡）**：dev2 证据已到位（§4.1：提交→6 条→重启仍 6 条→详情编辑→重启新值）；**已改标 ✅（证据已交，待 tester 实机复核）**；tester 按独立用例复核后标 PASS/PASS*，若发现与证据不符按 §5 派单。

## 7. 测试入口与纪律
- 材料：本文件、`docs/architecture/Cpp架构设计_v0.3.md`、`docs/tasks/Sprint3_*.md`、`artifacts/sprint3_integration/`、`artifacts/selftest_s3_integration/`、dev 证据目录（`artifacts/verify_dev2/s3/`、`artifacts/_verify_dev3/s3_ui|s3_logs/`）
- 隔离库：建议 `--db artifacts\test_tester_s3\db\<case>.db`；每例独立库，不污染默认库
- 隐藏 selftest/构建不受 cpp-control 限制；**可见窗口/模拟输入/前台抓屏先向 cpp-control 申请窗口**（FIFO，结束释放），失败标「环境原因」附截图不阻塞
- 报告：`docs/test/Sprint3_测试报告.md`；bug 先发架构 triage（15 分钟内派单），P0/P1 立即通知 lead
- 重点补测：表单空提交逐项红字、1024 列表内部横向滚动可操作性、详情编辑持久化独立复核、房型图片弹窗/CSV、--empty-data 空态、Sprint1/2 回归

## 8. 已知限制（非阻塞，tester 记录即可）
- 1024×640 列表表格需内部横向滚动（UI v0.2 §3.3 允许）；
- Toast 240 宽文案截断为 Sprint2 既有设计（非回归）；
- 照片/合同 PDF 上传为「二期」占位（种子路径空→显示占位）；
- 床位级联本期为文本输入（无 floor/room/bed 表，Sprint4 接）；
- dev2 原 1920 F11 抓图受前台焦点影响失败 → 已由本包 13/15/17 真全屏帧补齐。
