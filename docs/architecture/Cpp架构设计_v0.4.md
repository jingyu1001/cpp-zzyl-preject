# 中州养老 · C++ 转译端 架构设计 v0.4（Sprint 4 · M5/M6/M7 + M4 数据链与上传）

> 增量文档：承接 v0.3（Sprint 3：M4 入住办理 + SQLite 持久化 + 房型图片 + CSV，tester 72 用例 PASS 51+PASS*21/FAIL 0 可交付）；本文件只写 **Sprint 4 增量**，未变部分沿用 v0.1~v0.3。
> 开工：2026-11-24 21:30（lead 拍板）｜作者：cpp-arch｜状态：**骨架已落地，Release 构建 + 21 帧 selftest 通过**（见 §9）
> 输入：Web 前端 `views/nursing/{plan,level,floor}`、`api/nursing/*`；Web MySQL `zzyl` 只读导出（floor 11 / room 60 / bed 89 / nursing_plan 3 / nursing_level 3 / nursing_project_plan 6）；Web 规格 `Sprint4_功能规格_v0.1.md`（web-arch 产出中，当前按代码/DB 推导，到位后对齐）。

---

## 1. Sprint 4 范围
| 编号 | 内容 | 负责人 |
|---|---|---|
| **M5** | 服务管理-护理计划：列表（名称/状态/创建时间/操作）+ 表单（名称/状态/排序 + 护理项目明细：项目/执行时间/周期/频次，可增删） | dev2 |
| **M6** | 服务管理-护理等级：列表（等级名称/执行护理计划/护理费用/状态/说明/创建时间/操作）+ 表单（名称/护理计划/费用/状态/说明） | dev2 |
| **M7** | 在住管理-床位预览：楼层 Tab（11）→ 房间卡片（60）→ 床位卡片（89，空闲/已入住/请假状态色）+ 图例/点击详情 | dev1 |
| **M4 数据链** | SQLite v5 新增 floor/room/bed 表；入住表单床位级联（楼层→房间→床位，带出床位费）；check_ins 增加 floor_id/room_id/floor_name/room_code | dev2（UI）+ arch（数据） |
| **上传** | 平台文件对话框（Win32 GetOpenFileNameW）→ 复制 `assets/uploads/` → WIC 预览；入住表单 3 张照片 + PDF 合同；房型弹窗支持本地文件选择（保留素材库） | dev2（入住）/ dev3（房型）/ arch（平台） |
| 可选 | CSV 导出扩展到用户/护理计划/护理等级 | dev3（时间允许） |

硬约束延续：1280×720/1024×640/F11 全屏/中文/ECS/渲染数据分离；调试走 cpp-control；bug 流程「报告→架构 triage→dev 修复→tester 复测」。

---

## 2. SQLite schema v5（迁移与种子）

### 2.1 新增表 / 扩展列
```sql
floors(id PK, name, code, create_time, update_time, remark)                -- Web floor 11 条
rooms(id PK, code, sort, type_name, floor_id, create_time, update_time, is_deleted)  -- 60 条
beds(id PK, bed_number, bed_status, sort, room_id, create_time, update_time)         -- 89 行（有效布局 87；2 条 room_id=74 孤儿床位被过滤）
      -- bed_status: 0 空闲 / 1 已入住 / 2 请假（Web NULL 统一清洗为 0）
nursing_plans(id PK, sort_no, plan_name, status, create_time, update_time, remark)   -- 3 条
nursing_project_plan(id PK, plan_id, project_id, execute_time, execute_cycle, execute_frequency, create_time)  -- 6 条
nursing_levels 扩展列：plan_id, description, create_time, update_time（price=fee / enabled=status）
check_ins 扩展列：floor_id, room_id, floor_name, room_code
```
- 关联：`rooms.floor_id → floors.id`；`beds.room_id → rooms.id`；`nursing_plans.id → nursing_project_plan.plan_id → nursing_projects.id`；`nursing_levels.plan_id → nursing_plans.id`；`check_ins.floor_id/room_id` 为 M4 级联快照。

