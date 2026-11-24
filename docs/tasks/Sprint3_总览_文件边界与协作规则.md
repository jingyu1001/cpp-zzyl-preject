# Sprint 3 总览 · 文件边界与协作规则

> 迭代：Sprint 3（2026-11-24 19:55 开工）——M4 入退管理-入住办理 + SQLite3 本地持久化 + 房型图片接入（+可选 CSV 导出）
> 架构基线：`docs/architecture/Cpp架构设计_v0.3.md`（增量；v0.1/v0.2 未变部分继续生效）
> 输入：Web 端《Sprint3_M4入住办理规格_v0.1.md》（web-arch 产出中，到位后对齐）；SQLite 3.54.1 amalgamation；5 张房型图
> 架构师：cpp-arch；总控：lead；测试：cpp-tester

## 1. 本迭代目标
| 模块 | 内容 | 负责人 |
|---|---|---|
| M4 列表 | `/enterQuit/checkIn`：搜索（姓名/身份证）、9 列表格（5 条种子）、分页、查看跳详情、发起入住申请 | cpp-dev1 |
| M4 表单 | `/enterQuit/checkInForm`：基本信息/家属信息/入住配置/合同多分区、校验、提交入库、返回 | cpp-dev2 |
| M4 详情 | `/enterQuit/checkInInfo`：多分区只读+可编辑、保存更新 | cpp-dev2 |
| SQLite | 建库/种子/CRUD 持久化、DB 路径覆盖、`--empty-data` 保留 | cpp-arch（骨架）+ cpp-dev3（数据层收尾/迁移，协调） |
| 房型图片 | 5 张真实图（assets/roomtypes）+ 列表缩略图 + 表单预览 + 无图占位 | cpp-dev3 |
| 可选 CSV | 工具栏导出 `exports/*.csv` | cpp-dev3（时间允许） |

## 2. 文件边界（硬规则）
| 负责人 | 唯一可写文件 | 绝对不要动 |
|---|---|---|
| cpp-dev1 | `src/pages/CheckInListModule.cpp`、`src/shell/ShellModule.cpp`、`src/ui/Widgets.cpp`（通用组件维护：StatusTag 文字/表格行高） | 他人文件、CMake、`include/zzyl/**` 契约头（`Widgets.h` 由架构改）、`src/data/**` |
| cpp-dev2 | `src/pages/CheckInFormModule.cpp`、`src/pages/CheckInDetailModule.cpp` | 同上 |
| cpp-dev3 | `src/pages/RoomTypeModule.cpp`、`src/data/SqliteData.cpp`（仅 SQL/迁移收尾，改前与架构对齐）、`src/utils/CsvExport.cpp` + `include/zzyl/utils/CsvExport.h`（可选） | 同上（`SqliteData.h`/接口由架构改） |
| cpp-arch | CMake、docs、`include/zzyl/**`、`src/{main,core,platform,layout,render,data,app}/**`、`assets/**`、`src/utils/` 基线 | 三个 dev 页面实现文件 |

- 新建 `.cpp` 已由架构登记 CMake 并建空实现；**不新增其他 .cpp**；不 commit（lead 统一）；
- 构建目录 `build-devN`（gitignore）；不占前台：构建/隐藏 `--selftest` 可直接跑，**可见窗口/模拟输入/前台抓屏先 `send_message` 给 cpp-control 申请**（格式：申请者|目的|页面/功能|预计时长≤15min|需要资源；FIFO，结束发「释放」）。

## 3. 启动包（骨架已就绪）
- Release 已构建通过；SQLite 自动建库/灌种子；M4 三路由可达（占位页）；房型 5 张真实图片已显示；`--empty-data` 保留；
- 自测：`--selftest` 11 帧（01~11，新增 09 入住列表 / 10 表单 / 11 详情），`glyphsOk/commandQueueTimingOk/wicTextureOk` 全 true；
- DB 路径：默认 `<exe目录>/data/zzyl.db`；`--db <path>` 或 `ZZYL_DB_PATH` 覆盖（已实测）；
- 数据统计日志：`[Data] 数据源=SQLite | 用户=7 房型=6 护理项目=9 入住办理=5`。

