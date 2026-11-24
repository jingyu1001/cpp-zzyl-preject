# 中州养老 · C++ 转译端 架构设计 v0.3（Sprint 3 · M4 入住办理 + SQLite 持久化 + 房型图片）

> 增量文档：承接 v0.2（Sprint 2 已交付：M0 组件 + M1 用户管理 + M2 房型设置 + M3 护理项目，tester v5 65/65 PASS、可交付）；本文件只写 **Sprint 3 增量**，未变部分沿用 v0.1/v0.2。
> 开工：2026-11-24 19:55（lead 拍板）｜作者：cpp-arch｜状态：**骨架已落地，Release 构建 + 扩展 selftest（11 帧）通过**（见 §9）
> 输入：Web 端 `Sprint3_M4入住办理规格_v0.1.md`（web-arch 产出中，**尚未到位**；当前按 Web 截图 `assets_v0.2/04_入退管理-入住办理.png` + `check_in` 表字段推导，规格到位后对齐）、SQLite 3.54.1 amalgamation、5 张房型图素材。

---

## 1. Sprint 3 范围与增量总览

| 编号 | 内容 | 说明 |
|---|---|---|
| **M4** | 入退管理-入住办理：列表页 + 办理入住表单（多分区）+ 入住详情页（多分区只读/可编辑） | 主功能；路由 `/enterQuit/checkIn`、表单 `/enterQuit/checkInForm`、详情 `/enterQuit/checkInInfo`；列表种子 5 条（Web 实测） |
| **DB** | SQLite3 本地持久化（3.54.1 amalgamation） | 启动建库+灌种子（用户7/房型6/护理项目9/入住5）；CRUD 写库持久化；接口保持兼容、页面零改动 |
| **IMG** | 房型图片正式接入（5 张） | `assets/roomtypes/`（1.7MB）；房型列表缩略图 + 表单 148×148 预览；「标准双人间」无图占位 |
| 可选 | 工具栏「导出」CSV（写 `exports/`） | 时间允许由 dev3 做；不阻塞验收 |

**延续硬约束**：1280×720/最小 1024×640/缩放/F11 全屏/中文；ECS；渲染与数据分离；外壳固定尺寸作为 LayoutTokens 约束、页面比例布局；调试走 cpp-control；bug 走「报告→架构 triage→dev 修复→tester 复测」。

---

## 2. SQLite 数据层设计（Sprint 3 核心增量）