### 2.2 种子与迁移
- 种子来源：**Web MySQL `zzyl` 库只读导出**（`127.0.0.1:3306 root`）；导出的 172 条 SQL 存档于 `src/data/Sprint4SeedData.inc`（自动生成，含 floors 11 / rooms 60 / beds 89 / plans 3 / plan_projects 6 / levels 3 更新）。
- `SqliteStore::CreateTablesAndSeed`：v4 完成后再执行 v5 迁移；
  - 缺列用 `PRAGMA table_info` 检测后 `ALTER TABLE`（重复列不报错刷屏）；
  - `version<5` 或 floors/nursing_plans 为空时逐条执行 `kSprint4SeedSql`（`INSERT OR REPLACE` 幂等）；
  - 完成后 `app_meta.schema_version='5'`；二次启动不重复灌种子。
- 旧 v4 库自动升级；`--empty-data` 不打开 SQLite（内存空集合，新页面空态）。
- 验证：启动日志 `[Data] 数据源=SQLite | 用户=7 房型=6 护理项目=9 入住办理=5 护理计划=3 护理等级=3 楼层=11`；stderr 0 条 SQLite 告警。

### 2.3 数据服务接口（`BusinessServices.h`）
```cpp
INursingPlanService  : List(NursingPlanQuery) / Get / Add / Update / Remove / SetStatus
                        NursingPlanRecord{id,sortNo,planName,status,createTime,updateTime,projects[]}
                        NursingPlanProjectRecord{id,planId,projectId,projectName,executeTime,executeCycle(0天/1周/2月),executeFrequency}
INursingLevelService : List(NursingLevelQuery) / Add / Update / Remove / SetStatus
                        NursingLevelRecord{id,name,price,planId,planName,status,description,createTime,updateTime}
IFloorLayoutService  : Floors() / Rooms(floorId) / Beds(roomId) / BedsByFloor(floorId)
                        FloorRecord{id,name,code}; RoomRecord{id,code,sort,typeName,floorId}
                        BedRecord{id,bedNumber,bedStatus,sort,roomId,elderName}
```
- 实现：`SqliteNursingPlanService` / `SqliteNursingLevelService` / `SqliteFloorLayoutService`（`src/data/SqliteData.cpp`）；空态/DB 不可用回退 `Seed*Service`（内存 3 条计划/3 条等级；布局为空）。
- `DataServices` 新增 `nursingPlan / nursingLevel / floorLayout` 三个 unique_ptr；模块注册处统一创建。

---

## 3. M4 床位级联 + 上传（增强）

### 3.1 床位级联
- 表单使用 `DataServices.floorLayout`：`Floors()` → 选楼层 → `Rooms(floorId)` → 选房间 → `Beds(roomId)` → 选床位；选择后写 `CheckInRecord.floorId/roomId/floorName/roomCode/bedId/bedNumber`，并按房型价格带出 `bedFee`（`rooms.type_name` 对应房型价格；dev2 可先用查表/映射，架构提供 `roomType` 列表）。
- `check_ins` 持久化这些快照；列表/详情展示床位号（已带楼层房间编码）。
- 旧 5 条种子无级联字段（NULL/0），页面显示 fallback（床位号文本）。

### 3.2 平台层文件上传（`include/zzyl/platform/FileDialog.h` + `src/platform/FileDialog.cpp`）
```cpp
struct FilePickResult { bool ok, canceled; std::string relativePath, absolutePath, fileName, error; bool isPdf; };
FilePickResult PickImageFile(const std::string& subdir = "uploads", std::size_t maxBytes = 5MB);
FilePickResult PickPdfFile(const std::string& subdir = "uploads", std::size_t maxBytes = 2MB);
bool OpenFileWithShell(const std::string& relativePath);  // PDF「查看」
```
- **实现：应用内文件浏览器（Win32 自定义窗口，2026-11-24 晚替换原生对话框）**：
  - 原因：本机 `GetOpenFileNameW` 的 #32770 已创建但**内容全白不绘制、主线程挂起**（Shell 命名空间/云盘同步扩展初始化阻塞；dev3 实测 + 全屏截图为证）。为保 Sprint 4 交付，弃用原生对话框；
  - 交互：目录路径栏 + 列表（目录 `[name]` 在前、文件在后）+ 选择/取消/上一级按钮；双击目录进入、双击文件/回车选择、Esc/取消/X 关闭；纯 Win32 子控件，不依赖 Shell/COM；
  - 默认目录：`<exe>/assets/roomtypes`（roomtype 子目录）/ `<exe>/assets/<subdir>` / `<exe>/assets`；可用环境变量 `ZZYL_PICKER_START_DIR` 覆盖（自动化）；
  - 选中后校验扩展名/大小（超限在复制前返回 error），复制到 `<exe>/assets/uploads/<时间戳>_<原名>`，返回 `relativePath`；
  - **页面调用方式不变**（`PickImageFile/PickPdfFile` 名称/结构不变）；
  - 自动化：`--picker-selftest` + 环境变量 `ZZYL_PICKER_AUTOTEST=1` 隐藏直选首文件（已验证 ok=1+复制）；`ZZYL_PICKER_SHOWTEST=1` 显示窗口 1.2s 自动选择（已 cpp-control 窗口验证：窗口显示/自动选文件/关闭正常）；
  - 后续迭代：原生对话框待本机 Shell 环境清理后替换，或保持应用内浏览器（文档标注）。