## 4. 公共契约速查
```cpp
// 数据（SQLite 实现已就绪，页面零改动）
auto* services = world.TryResource<DataServices>();
services->checkIn->List(data::CheckInQuery{});       // 列表
services->checkIn->Get(id, &record, &err);           // 详情
services->checkIn->Add(record, &err);                // 新建（写库）
services->checkIn->Update(record, &err);             // 更新（写库）
services->checkIn->Remove({id}, &err);
// 跨页上下文
world.TryResource<CheckInSelection>();               // 列表点「查看」写 id；详情读 id
// 路由
TryRouteFromPath("/enterQuit/checkIn", route);       // CheckInList / CheckInForm / CheckInDetail
// UI/图片
ui::DrawSearchBar/DrawToolbar/DrawDataTable/DrawPagination/BeginFormDialog/EndFormDialog/DrawConfirmDialog/PushToast/DrawToasts
world.TryResource<render::TextureCache>()->Get("assets/roomtypes/单人套房.png"); render::DrawImageCover/Contain
world.Resource<layout::ShellLayout>();               // pageViewport
```
> 页面统一骨架：`搜索栏 → 工具栏 → 表格（余高）→ 分页（贴底）`；表单用 `BeginFormDialog/EndFormDialog` 或整页分区（M4 表单/详情可整页）；弹窗/Toast 在页面末尾绘制。

## 5. 构建 / 自测
```bat
cmake -S . -B build-devX -G "Visual Studio 18 2026" -A x64
cmake --build build-devX --config Release --parallel
build-devX\Release\zzyl_desktop.exe --auto-login
build-devX\Release\zzyl_desktop.exe --selftest artifacts\selftest_devX
:: 可选：自定义 DB / 空态
build-devX\Release\zzyl_desktop.exe --selftest artifacts\selftest_devX --db artifacts\devX\zzyl.db
build-devX\Release\zzyl_desktop.exe --selftest artifacts\selftest_devX --empty-data
```
验收自测帧：01 登录 / 02 看板 / 03 1024×640 / 04 全屏 / 05 用户 / 06 房型（真实图） / 07 护理项目 / 08 WIC / **09 入住列表 / 10 办理入住 / 11 入住详情**。

## 6. 汇报格式（给 lead + 抄架构师）
```
【cpp-devX · Sprint3 完成汇报】
1. 完成内容：<M4 列表/表单/详情/SQLite/图片/CSV 对应项>
2. 改动文件：<绝对路径>
3. 验证：编译=通过/失败；selftest=<路径>；功能自测=<列表/表单校验/提交入库/详情编辑/图片/持久化>
4. SQLite 验证：新库路径 / 二次启动数据仍在 / `--db` 覆盖 / `--empty-data`
5. 未完成/风险：<规格未到位字段、依赖>
6. 下一步：<需 tester 重点测什么>
```

## 7. Bug Triage 流程（沿用 v0.2，新增文件归属）
```
tester 报告 → 架构按文件归属判定 → 派 dev → dev 先申请 cpp-control 再复现/修复 →
dev 与 tester 直连复测 → 全部通过 → 报告交 lead
```
| 范围 | 负责人 |
|---|---|
| `CheckInListModule.cpp`、`ShellModule.cpp`（M4 列表/外壳路由） | cpp-dev1 |
| `CheckInFormModule.cpp`、`CheckInDetailModule.cpp`（M4 表单/详情） | cpp-dev2 |
| `RoomTypeModule.cpp`（图片）、`SqliteData.cpp`（数据层收尾）、`CsvExport.cpp`（可选） | cpp-dev3 |
| 数据接口/表结构/迁移/纹理/路由/构建/资源/文档 | cpp-arch |

## 8. 验收清单（lead/tester）
- [ ] M4 列表：搜索姓名/身份证、5 条种子、9 列、分页「共 5 条」、查看跳详情；
- [ ] M4 表单：四分区、必填/身份证/日期校验、提交成功入库并回列表；
- [ ] M4 详情：选中记录正确、分区展示、编辑保存；
- [ ] SQLite：默认库自动创建+种子；**新增/编辑/删除后重启仍在**（CRUD 持久化）；`--db`/`ZZYL_DB_PATH` 覆盖；`--empty-data` 空态；二次启动不重复种子；
- [ ] 房型图片：列表 5 图 + 标准双人间「无图」；表单 148×148 预览；新增/编辑保留图片；
- [ ] 回归：Sprint1/2 页面、1024×640/1920 自适应、中文、无崩溃/ImGui 断言；
- [ ] 可选 CSV：导出文件出现在 `exports/`（不做不阻塞）。

## 9. 时间与顺序
| 顺序 | 任务 | 说明 |
|---|---|---|
| 1 | dev1 M4 列表 + 外壳路由；dev2 表单+详情；dev3 图片+数据层收尾/CSV | 三线并行、文件不重叠 |
| 2 | 各自隐藏 selftest + 构建自测；可见调试先申请 cpp-control | — |
| 3 | tester 正式测试 → 架构 triage → dev 修复复测 | 按 §7 |
| 4 | lead 集成验收 | 归档 docs/test + artifacts |

> 新建/修改契约必须先经架构；`SqliteData.cpp` 为数据层核心，dev3 改动前须与 cpp-arch 对齐；M4 规格文档到位后架构会补字段/迁移 v4 并通知。