### 2.1 构建与依赖
- 依赖路径：`C:\Users\ASUS\Desktop\C++\sqlite3-amalgamation-3540100\sqlite-amalgamation-3540100\`（`sqlite3.c` 9.6MB / `sqlite3.h`）；
- CMake 新增 `ZZYL_SQLITE_DIR`（基于 `ZZYL_DEPS_ROOT`），**不复制大文件进仓库**：
  ```cmake
  add_library(sqlite3 STATIC "${ZZYL_SQLITE_DIR}/sqlite3.c")
  target_include_directories(sqlite3 SYSTEM PUBLIC "${ZZYL_SQLITE_DIR}")
  target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION SQLITE_DQS=0 SQLITE_DEFAULT_MEMSTATUS=0)
  target_link_libraries(zzyl_desktop PRIVATE sqlite3)
  ```
- Windows 下单文件 `sqlite3.c` 无需额外系统库；编译通过（Release）。

### 2.2 数据库与表结构（本迭代基线）
- 路径：默认 **`<exe 目录>/data/zzyl.db`**（自动创建目录）；`--db <path>` 或环境变量 `ZZYL_DB_PATH` 覆盖；`--empty-data` 时不打开库（内存空集合）；
- 模式：WAL、`foreign_keys=ON`；
- 表（`src/data/SqliteData.cpp` 建表，`app_meta.schema_version='3'` 预留迁移）：
  ```sql
  users(user_id PK, user_name, nick_name, dept_name, phone, email, sex, enabled, create_time, remark, post_names, role_names)
  room_types(id PK, name, bed_count, price, introduction, photo_path, remark, enabled, create_time)
  nursing_projects(id PK, name, unit, order_no, price, enabled, nursing_requirement, image_path, create_time)
  check_ins(id PK, elder_name, elder_id, id_card, birthday, age, sex, phone, address,
            image_path, id_card_portrait_img, id_card_national_emblem_img,
            start_date, end_date, fee_start_date, fee_end_date,
            nursing_level_id, nursing_level_name, bed_number, bed_id,
            deposit, nursing_fee, bed_fee, other_fees, insurance_payment, government_subsidy,
            contract_name, sign_date, third_party_name, third_party_phone, agreement_path,
            status, create_time, remark)                      -- status: '0' 已入住 / '1' 已退住
  check_in_family(check_in_id, seq, name, phone, kinship, PK(check_in_id,seq))   -- 家属信息（v4）
  nursing_levels(id PK, name, price, enabled)                -- 护理等级字典（76/77/78 -> 1500/2000/3000）
  app_meta(key PK, value)                                    -- schema_version=4
  ```
- 种子策略：每张表为空时灌入 Web 实测种子（用户 7 / 房型 6 / 护理项目 9 / 入住办理 5）；二次启动不重复（验证见 §9）；`--empty-data` 查表为空 → 页面空态；
- **M4 数据注意（Web 实测）**：`check_in_config` 仅 1/5、`contract` 仅 1/5、`elder` id=325 姓名为电话（脏数据）→ C++ 种子已**清洗补全**：5 条入住记录全部带基本信息/配置/合同字段，`nursing_level_name` 统一为「1/2/3号护理等级」，家属 1~2 位/条，护理等级字典 3 条；照片/合同 PDF 本地无素材 → 路径留空显示占位（不伪造图片）。
- **家属信息存储决策（v4，架构定）**：采用**独立子表 `check_in_family`**（`CheckInRecord.families` 读写；Add/Update 内事务级替换），**不用 remark JSON**——理由：避免无 JSON 库时手写解析、支持增删/查询、字段结构化可校验；与 Web 前端语义等价（Web 仅为历史兼容存 JSON）。
- **v4 迁移（2026-11-24 已落地）**：`app_meta.schema_version` v3→v4；旧库执行 24 条 `ALTER TABLE check_ins ADD COLUMN`（已存在则忽略）+ 建 `check_in_family`/`nursing_levels`；`INSERT OR REPLACE` 清洗后的 5 条入住 + 家属 + 护理等级字典；版本置 4。新库直接按 v4 建表灌种子。**迁移入口在 `SqliteData.cpp::CreateTablesAndSeed`，由架构维护；dev3 只做事务/日志收尾。**
- **床位级联简化（本期）**：本地无 `floor/room/bed` 表 → 床位号用**文本输入**（格式如 `104-1`），不做楼层→房间→床位三级联动；Sprint 4 接入床表后再升级为级联控件（已在 dev2 任务单标注）。
- 持久化：`Sqlite*Service` 的 `Add/Update/Remove/SetStatus` 直接 `INSERT/UPDATE/DELETE`（页面零改动，继续用 `BusinessServices.h` 接口）。

### 2.3 服务实现与初始化流程
```
App.Run
 └─ ModuleRegistry::RegisterAllModules(emptyData, dbPath)
      ├─ emptyData=true  → Seed*Service(true)（内存空集合；不打开 SQLite）
      └─ 正常             → SqliteStore::Open(path, seed=true)
                            ├─ 成功 → world.EmplaceResource<shared_ptr<SqliteStore>> + Sqlite*Service
                            └─ 失败 → Seed*Service(false)（内存种子回退，日志告警）
      └─ 统计日志：[Data] 数据源=SQLite | 用户=7 房型=6 护理项目=9 入住办理=5