- PDF 本期仅保存路径/文件名并显示「[查看]」占位（`ShellExecute` 打开默认程序），不渲染内容；取消 `canceled=true` 不报错；失败 Toast。

### 3.3 各页面上传点
| 页面 | 字段 | 类型 | 责任 |
|---|---|---|---|
| 入住表单 | 一寸照片 / 身份证人像面 / 身份证国徽面 | png/jpg（WIC 预览） | dev2 |
| 入住表单 | 入住合同 | PDF（仅文件名/查看占位） | dev2 |
| 房型弹窗 | 房间图片 | png/jpg（148×148 预览；素材库 5 图保留 + 「本地文件」按钮） | dev3 |

---

## 4. 页面与路由（Sprint 4 新增）
| Route | path | 标题 | 负责人 | 结构要点 |
|---|---|---|---|---|
| `NursingPlan` | `/nursing/plan` | 护理计划 | dev2 | 搜索（名称/状态）+ 表格（序号/名称/状态/创建时间/操作 编辑·删除·启停）+ 分页（3 条）；表单弹窗：名称*/状态*/排序* + 项目明细行（项目下拉/执行时间/周期 0天1周2月/频次/增删） |
| `NursingLevel` | `/nursing/level` | 护理等级 | dev2 | 搜索（等级名称/状态）+ 表格（7 列：等级名称/执行护理计划/护理费用/状态/等级说明/创建时间/操作）+ 分页（3 条）；表单：名称*/护理计划*/费用*/状态/说明 |
| `BedPreview` | `/liveIn/floor` | 床位预览 | dev1 | 楼层 Tab（11）→ 房间卡片（编号+房型，grid）→ 床卡（床位号+老人名/空闲；状态色：空闲灰/已入住绿/请假橙）+ 图例；点击床位右侧/弹窗显示详情（只读）；1280 首屏约 6~8 房间、纵向滚动 |

- 菜单已存在（`MenuCatalog`：`/nursing/plan`、`/nursing/level`、`/liveIn/floor`）；路由映射已加（`Routes.cpp`），面包屑自动取 RouteTitle。
- 占位模块已建并登记 CMake：`NursingPlanModule.cpp`、`NursingLevelModule.cpp`、`BedPreviewModule.cpp`；`Register*Module` 已注册（系统数 15→18）。

---

## 5. 文件边界（Sprint 4）
| 负责人 | 可写文件 | 只读依赖 |
|---|---|---|
| cpp-dev1 | `src/pages/BedPreviewModule.cpp`、`src/shell/ShellModule.cpp`（新路由面包屑/标签/菜单，如需） | `IFloorLayoutService`、`ui::*`、Routes/AppState |
| cpp-dev2 | `src/pages/NursingPlanModule.cpp`、`src/pages/NursingLevelModule.cpp`、`src/pages/CheckInFormModule.cpp`（床位级联 + 3 图/PDF 上传） | `INursingPlanService`/`INursingLevelService`/`IFloorLayoutService`/`ICheckInService`、`platform/FileDialog.h`、`ui::*` |
| cpp-dev3 | `src/pages/RoomTypeModule.cpp`（本地上传 + 保留素材库）、`src/utils/CsvExport.cpp`（CSV 扩展，可选）、`src/data/SqliteData.cpp`（仅数据层回归/事务收尾，改前与架构对齐） | 同上 + `platform/FileDialog.h` |
| cpp-arch | CMake、docs、`include/zzyl/**`、`src/{main,core,platform,layout,render,data,app}/**`、`assets/**`、`src/data/Sprint4SeedData.inc` | — |

