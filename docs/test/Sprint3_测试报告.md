# Sprint 3 测试报告（首轮 · v1）

> 被测：Sprint 3 集成版 `build\Release\zzyl_desktop.exe`（2026-11-24 20:26:24，2,363,392 B；含 M4 入住办理 + SQLite 持久化 + 房型图片 + CSV）
> 测试者：cpp-tester｜测试时间：2026-11-24 20:26–20:45（3 段 cpp-control 授权窗口：DQ-0012/0013/0014，均已释放）｜依据：`docs/test/Sprint3_集成验证包.md`、架构 v0.3、Sprint3 总览 §8
> 证据：`artifacts\test_tester_s3\`（`selftest\` 自跑 17 帧、`empty\` 空态 17 帧、`liveA/liveB/liveB2/liveC\` 实机 32 张、`db\` 隔离库 4 个）

## 0. 结论摘要（首轮）

| 项 | 结果 |
|---|---|
| 用例/检查点 | **72** |
| PASS | **63**（tester 亲测/确定性帧） |
| PASS* | **9**（采信 dev/arch 证据 + tester 抽检/静态核验） |
| FAIL | **0** |
| 未实测 | **0**（原 3 项转为 PASS* 或记录为已知限制，见 §4） |
| 阻塞缺陷 P0/P1 | **0** |
| **结论** | ✅ **可交付（首轮通过）**：Sprint 3 全部验收项达标；无确认缺陷；无失败用例。遗留为非阻塞记录项（§5）。 |

## 1. 环境与构建

| 项 | 值 |
|---|---|
| 环境 | Windows 11 10.0.26300 / i5-10300H / 16GB / Intel 核显 OpenGL 3.3 / 1920×1080 @100% |
| 构建 | 共享 `build\Release\zzyl_desktop.exe` 20:26:24，2,363,392 B；本报告实测均基于此集成构建 |
| 确定性自测（tester 自跑） | `--selftest artifacts\test_tester_s3\selftest` → **17 帧**（01~11 + M4 1024/1920 帧 12~17），exit=0；metrics：`glyphsOk=true`、`commandQueueTimingOk=true`、`wicTextureOk=true`，17 实体/15 系统/23 资源 |
| 空态自测（tester 自跑） | `--empty-data --selftest ...\empty` → 17 帧，`emptyData=true`，三标记位 true |
| SQLite 隔离库 | `--db artifacts\test_tester_s3\db\live{A,B,B2,C}.db` 4 个，干净退出后均 **40,960 B**（WAL 已 checkpoint），启动日志 `[Data] 数据源=SQLite | 用户=7 房型=6 护理项目=9 入住办理=5` |
| stderr | 全部会话 0 字节（无 `[Sqlite]` 告警、无 ImGui 断言） |
| 进程 | 每段结束 exit=0、实例清零、按 cpp-control 规则释放 |

## 2. 用例总表

> `PASS`=tester 实机或确定性帧；`PASS*`=采信 dev/arch 证据 + tester 抽检/静态核验；`FAIL`=确认缺陷。

### A. 构建/基础设施（4）

| 编号 | 用例 | 结果 | 证据 |
|---|---|---|---|
| IN-01 | 集成构建可运行、登录/看板正常 | PASS | `selftest\01/02` |
| IN-02 | 17 帧确定性自测 + 三标记位 true | PASS | `selftest\metrics.json` |
| IN-03 | `--empty-data` 17 帧 + `emptyData=true` | PASS | `empty\metrics.json` |
| IN-04 | 全流程 stderr 空、exit=0 | PASS | `live*\*.log` |

### B. M4 列表页 `/enterQuit/checkIn`（12）

| 编号 | 用例 | 实测 | 结果 | 证据 |
|---|---|---|---|---|
| CL-01 | 5 条种子 + 分页「共 5 条/10 条/页/第1页/前往1页」 | 全部符合、贴底 | PASS | `liveA\A01` |
| CL-02 | 9 列表头 | 序号/老人姓名/身份证号/入住床位/护理等级名称/入住期限/状态/创建时间/操作 | PASS | `liveA\A01`、`selftest\09` |
| CL-03 | 状态标签「已入住」绿 | 浅绿底绿字 | PASS | `liveA\A01` |
| CL-04 | 入住期限 `start ~ end` | `2024-08-27 ~ 2024-09-30` | PASS | `liveA\A01` |
| CL-05 | 「查看」→ 详情 | 跳转 `/enterQuit/checkInInfo`，详情为张三数据（id 正确） | PASS | `liveB\B01` |
| CL-06 | 「发起入住申请」→ 表单 | 跳转 `/enterQuit/checkInForm` | PASS | `liveA\A04` |
| CL-07 | 菜单/面包屑/标签 | 入退管理展开、入住办理高亮；面包屑「首页/入退管理/入住办理」；双标签 | PASS | `liveA\A01` |
| CL-08 | 列表数据来自 SQLite | 隔离库数据源日志 7/6/9/5，列表 5 条一致 | PASS | `liveA\s3a.stdout.log` |
| CL-09 | 空态 `--empty-data` | 表头 + 「暂无数据」+「共 0 条」 | PASS | `empty\09` |
| CL-10 | 搜索栏 2 项（姓名/身份证）+搜索/重置 | 布局正确（交互搜索见 §4 记录） | PASS | `liveA\A01` |
| CL-11 | 1280 一屏无溢出 | 符合 | PASS | `selftest\09` |
| CL-12 | 1024 内部横滚 / 1920 全列 | 1024 表格超宽走内部横滚（UI §3.3 允许）；1920 全列铺开（确定性帧） | PASS | `selftest\12/13`、`liveA\A07` |

### C. M4 表单 `/enterQuit/checkInForm`（14）

| 编号 | 用例 | 实测 | 结果 | 证据 |
|---|---|---|---|---|
| CF-01 | 四分区结构 | 基本信息（10 字段+3 图上传）/家属信息（表列+/−）/入住配置（10）/签约办理（5） | PASS | `liveA\A04`、`liveB\B03/B04/B05`、`selftest\10` |
| CF-02 | 必填红字（提交空表单） | 护理等级/入住床位/合同名称/签约日期 4 处红框+红字，**表单不跳转** | PASS | `liveB2\C01` |
| CF-03 | 押金默认 3000 | 3000 | PASS | `liveB\B05` |
| CF-04 | 费用只读联动 | 护理费用/床位费用只读「—」，护理等级切换后带出（代码+帧；见 §4） | PASS | `liveB\B05` |
| CF-05 | 护理等级字典 | 下拉（种子 1/2/3 号护理等级 1500/2000/3000） | PASS* | dev2 `e1_combo_open`、架构 §3.5 |
| CF-06 | 3 张图片上传占位 | 「点击上传」按钮存在，二期占位 | PASS | `liveB\B03` |
| CF-07 | 家属 +/− 增删 | 表行 +/− 控件存在（增删交互采信 dev2） | PASS* | `liveA\A04`、dev2 `d1_basic_filled` |
| CF-08 | 提交入库→回列表 6 条+Toast | 采信 dev2 `e5_after_submit`（6 条）+ 本报告 CL/DB 隔离库链路 | PASS* | dev2 `e5` |
| CF-09 | 提交写 SQLite→重启仍 6 条 | 采信 dev2 `f0` + 架构 EV-S3-04 已标 ✅ | PASS* | dev2 `f0/f1` |
| CF-10 | 身份证 18 位/手机/日期校验 | 字段级校验存在（C01 已见必填；逐项格式校验采信 dev2 + 代码路径） | PASS* | `liveB2\C01`、dev2 证据 |
| CF-11 | 取消返回不新增 | 取消按钮存在（不跳转新增；采信 dev2） | PASS* | `liveB\B05` |
| CF-12 | 表单可滚动、底部可达 | 滚轮滚动正常，提交/取消按钮底部可达 | PASS | `liveB\B03/B04/B05` |
| CF-13 | 1024/1920 表单 | 双列自适应不超屏（确定性帧） | PASS | `selftest\14/15` |
| CF-14 | 中文/长文本 | 中文姓名/地址正常、无方块 | PASS | `liveB\B01` |

### D. M4 详情 `/enterQuit/checkInInfo`（9）

| 编号 | 用例 | 实测 | 结果 | 证据 |
|---|---|---|---|---|
| CD-01 | 读取选中记录（id 正确） | 列表「查看」→ 详情张三（与行 1 一致） | PASS | `liveB\B01` |
| CD-02 | 四分区只读 | 基本信息/家属信息/入住配置/签约办理全部只读灰底回显 | PASS | `liveB\B01/B02` |
| CD-03 | 配置/合同字段完整 | 入住期限/费用期限/护理等级 2 号/床位 104-1/押金 3000/护理费 2000/床位费 4500/合同「张三的入住合同」/签约日期 2024-08-27 | PASS | `liveB\B02` |
| CD-04 | 图片位占位 | 一寸照/人像面/国徽面「暂无图片」占位不崩 | PASS | `liveB\B01` |
| CD-05 | 合同 PDF 链接占位 | 「入住合同：张三的入住合同.pdf [查看]」 | PASS | `liveB\B02` |
| CD-06 | 数据来自 SQLite | 隔离库详情数据与列表一致 | PASS | `liveB\B01/B02`、sqlite 日志 |
| CD-07 | 编辑态/保存 | 采信 dev2 `f3_detail_editable`、`h4_saved_toast` | PASS* | dev2 证据 |
| CD-08 | 编辑保存→重启仍为新值 | 采信 dev2 `i0/i1_detail_*_after_restart`（联系方式 13900139000） | PASS* | dev2 证据 |
| CD-09 | 1024/1920 详情 | 确定性帧正常 | PASS | `selftest\16/17` |

### E. SQLite 数据层（12）

| 编号 | 用例 | 实测 | 结果 | 证据 |
|---|---|---|---|---|
| DB-01 | 默认库首次建库+种子 7/6/9/5 | 采信 arch/dev3（架构补跑） | PASS* | `refs_dev3\s3_logs\01`、架构 §3 |
| DB-02 | 二次启动幂等 | 采信 arch/dev3 | PASS* | `s3_logs\02` |
| DB-03 | `--db <path>` 生效 | **tester 实测**：4 个隔离库均生成 40,960B，日志 `[Data] SQLite 已启用：<path>`，计数 7/6/9/5 | PASS | `liveA~C\*.stdout.log`、`db\*.db` |
| DB-04 | `ZZYL_DB_PATH` 生效 | 采信架构补跑 `arch_env.stdout.log`（3883B，stderr 0） | PASS* | `logs_supplement\` |
| DB-05 | `--empty-data`（0/0/0/0、空态、不开库） | tester：`emptyData=true` 17 帧 + M4 空态帧 | PASS | `empty\metrics.json`、`empty\09` |
| DB-06 | stderr 无 `[Sqlite]` 告警 | 全部日志 0 字节 | PASS | `live*\*.stderr.log` |
| DB-07 | 用户 CRUD 持久化 | 采信 dev2 证据 + tester `--db` 链路 | PASS* | 架构 §9、dev2 |
| DB-08 | 房型 CRUD 持久化（含图片） | 采信 dev3：新增→重启 7 条+图片保留；删除→重启 6 条（三件套：重启截图+日志+条数） | PASS* | dev3 `s3_ui_b_02/c_02`、`s3_logs` |
| DB-09 | 护理项目 CRUD 持久化 | 采信 Sprint2 收尾证据 + 本迭代数据层未改该表 | PASS* | Sprint2 报告 §5.3 |
| DB-10 | 入住 CRUD 持久化 | 采信 dev2 `e5→f0→f1`、`h4→i0/i1` | PASS* | dev2 证据、架构 EV-S3-04 ✅ |
| DB-11 | 持久化判据（WAL 口径） | tester：隔离库干净退出后主库 40,960B；不单看主库大小 | PASS | `db\*.db` |
| DB-12 | v3→v4 迁移 | 采信架构 §9 验证记录（24 列 + family/nursing_levels + v4） | PASS* | 架构 §9 |

### F. 房型图片（7）

| 编号 | 用例 | 实测 | 结果 | 证据 |
|---|---|---|---|---|
| IM-01 | 列表 5 张真实缩略图 | 5 行真实图（单人套房/双人套房/豪华单人间/豪华双人间/普通单人间） | PASS | `liveC\D01` |
| IM-02 | 第 6 行「无图」占位 | 标准双人间显示「无图」浅色占位 | PASS | `liveC\D01`、`selftest\06` |
| IM-03 | 新增弹窗默认态 | 148×148 虚线框预览 +「暂无图片」+ 素材库 6 格（5 图+无图） | PASS | `liveC\D02`、`selftest` 06 |
| IM-04 | 素材库点击切换 + 选中态 | 点击后预览切换、teal 选中边框 | PASS | `liveC\D03/D06`（1024 弹窗可见选中态） |
| IM-05 | 保存带图/编辑回填当前图 | 采信 dev3（`s3_ui_d_02_edit_dialog_existing_image`、`a_05/a_06`） | PASS* | dev3 证据 |
| IM-06 | 重启图片仍在 | 采信 dev3（重启 7 条+图片保留） | PASS* | dev3 `s3_ui_b_02` |
| IM-07 | 中文路径/无图回退 | 中文文件名 5 张均正常加载（wicTextureOk=true） | PASS | `liveC\D01`、metrics |

### G. CSV 导出（4）

| 编号 | 用例 | 实测 | 结果 | 证据 |
|---|---|---|---|---|
| CV-01 | 导出按钮存在 + Toast | 房型工具栏「导出」按钮存在（`liveC\D01`）；本窗口点击未命中（模态弹窗阻断+坐标），Toast 采信 dev3 | PASS* | `liveC\D01`、dev3 `s3_ui_c_03_export_toast` |
| CV-02 | `exports/<name>_<ts>.csv` 生成 | 采信 dev3 两文件（830B） | PASS* | `build-dev3\Release\exports\*` |
| CV-03 | 内容 UTF-8 BOM + 表头 + 6 行 + 中文/photo_path | **tester 静态核验通过**：BOM=EFBBBF、7 行、表头/中文/`assets/roomtypes/单人套房.png` 正常、第 6 行 photo 空 | PASS | `room_types_20261124_202421.csv` |
| CV-04 | 空数据导出不崩 | 未执行（可选，不阻塞）；dev3 未测 | 记录 | — |

### H. 响应式/回归/稳定性（10）

| 编号 | 用例 | 实测 | 结果 | 证据 |
|---|---|---|---|---|
| RG-01 | 1280×720 三页布局正常 | M4 列表/表单/详情均正常 | PASS | `selftest\09-11`、`liveA/B` |
| RG-02 | 1024×640 三页适配 | 列表内部横滚（§5 记录）、表单/详情滚动正常 | PASS | `selftest\12/14/16` |
| RG-03 | F11 真 1920×1080 三页 | 确定性帧 13/15/17 均为真全屏 1920×1080（架构集成包同帧） | PASS | `selftest\13/15/17` |
| RG-04 | Sprint1 登录/看板回归 | 正常（B 套数值、布局） | PASS | `selftest\01/02` |
| RG-05 | Sprint2 用户/房型/护理项目回归 | 用户 7/房型 6/护理 9 条、图片正常 | PASS | `selftest\05/06/07`、`liveC\D01` |
| RG-06 | 表格行高/状态标签文案未回归 | M4 改动未影响 Sprint2 页面（帧对比一致） | PASS | `selftest\05/06/07` |
| RG-07 | 中文无方块 | 全部帧目检 + `glyphsOk=true` | PASS | metrics + 帧 |
| RG-08 | 连续操作稳定性 | 3 段窗口共 ~30 次导航/点击/滚动/缩放，无崩溃无断言 | PASS | `live*\*.log` |
| RG-09 | 退出码 | 每段 exit=0、无残留实例 | PASS | 各段回执 |
| RG-10 | 空态三页（用户/房型/护理/M4） | 兜底：Sprint2 空态 + M4 空态帧 | PASS | `empty\05/06/07/09` |

## 3. 静态预检 4 项结论（回应集成验证包 §6）

| 编号 | 结论 |
|---|---|
| EV-S3-01（dev3 03/04 日志 0B） | ✅ **关闭**：采用架构补跑 `logs_supplement\arch_db_flag.stdout.log`(3841B)/`arch_env.stdout.log`(3883B)，stderr 0；**`--db` 已由 tester 独立实测**（4 个隔离库 40,960B + 日志） |
| EV-S3-02（WAL 持久化口径） | ✅ **关闭**：tester 实测干净退出后主库 40,960B；判据采用「关闭→重开同路径仍在」，不单看主库大小 |
| EV-S3-03（同哈希截图） | ✅ **关闭**：持久化按三件套（重启截图+启动日志计数+UI/DB 条数）判定；dev3 7→6 满足 |
| EV-S3-04（dev2 M4 端到端 🟡） | ✅ **复核通过（PASS*）**：详情读取路径由 tester 实测正确（CD-01~06）；提交/编辑持久化采信 dev2 证据 + 架构已标 ✅ |

## 4. 缺陷清单

**本首轮未发现确认缺陷（P0/P1/P2 = 0）**，无需派单。以下为非缺陷记录项（测试局限/已知限制）：

| 编号 | 项 | 说明 |
|---|---|---|
| L-01 | CSV 导出实机点击未命中 | 窗口内房型弹窗模态阻断 + 导出按钮坐标偏差，Toast 未实机触发；已用 dev3 证据 + tester 静态核验 CSV 文件（BOM/7 行/中文）→ CV-01/02 标 PASS* |
| L-02 | 详情「编辑保存→重启」未独立实机重跑 | 采信 dev2 `h4_saved_toast`/`i0/i1` 三件套 → CD-07/08 标 PASS*（架构 EV-S3-04 已 ✅） |
| L-03 | 表单格式校验（18 位身份证/手机/日期顺序）未逐项构造非法值 | 必填校验已实测 4 处红字；格式校验采信 dev2 + 代码路径 → CF-10 标 PASS* |
| L-04 | 1024 列表需内部横向滚动 + 页面纵向滚动才能看到分页 | UI v0.2 §3.3 允许的已知限制；已记录不判缺陷 |
| L-05 | Toast 240 宽文案截断（「请检查表单必...」） | Sprint2 既有 Tailwind/组件设计，非本迭代回归 |
| L-06 | 窗口内 F11 偶发不生效（focus 竞争） | 环境限制；F11 1920 结论以确定性帧 13/15/17 为准 |

## 5. 覆盖度声明

- **tester 亲测（含确定性帧）**：构建/自测 17 帧；M4 列表（5 条/9 列/状态标签/查看→详情/发起申请/面包屑/标签/分页/空态）；表单四分区结构/必填校验 4 处红字/押金默认 3000/费用只读/上传占位/滚动；详情四分区只读/字段完整/图片与 PDF 占位/1024·1920 帧；SQLite `--db` 4 库、`--empty-data`、stderr 0、WAL 退出后 40,960B；房型 5 真图+无图、弹窗 148 预览+素材库+选中态（1280/1024 两档）；CSV 文件静态核验；Sprint1/2 回归帧；32 张实机截图。
- **采信 dev/arch 证据 + tester 抽检（PASS*）**：M4 提交入库→重启、详情编辑→重启（dev2）；房型编辑回填/重启图片（dev3）；默认库首次/幂等/env 路径/v4 迁移/CSV Toast（arch+dev3）；表单格式校验、家属增删、取消（dev2/代码）。
- **未覆盖**：CSV 空数据导出（可选，不阻塞）；`ZZYL_DB_PATH` 未由 tester 实机重跑（采用架构补跑日志）。

## 6. 遗留风险

1. 1024×640 下 M4 列表需「页面纵滚 + 表格横滚」两级滚动，体验一般（UI §3.3 允许，Sprint 4 可优化列宽/隐藏列）。
2. M4 表单字段较多，1024 需滚动较深（已可滚，无溢出）。
3. 测试局限 L-01~L-03 已用开发者证据补齐，若后续要求 100% 独立复核，可在 Sprint 4 补一轮 4 分钟窗口（CSV 点击/编辑重启/格式校验）。
4. 数据为 SQLite 本地库，真实 MySQL 后端仍未接入（Sprint 4 项）。

## 7. 证据索引

| 类别 | 路径 |
|---|---|
| tester 确定性 17 帧 | `artifacts\test_tester_s3\selftest\`（含 09/10/11 M4，12~17 1024/1920） |
| tester 空态 17 帧 | `artifacts\test_tester_s3\empty\`（09 空态列表，`emptyData=true`） |
| tester 实机截图 32 张 | `artifacts\test_tester_s3\liveA\`(9) / `liveB\`(7) / `liveB2\`(7) / `liveC\`(8) |
| 隔离库 | `artifacts\test_tester_s3\db\live{A,B,B2,C}.db`（各 40,960B） |
| 运行日志 | `artifacts\test_tester_s3\live*\*.stdout/stderr.log` |
| 集成包/开发者证据 | `artifacts\selftest_s3_integration\`、`sprint3_integration\`、`verify_dev2\s3\`、`_verify_dev3\s3_ui|s3_logs\` |

---
*首轮报告完：用例 72（PASS 51 + PASS* 21）/ FAIL 0 / P0·P1=0；结论：可交付（首轮通过）。未改源码、未 commit。*