```
- 新增接口：`ICheckInService`（List/Get/Add/Update/Remove）+ `CheckInRecord/CheckInQuery`（`BusinessServices.h`）；`DataServices.checkIn`；
- 生命周期：`SqliteStore` 由 `shared_ptr` 资源持有；服务持裸指针；退出随 World 析构关闭；
- 线程：本迭代全部主线程同步访问（SQLite 已开 THREADSAFE=1，为 Sprint 4 异步预留）。

### 2.4 迁移与演进
- `app_meta.schema_version` 记录版本；后续变更加增量迁移 SQL（`if version<4: ALTER ...`）；
- 页面/接口兼容原则：**只加字段/方法不删**；由架构统一更新 `BusinessServices.h` 与迁移。

---

## 3. M4 页面结构与契约

### 3.1 路由与菜单
| Route | path | 标题 | 负责人 |
|---|---|---|---|
| `CheckInList` | `/enterQuit/checkIn` | 入住办理 | dev1 |
| `CheckInForm` | `/enterQuit/checkInForm` | 办理入住 | dev2 |
| `CheckInDetail` | `/enterQuit/checkInInfo` | 入住详情 | dev2 |

- 菜单「入退管理 → 入住办理」route 已为 `/enterQuit/checkIn`（MenuCatalog 已更新）；外壳进入时自动展开+高亮+面包屑（dev1 验证）；
- 跨页上下文资源：`CheckInSelection { int id; }`（列表点「查看」写入 id → 详情读）；`CommandQueue{Navigate}` 切页。

### 3.2 列表页（Web 实测基准 `assets_v0.2/04_入退管理-入住办理.png`）
- 搜索栏：老人姓名（Text）、身份证号（Text）+ 搜索/重置；
- 工具栏：发起入住申请（主按钮，跳 Form）；
- 表格 9 列：序号(50) / 老人姓名(auto) / 身份证号(160) / 入住床位(100) / 护理等级名称(150) / 入住期限(180，`start ~ end`) / 状态(100，已入住 StatusTag) / 创建时间(160) / 操作(80，「查看」文字按钮)；
- 分页：共 5 条、10 条/页（贴底）；数据来自 `DataServices.checkIn->List({})`。

### 3.3 办理入住表单（多分区；规格到位后按字段对齐）
- 分区：基本信息（老人姓名*/身份证号*/性别/年龄/电话）→ 家属信息（姓名/关系/电话/地址，可加多条）→ 入住配置（房型/床位/护理等级/入住日期*/预计退住日期/押金/费用）→ 合同（合同编号/签约日期/备注，可上传占位）；
- 底部：提交（`checkIn->Add` → Toast → 回列表）/ 取消；
- 校验：必填红字、身份证 18 位、日期范围；提交写 SQLite 持久化。

### 3.4 入住详情（只读 + 可编辑）
- 读取：`CheckInSelection.id` → `checkIn->Get(id)`；分区展示同表单；
- 操作：编辑（进入可编辑态 → `Update` → Toast）/ 返回列表；
- 当前 `CheckInRecord` 字段为列表 + 基础字段；**M4 规格文档到位后**由架构补充分区字段（家属/合同/费用）到 `CheckInRecord` 与 `check_ins` 表（迁移 v4）。

### 3.5 M4 字段契约（v4，2026-11-24 规格对齐，已落地）

`CheckInRecord`（`BusinessServices.h`）已按 Web《Sprint3_M4入住办理规格_v0.1》§2.2/§4.1/§4.2 扩展：

| 分区 | 字段（CheckInRecord） | 必填 | 存储 |
|---|---|---|---|
| 基本信息（10） | `elderName`、`idCard`、`birthday`、`age`、`sex`(1男/0女)、`phone`、`address`、`imagePath`(一寸照)、`idCardPortraitImg`(人像面)、`idCardNationalEmblemImg`(国徽面) | 姓名/身份证必填 | check_ins 列（图片为本地相对路径，空=占位） |
| 家属信息（可增删） | `families[] { name, phone, kinship }`（kinship: 0子女/1配偶/2亲属/3朋友/4社工/5其他） | 至少 1 位建议必填（dev2 校验可放宽） | **`check_in_family` 子表**（事务替换） |
| 入住配置（10） | `startDate`、`endDate`、`feeStartDate`、`feeEndDate`、`nursingLevelId`、`nursingLevelName`、`bedNumber`、`bedId`、`deposit`（默认 3000）、`nursingFee`、`bedFee`、`otherFees`、`insurancePayment`、`governmentSubsidy` | 全部必填（费用可由等级/房型带出，只读） | check_ins 列 |
| 签约办理（5） | `contractName`、`signDate`、`thirdPartyName`、`thirdPartyPhone`、`agreementPath`（PDF 本地路径，本期可占位） | 合同名称/签约日期必填 | check_ins 列 |
| 状态/审计 | `id`、`status`（**int：0 已入住 / 1 已退住**）、`createTime`、`remark` | — | check_ins 列 |

- **护理等级字典**：`ICheckInService::NursingLevels()` 返回 3 条（76/77/78 → 1500/2000/3000），dev2 表单下拉使用；Web `/nursing/level/all` 缺失，本地化。
- **家属读写**：`Get/List` 自动填充 `families`；`Add/Update` 事务内替换子表；dev2 不用关心 SQL。
- **详情数据组装**：Web 后端无 detail 接口 → C++ `ICheckInService::Get(id)` 本地组装（含家属/配置/合同）。
- **图片上传**：3 张证件照与合同 PDF 本期**不做真实上传**（按钮占位提示「二期」）；种子路径留空；后续 Sprint 可接本地文件选择（WIC 已支持 png/jpg，PDF 预览二期）。

---

## 4. 图片资源管理（Sprint 3）

- 新增素材：5 张房型图（单人套房/双人套房/豪华单人间/豪华双人间/普通单人间，合计 **1.7MB**）已复制到 `assets/roomtypes/`（中文文件名，WIC 支持）；
- 种子 `photo_path` 指向 `assets/roomtypes/xxx.png`；房型列表图片列（`CellKind::Image`）缩略图、表单 148×148 预览直接走 `TextureCache`（WIC）；
- **标准双人间无图** → 空路径显示「无图」占位（D-23 延续）；
- 构建后 `assets/` 自动复制到 exe 旁（既有 CMake POST_BUILD）；`ResolveAssetPath` 兼容源码根运行。

---

## 5. 文件边界（Sprint 3）

| 负责人 | 可写文件 | 只读依赖 |
|---|---|---|
| cpp-dev1 | `src/pages/CheckInListModule.cpp`、`src/shell/ShellModule.cpp`（M4 路由/菜单/面包屑/标签） | `ICheckInService`、`CheckInSelection`、`ui::*`、Routes/AppState |
| cpp-dev2 | `src/pages/CheckInFormModule.cpp`、`src/pages/CheckInDetailModule.cpp` | `ICheckInService`、`CheckInSelection`、`ui::*`、Routes/AppState |
| cpp-dev3 | `src/pages/RoomTypeModule.cpp`（图片接入）、`src/data/SqliteData.cpp`（数据层收尾/迁移，**与架构协调**）、`src/utils/CsvExport.cpp`（可选，+ `include/zzyl/utils/CsvExport.h`） | `SqliteData.h`、`TextureCache`、`ui::*` |
| cpp-arch | CMake、docs、include/zzyl/**、src/{main,core,platform,layout,render,data,app}/**、assets/** | — |

- 新建 `.cpp` 已由架构登记 CMake 并建占位实现；不新增其他 .cpp；
- 不改契约头（`BusinessServices.h` 如需扩展由架构加）；不 commit；调试走 cpp-control。

---

## 6. Bug Triage 流程（沿用 v0.2）
```
tester 报告 → 架构按文件归属判定 → 派 dev → dev 先申请 cpp-control 再复现/修复 →
dev 与 tester 直连复测 → 全部通过 → 报告交 lead
```
文件归属映射：`CheckInListModule.cpp`/`ShellModule.cpp`→dev1；`CheckInFormModule.cpp`/`CheckInDetailModule.cpp`→dev2；`RoomTypeModule.cpp`/`SqliteData.cpp`/`CsvExport.cpp`→dev3；数据/纹理/路由/构建/资源→cpp-arch。

---

## 7. 测试与覆盖（Sprint 3 新增）
- 列表：搜索（姓名/身份证）、分页（共5条）、查看跳详情；
- 表单：必填/身份证/日期校验、提交成功→列表新增 6 条且 SQLite 持久化（重启仍在）；
- 详情：读取选中记录、编辑保存→重启仍在；
- 图片：房型列表 5 张真实图 + 标准双人间「无图」；新增/编辑房型图片保留；
- DB：`--db` / `ZZYL_DB_PATH` 覆盖；默认路径自动建目录；二次启动种子不重复；`--empty-data` 空态；
- 回归：Sprint 1/2 全部用例（登录/看板/M0/M1/M2/M3）不回归。

---

## 8. 风险与待决策
| 风险 | 影响 | 对策 |
|---|---|---|
| M4 规格文档未到位（web-arch 产出中） | 表单/详情字段可能与 Web 不完全一致 | 先按截图+check_in 表基线实现骨架；规格到位后架构补 `CheckInRecord`/表迁移 v4 并更新任务单 |
| check_in 表缺家属/合同/费用字段（Web 库字段有限） | 表单分区需新增表字段 | 迁移 v4：新增 `family_*`/`contract_*`/`fee_*` 列或子表（架构统一做） |
| SQLite 文件被误提交 | 仓库污染 | `.gitignore` 增加 `*.db`、`data/`、`exports/` |
| dev3 与架构同改 `SqliteData.cpp` | 冲突 | 任务单明确：dev3 仅做 SQL/迁移收尾，改前先与架构对齐；接口/表结构变更由架构统一 |
| 中文文件名/路径 | 图片加载失败 | WIC 已支持中文路径并实测；失败回退占位 |
| CSV 导出为可选 | 可能来不及 | 不阻塞验收；置灰按钮保留 |

---

## 9. 骨架验证记录（2026-11-24 晚，Release）
| 项 | 结果 |
|---|---|
| Release 构建 | ✅ 通过（新增 sqlite3 静态库 + SqliteData.cpp + 3 个 M4 页面模块；CMake 配置成功） |
| SQLite | ✅ 建库 `<build>/Release/data/zzyl.db`（28KB），种子 **用户7/房型6/护理项目9/入住5**；二次启动计数不变（不重复） |
| **v4 迁移（M4 规格对齐）** | ✅ v3 旧库自动迁移：24 列补齐 + `check_in_family`（6 条）/`nursing_levels`（3 条）+ 5 条清洗种子；启动日志样例：`id=1 姓名=张三 床位=104-1 等级=2号护理等级 状态=0 家属=1 / 护理等级字典=3`；`schema_version=4` |
| **数据层三组（dev3）** | ✅ 默认库首次/二次幂等（7/6/9/5）、`--db`、`ZZYL_DB_PATH`、`--empty-data`（0/0/0/0）、11 帧 selftest；**stderr 0 条 SQLite 告警**；证据 `artifacts/_verify_dev3/s3_logs` |
| **房型图片 UI（dev3）** | ✅ 弹窗 148×148 预览 + 素材库 5 图/无图点击切换/选中态；新建保存带图；**编辑回填并保留当前图**（photo_path 不再被清空）；证据 `artifacts/_verify_dev3/s3_ui/`（17 张） |
| **UI 持久化 + CSV（dev3）** | ✅ 房型新增→重启 **7 条仍在且图片保留**；删除→重启 **6 条**（删除持久化）；导出 `exports/room_types_20261124_202421.csv`（BOM+表头+6 行，中文/photo_path 正确）；证据同上 |
| **M4 端到端 UI 证据（dev2）** | ✅ **证据已交（待 tester 实机复核）**：提交→列表 6 条→重启仍 6 条；详情编辑联系方式→重启仍为新值；1024 详情帧；1920 由架构集成帧 13/15/17 补齐；证据 `artifacts/verify_dev2/s3/` |
| **正式测试（tester v1，2026-11-24 20:32）** | ✅ **可交付（首轮通过）**：用例 72（PASS 51 + PASS* 21）/ **FAIL 0** / **P0·P1=0**；三段窗口已释放、实例清零；静态预检 EV-S3-01~04 全部闭环；非缺陷记录 L-01~L-06；报告 `docs/test/Sprint3_测试报告.md`，证据 `artifacts/test_tester_s3/` |
| DB 路径覆盖 | ✅ `--db artifacts\db_custom\my.db`、`ZZYL_DB_PATH=...\env.db` 均生效 |
| 页面数据来自 DB | ✅ 用户/房型/护理项目列表读 SQLite；房型 5 张真实图片显示、标准双人间「无图」 |
| M4 路由 | ✅ 通过 CommandQueue 切到 `/enterQuit/checkIn`、`/enterQuit/checkInForm`、`/enterQuit/checkInInfo`（截图 09/10/11，占位页） |
| `--empty-data` | ✅ 保留：内存空集合、不打开 SQLite、页面空态（Sprint 2 用例继续可跑） |
| selftest | ✅ 11 帧（新增 09 checkin list / 10 form / 11 detail），`glyphsOk/commandQueueTimingOk/wicTextureOk` 均 true |
| 证据 | `artifacts/selftest_s3/`、`artifacts/selftest_s3_empty/`、`artifacts/db_custom/`、`artifacts/db_env/` |
| 待验证 | CRUD 写库持久化的 UI 端到端（由 dev 页面接完后 tester 验证）；M4 字段随规格对齐 |

---

## 10. 迭代计划（Sprint 3 内部）
| 顺序 | 任务 | 负责人 | 说明 |
|---|---|---|---|
| 1 | M4 列表页 + 外壳路由/菜单 | dev1 | 依赖 SQLite 服务已就绪 |
| 1 | M4 表单 + 详情页 | dev2 | 依赖 `ICheckInService`/`CheckInSelection` |
| 1 | 房型图片接入 + SQLite 数据层收尾/迁移 + CSV（可选） | dev3 | 与架构协调 `SqliteData.cpp` |
| 2 | 各自自测（隐藏 selftest/构建；可见调试先申请 cpp-control） | dev1/2/3 | 每人独立 build-devN |
| 3 | tester 正式测试（M4+DB+图片+回归）→ 架构 triage → 修复复测 | tester | 按既有流程 |
| 4 | lead 集成验收 | lead | 报告与证据归档 |

---

*契约变更（BusinessServices/SqliteData/AppState/Routes）必须由架构同步更新本文档与任务单并通知 dev/tester；`SqliteData.cpp` 为数据层核心，dev3 改动前须与 cpp-arch 对齐。*