- 新建 `.cpp` 已登记 CMake；不新增其他 .cpp；不 commit；可见窗口/输入/抓屏先申请 cpp-control。

## 6. Bug 归属（Sprint 4）
| 范围 | 负责人 |
|---|---|
| M7 床位预览、外壳路由/标签 | cpp-dev1 |
| M5 护理计划、M6 护理等级、M4 表单级联/上传 UI | cpp-dev2 |
| 房型图片本地上传、CSV、SqliteData 收尾 | cpp-dev3 |
| 数据契约/表结构/v5 迁移/种子/路由表/平台 FileDialog/构建/资源/文档 | cpp-arch |

---

## 7. 风险与决策
| 风险 | 对策 |
|---|---|
| Web Sprint4 规格未到位 | 先按代码/DB 实现；规格到位后架构补字段/迁移 v6 并通知 |
| Web 床状态含 NULL/2 | 种子统一清洗：NULL→0，保留 0/1/2（2 请假）；M7 图例按 0/1/2 |
| 上传文件体积/重复 | `assets/uploads/` 时间戳前缀防覆盖；`.gitignore` 增加 `assets/uploads/`（保留目录占位）；单文件建议 ≤10MB（后续可加限制） |
| 楼层/房间/床位数量大（11/60/89） | M7 按楼层懒加载（Floors → Rooms(floor) → Beds(room)），只渲染当前楼；不一次性全查 |
| 表格/卡片 1024 空间 | 卡片 grid 自适应，1024 单行 2~3 房间；纵向滚动；由组件锚定 pageViewport |
| PDF 不渲染 | 明示「本期仅保存文件名/链接」；tester 按占位验收 |
| 中文路径 | Win32 统一 WideChar→UTF8；已有 WIC 中文路径实测 |

---

## 8. 测试与覆盖（新增）
- M5：列表 3 条、搜索名称/状态、新增/编辑/删除/启停、项目明细增删保存并重启仍在；
- M6：列表 3 条（1500/2000/3000）、护理计划下拉、CRUD/启停、重启仍在；
- M7：11 楼层 Tab、60 房间/89 床位分布、状态色/图例、点击床位详情、1024/1920 自适应；
- M4 级联：楼层→房间→床位联动、床位费带出、保存后重启详情字段正确；
- 上传：图片选择→复制 `assets/uploads/`→WIC 预览；PDF 选择→仅文件名/查看占位；取消不报错；中文文件名；
- DB：v5 迁移/种子/幂等/0 告警/`--db`/`ZZYL_DB_PATH`/`--empty-data`；
- 回归：Sprint1~3 全部用例。

---

## 9. 骨架验证记录（2026-11-24，Release）
| 项 | 结果 |
|---|---|
| Release 构建 | ✅ exit 0（新增 `FileDialog.cpp`、3 个页面模块、`SqliteData` v5、种子 include、18 系统） |
| SQLite v5 | ✅ 迁移成功：`[Data] 数据源=SQLite | 用户=7 房型=6 护理项目=9 入住办理=5 护理计划=3 护理等级=3 楼层=11 房间=60 床位=87`（床位表 89 行，2 条 orphan room_id=74 被过滤）；v4→v5 自动 ALTER + 172 条种子幂等；stderr **0 告警** |
| 路由/菜单 | ✅ `/nursing/plan`、`/nursing/level`、`/liveIn/floor` 可达；菜单自动展开/面包屑/标签正常（占位页） |
| selftest | ✅ **21 帧**：新增 18 护理计划 / 19 护理等级 / 20 床位预览 / 21 M4 表单级联（占位页）；`glyphsOk/commandQueueTimingOk/wicTextureOk` true |
| `--empty-data` | ✅ 内存空集合：用户/房型/护理项目/入住/计划/等级/楼层均 0，21 帧正常 |
| 上传平台 | ✅ 编译/链接通过（`FileDialog.cpp`，comdlg32 由系统库/既有链接提供；实际选择对话框由 dev 页面接入后 tester 实机验证） |
| 证据 | `artifacts/selftest_s4/`（21 帧 + metrics）、`artifacts/selftest_s4_empty/`、`artifacts/selftest_s4_stderr.log` (0B) |
| 待验证 | 各页 CRUD/级联/上传的 UI 端到端（dev 实现后 tester 覆盖） |

---

## 10. Sprint 4 内部计划
| 顺序 | 任务 | 负责人 |
|---|---|---|
| 1 | M5+M6 列表/表单 + M4 级联/上传 UI | dev2 |
| 1 | M7 床位预览 + 外壳适配 | dev1 |
| 1 | 房型本地上传 + CSV（可选）+ 数据层回归 | dev3 |
| 2 | 各自 build-devN + 隐藏 selftest（可见调试申请 cpp-control） | dev1/2/3 |
| 3 | tester 正式测试 → 架构 triage → 修复复测 | tester |
| 4 | lead 集成验收 | lead |

> 契约变更（BusinessServices/SqliteData/AppState/Routes/FileDialog）由架构统一更新并通知；`Sprint4SeedData.inc` 为自动生成种子，不手改。

---

## 11. 规格对齐记录（v0.4.1，2026-11-24 · 依据 Sprint4_功能规格_v0.1）

### 11.1 字段/契约差异与已对齐项
| 项 | Web 规格 | C++ 已落地 | 说明 |
|---|---|---|---|
| M5 列表 | 5 列（序号/名称/状态/创建时间/操作），操作 4 个（修改/删除/查看/启停），被护理等级引用时修改/删除禁用 | `NursingPlanRecord.levelRefCount`（服务计算 `COUNT(nursing_levels.plan_id)`）；页面按 >0 禁用 | 状态用 DB 真值 0 禁用/1 启用（Web 字典写禁用=2 但 DB 实际 0；以 DB 为准，UI 显示禁用） |
| M5 弹窗 | 1000×614；顶部 3 字段；项目子表 5 列，表头 50、表体 300 可滚动、行高 50；项目去重校验 | 数据字段已备（`projects[]`），布局/校验由 dev2 实现 | 种子 plans 3 / project_plan 6（已灌） |
| M6 列表 | 8 列（序号/名称/执行计划/费用/状态/说明/创建时间/操作） | 服务已带 `planName/price/status/description/createTime` | — |
| M6 弹窗 | 500×432、5 字段全必填 | 字段已备；校验由 dev2 | 状态 0 禁用/1 启用；提交前 5 项均非空 |
| M7 布局 | Tab 高 57（选中 #008D71+2px 下划线）；房间卡 510.5×139（1024 382.5）；床位卡 235.3×72（2 列）；页面总高 1080 滚动；图例 26×26 | 数据服务已备（floors/rooms/beds + elderName）；布局由 dev1 按此实现 | 3 列断点建议 ≥1600（修 D-43） |
| M7 状态 | 床位卡统一 #F5F5F5，靠图标区分已入住/空闲/请假；D-44「请假中」Web 未实现 | `bed_status` 0 空闲/1 已入住/2 入住申请中；C++ 建议 2 显示「请假中」并启用图例（明确映射） | M7 显示**全部状态**；M4 级联只取 status=0 |
| M7 房型 | `RoomVo` 缺 typeName（D-41） | **服务已修正**：`RoomRecord.typeName` 本地 join；`price`（room_type.price）一并带出 | 15 间房 typeName 不在 room_type（特护房7/普通双人间8）→ typeName 原样显示、price=0；建议展示 + 编辑时兜底（P1） |
| M7 CRUD | 楼层/房间/床位增删改 + 弹窗（628 宽） | **本期 P0 未含写接口**（服务只读）；P1 需新增 IFloorLayoutService 写方法 + 迁移/校验 | 见 §11.3 决策 |
| M4 级联 | `getRoomAndBedByBedStatus/0` 只取空闲床；选择后带出床位费 | `IFloorLayoutService::AvailableBeds(roomId)`（status=0）+ `RoomRecord.price`；`CheckInRecord` 级联字段已扩展 | 保存时写 floor/room/bed 快照（seed 已补 5 条映射） |
| 上传 | 图片：png/jpg/jpeg，5MB（房型/项目）/2MB（入住 3 图）；PDF ≤2MB | 平台 API 已加 `maxBytes`；原生对话框因本机 Shell 挂起废弃，**改为应用内浏览器**（页面调用不变），`OpenFileWithShell` 支持 PDF 查看；隐藏/可见自测均通过 | 见 §3.2 |
| 数据问题 | 15 房型不匹配、3 条 NULL 床、floor.code 重复、9/10 楼空、2 孤儿床位 | 种子保留 Web 原值；NULL→0；孤儿床位过滤（有效 87）；房型价格 join 兜底 0 | 见 §11.3 |

### 11.2 M4 级联种子映射（已补 check_ins 快照，幂等）
| check_in | floor | room | bed |
|---|---|---|---|
| 1 张三 | 1 / 1楼 | 4 / 104 | 5 / 104-1 |
| 2 李天龙 | 1 / 1楼 | 4 / 104 | 6 / 104-2 |
| 3 老李 | 1 / 1楼 | 1 / 101 | 170 / 101-2 |
| 4 老李头儿 | 1 / 1楼 | 1 / 101 | 1 / 101-1 |
| 5 魏忠语 | 4 / 4楼 | 24 / 403 | 36 / 403-1 |

### 11.3 决策/待确认
1. **M7 CRUD（楼层/房间/床位）— lead 已裁定：本轮暂缓（P1）**。本期只交付「可视化 P0」：楼层 Tab 切换、图例、房间卡/床位网格渲染、hover 效果、点击床位显示信息（tooltip/弹层均可）；编辑/删除/加床位/加房间等按钮**保留外观**，点击 Toast「后续迭代接入」；CRUD 写接口（floors/rooms/beds）+ 房间编辑弹窗排 **Sprint 5**。已在任务单 dev1 §6 标注为已接受差异。
2. **状态枚举**：M5/M6 状态以 DB 真值 0/1 为准（Web 字典的 2 视为禁用历史值）；M7 bed_status 2 明确定义为「请假中」用于图例（D-44 修正）；**M7 显示全部床位状态（空闲/已入住/请假三态图例），M4 入住级联只取 bed_status=0 空闲床**（lead 已确认以规格与本实现为准）。
3. **15 间房型不匹配**：保留 `type_name` 原值显示（不伪造房型价格）；编辑房型时下拉只列 room_type 6 条，未匹配项提示选择/新建（M7 编辑 P1 时处理）。
4. **孤儿床位 2 条**（room_id=74 不存在）：保留 89 行种子，布局服务过滤后显示 87；lead 已认可。
5. **M7 点床位交互口径**：具体形式（tooltip 或弹层）由 **dev1 定**，建议显示「床位号 + 状态 + 老人姓名」；验收时 tester 按 dev1 实现口径检查（任务单 dev1 §6 已注明）。

---

## 12. 缺陷/修复记录（Sprint 4）

### BUG-S4-01（P2）上传文件名中文双重编码乱码 — ✅ 已修复（2026-11-24 22:25）
| 项 | 内容 |
|---|---|
| 现象 | 上传后 NTFS 文件名为 `20261124_221632_鍗曚汉濂楁埧.png`（「单人套房」UTF-8 字节被按 GBK 解释）；重复运行叠加二次乱码；`relativePath` 为 GBK 字节，预览/重启图片可能加载失败 |
| 根因 | `FileDialog.cpp` 用 UTF-8 `std::string` 参与 `std::filesystem::path::operator/`（Windows 按 ACP/GBK 解释窄串）；`path.filename().string()` 又把宽字符转 ACP 而非 UTF-8 |
| 修复 | 文件命名/复制全程 **std::wstring + 宽字符 path**（`targetName = ToWide(Timestamp()) + L"_" + source.stem().wstring() + extension`）；`absolutePath`/`relativePath` 用 `ToUtf8(target.wstring())`/`ToUtf8(target.filename().wstring())`；`AssetsRootDirectory` 返回 UTF-8 并新增内部 `AssetsRootDirectoryPath()`；`ResolveAssetAbsolutePath` 走宽路径 |
| 验证 | `--picker-selftest`（AUTOTEST=1）输出 `file=单人套房.png | rel=assets/uploads/picker_selftest/20261124_222503_单人套房.png`；NTFS 实际文件名中文正确；连续两次运行均 ok=1、无二次编码；Release 重建 22:25 |
| 归属 | cpp-arch（`src/platform/FileDialog.cpp`）｜后续 | ✅ **tester 全链路复测 PASS（2026-11-24 22:37，P2 关闭）**：中文图→148 预览→保存列表 7 条→重启同库 7 条图片仍在；上传产物中文名正确 |

### BUG-S4-02（P2）M6 空提交校验红字位置错误（含 M5 同类）— ✅ 已修复（2026-11-24 22:31）
| 项 | 内容 |
|---|---|
| 现象 | M6 新增弹窗空提交时「等级名称不能为空」浮在标题下/第一行上方，字段无红框；其余必填无提示 |
| 修复（dev2） | M6：新增逐项错误状态（eName/ePlan/ePrice/eDescription），每字段就地红框+字段下方 12px 红字，5 项一次校验全显示、编辑即清除；顶部仅保留服务层错误。M5 同类一并修（名称/排序/项目重复就地红字） |
| 验证 | 影子自测 `artifacts/verify_dev2/s4_shadow/shadow_19_level_validation_1280x720.png`（4 字段同屏红框+红字）、`shadow_18_plan_validation*`；共享 Release 重建 **22:31** |
| 归属 | cpp-dev2（`NursingLevelModule.cpp` / `NursingPlanModule.cpp`）｜后续 | ✅ **tester 独立复测 PASS（2026-11-24 22:37，P2 关闭）**：M6 4 字段同屏红框+就地红字；M5 名称红字；footer 可见可点 |

> **观察（非缺陷）：校验后影子截图 footer 缺失**——dev2 真实窗口 `artifacts/verify_dev2/s4/q12_m6_validation_realwin_1280x720.png` 显示校验后「确定/取消」正常可点；根因为 `AlwaysAutoResize` 窗口在内容增高当帧 clip 仍按上一帧高度，footer 被裁 1 帧，下一帧恢复。tester 复测 0ms/300ms/900ms 三帧一致、绿色按钮质心下移可点、取消可关闭 → **判 P3（测试截图时序伪影），非缺陷、不派单**。

### 正式测试（tester 首轮 v1，2026-11-24 22:37）— ✅ 可交付
- 用例 **56**：PASS 39 + PASS* 14 + 覆盖缺口 3 / **FAIL 0** / **P0·P1 = 0**；共享构建 22:31:52；隔离库/空态/稳定性 6 连通过；stderr 0。
- 缺陷：BUG-S4-01/02（P2）均修复并经 tester 复测关闭；footer 判 P3 非缺陷。
- 覆盖缺口（非阻塞，下一轮 5~8 分钟窗口）：GAP-01 M4 级联交互联动、GAP-02 M6 保存/修改删除/启停/重启持久化、GAP-03 M5/M6 1024/1920 逐像素、GAP-04 可选 CSV 导出核验。
- 报告：`docs/test/Sprint4_测试报告.md`；证据：`artifacts/test_tester_s4/`；集成包 `docs/test/Sprint4_集成验证包.md`。

### O-S4-01 v5 DB WAL 未 checkpoint — ✅ 已闭环（tester 复测通过）
- `SqliteStore::Close()` 增加 `PRAGMA wal_checkpoint(TRUNCATE);`：干净退出后 WAL 合并回主库（实测 selftest 后主库 77,824B、无 `-wal`/`-shm` 残留），便于备份/归档；运行期仍 WAL 模式；持久化判据仍为「重开同路径仍在」。
- tester 隔离库复测：`--selftest … --db stab.db` exit=0、21 帧、7.98s；退出后仅 `stab.db=77,824B`，无 WAL 残留 ✅。

### O-S4-02 selftest 偶发不稳定 — ✅ 按环境/测试方式限制关闭（非产品缺陷）
- tester 空闲期用 `Start-Process -RedirectStandardOutput/Error`（CI 式）连续 6 次：6/6 exit=0、21 帧、stdout 4633B、stderr 0B（8.1~10.1s）；
- 直接管道 `& exe | Out-String` 在「其它 agent 可见窗口/构建并发」下有 1/3 次 11 帧提前退出（无 stderr、无崩溃事件、exit=0）；
- 结论：**非产品缺陷**，属控制台/句柄竞争 + GPU 并发环境限制。测试规范：隐藏 selftest 用 `Start-Process` 重定向 stdout/stderr，且**不要与前台可见窗口并发**；不要求产品侧加固。
