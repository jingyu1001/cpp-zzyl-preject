# Sprint 4 总览 · 文件边界与协作规则

> 迭代：Sprint 4（2026-11-24 21:30 开工）——M5 护理计划 + M6 护理等级 + M7 床位预览 + M4 数据链（floor/room/bed + 床位级联 + 文件上传）+（可选）CSV 扩展
> 架构基线：`docs/architecture/Cpp架构设计_v0.4.md`（增量；v0.1~v0.3 未变部分继续生效）
> 输入：Web 前端 `views/nursing/{plan,level,floor}`、`api/nursing/*`；Web MySQL `zzyl` 只读导出（floors 11 / rooms 60 / beds 89 / nursing_plans 3 / nursing_levels 3）；Web 规格 `Sprint4_功能规格_v0.1.md`（产出中，到位后对齐）
> 架构师：cpp-arch；总控：lead；测试：cpp-tester

## 1. 本迭代目标与分工
| 模块 | 内容 | 负责人 | 可写文件 |
|---|---|---|---|
| M7 床位预览 | 楼层 Tab → 房间卡片 → 床位状态网格（空闲/已入住/请假）+ 图例 + 点击详情 | cpp-dev1 | `src/pages/BedPreviewModule.cpp`、`src/shell/ShellModule.cpp`（如需路由适配） |
| M5 护理计划 | 列表（名称/状态/创建时间/操作）+ 表单（名称/状态/排序 + 护理项目明细增删） | cpp-dev2 | `src/pages/NursingPlanModule.cpp` |
| M6 护理等级 | 列表（7 列）+ 表单（名称/护理计划/费用/状态/说明） | cpp-dev2 | `src/pages/NursingLevelModule.cpp` |
| M4 级联 + 上传 | 表单楼层→房间→床位级联、床位费带出；3 张照片 + PDF 合同上传（WIC/文件名占位） | cpp-dev2 | `src/pages/CheckInFormModule.cpp` |
| 房型本地上传 | 房型弹窗保留素材库 + 新增「本地文件」按钮（148×148 预览） | cpp-dev3 | `src/pages/RoomTypeModule.cpp` |
| CSV 扩展（可选） | 用户/护理计划/护理等级列表导出（复用 `CsvExport`） | cpp-dev3 | `src/pages/UserList?`（由各页 owner 接入；先做护理计划/等级需与 dev2 协调）或仅提供工具契约 |
| 数据层 v5/平台/路由/文档 | floor/room/bed + nursing_plans + 种子导出 + 迁移；FileDialog；Routes/selftest/CMake | cpp-arch | 见架构 v0.4 §5 |

> **边界硬规则**：仅改自己列出的文件；不碰他人页面、不改 CMake、不改契约头（`include/zzyl/**` 由架构改）、不 commit。新建 `.cpp` 已由架构登记并建占位。

## 2. 启动包（骨架已就绪）
- Release 构建 exit 0；18 个系统；路由 `/nursing/plan`、`/nursing/level`、`/liveIn/floor` 可达（占位页）；菜单/面包屑/标签正常。
- SQLite v5：迁移/种子完成 → `护理计划=3 护理等级=3 楼层=11 房间=60`；床位表 89 行、有效布局 87（2 条 orphan room_id=74 被过滤）；stderr 0 告警；`--empty-data` 21 帧空态正常。
- selftest **21 帧**：01~17 既有页面 + **18 护理计划 / 19 护理等级 / 20 床位预览 / 21 M4 表单级联**（当前占位）；证据 `artifacts/selftest_s4/`。
- 上传平台：`include/zzyl/platform/FileDialog.h`（`PickImageFile`/`PickPdfFile`）已编译；选中复制到 `<exe>/assets/uploads/`，返回 `relativePath` 可直接给 `TextureCache`。
- 数据服务：`DataServices.nursingPlan / nursingLevel / floorLayout` 已就绪（SQLite 实现 + 空态回退）。

## 3. 公共契约速查
```cpp
auto* services = world.TryResource<DataServices>();
// M5
services->nursingPlan->List(data::NursingPlanQuery{});
services->nursingPlan->Get(id, &plan, &err);   // plan.projects 已带项目明细
services->nursingPlan->Add/Update/Remove/SetStatus(...)
// M6
services->nursingLevel->List(data::NursingLevelQuery{});
services->nursingLevel->Add/Update/Remove/SetStatus(...)
// M7 / M4 级联
services->floorLayout->Floors();            // 11
services->floorLayout->Rooms(floorId);      // 60 总计
services->floorLayout->Beds(roomId);        // 床 + elderName
services->floorLayout->BedsByFloor(floorId);
// 上传（平台层）
auto img = zzyl::platform::PickImageFile("uploads/checkin");  // ok / canceled / error
record.imagePath = img.relativePath;   // assets/uploads/...
auto pdf = zzyl::platform::PickPdfFile("uploads/contract");
record.agreementPath = pdf.relativePath; // PDF 仅文件名/查看占位
// 渲染
world.TryResource<render::TextureCache>()->Get(path); render::DrawImageContain(...)
```
> 页面统一结构：`搜索栏 → 工具栏 → 表格/卡片区（余高）→ 分页/图例（贴底）`；表单用 `BeginFormDialog/EndFormDialog` 或整页；所有操作经服务层，不直接 include `SqliteData.h`。

## 4. 构建 / 自测
```bat
cmake -S . -B build-devX -G "Visual Studio 18 2026" -A x64
cmake --build build-devX --config Release --parallel
build-devX\Release\zzyl_desktop.exe --auto-login
build-devX\Release\zzyl_desktop.exe --selftest artifacts\selftest_devX
:: 可选
build-devX\Release\zzyl_desktop.exe --selftest artifacts\selftest_devX --empty-data
build-devX\Release\zzyl_desktop.exe --selftest artifacts\selftest_devX --db artifacts\devX\zzyl.db
```
- 新增 selftest 帧：**18 护理计划 / 19 护理等级 / 20 床位预览 / 21 M4 表单级联**（dev 实现后应显示真实数据）。
- 可见窗口/模拟输入/前台抓屏先向 **cpp-control** 申请（FIFO，≤15 分钟，结束释放）；构建/隐藏 selftest 免申请。

## 5. Bug 归属映射
| 范围 | 负责人 |
|---|---|
| M7 床位预览 / 外壳路由（`BedPreviewModule.cpp`、`ShellModule.cpp`） | cpp-dev1 |
| M5 护理计划 / M6 护理等级 / M4 表单级联+上传（`NursingPlanModule.cpp`、`NursingLevelModule.cpp`、`CheckInFormModule.cpp`） | cpp-dev2 |
| 房型本地上传（`RoomTypeModule.cpp`）、CSV（`CsvExport.cpp`）、SqliteData 事务/日志收尾（协调） | cpp-dev3 |
| 数据契约/表结构/v5 迁移/种子/路由表/FileDialog/CMake/资源/文档 | cpp-arch |
| 测试报告/产物 | cpp-tester |

## 6. 验收清单（lead/tester）
- [ ] M5：3 条计划、搜索名称/状态、新增/编辑/删除/启停，项目明细增删保存，重启仍在；
- [ ] M6：3 条等级（1500/2000/3000）、护理计划下拉、CRUD/启停，重启仍在；
- [ ] M7：11 楼层切换、房间 60/床位 89 分布、状态色+图例、点击床位详情、1024/1920 自适应；
- [ ] M4 级联：楼层→房间→床位联动、床位费带出、保存/重启详情正确；
- [ ] 上传：图片→`assets/uploads/`+WIC 预览；PDF→文件名/查看占位；取消不报错；中文文件名；
- [ ] 房型弹窗：素材库 5 图保留 + 本地文件选择；保存/重启图保留；
- [ ] DB v5：迁移/种子/幂等/0 告警/`--db`/`ZZYL_DB_PATH`/`--empty-data`；
- [ ] 回归：Sprint1~3 全部用例；1024×640 / 1920 全屏 / 中文 / 无崩溃/断言；
- [ ] 可选 CSV：护理计划/等级/用户导出文件在 `exports/`。

## 7. 时间与顺序
| 顺序 | 任务 | 说明 |
|---|---|---|
| 1 | dev2 M5+M6+M4 级联/上传；dev1 M7；dev3 房型上传+CSV | 三线并行，文件不重叠 |
| 2 | 各自隐藏 selftest + 构建；可见调试申请 cpp-control | — |
| 3 | tester 正式测试 → 架构 triage → 修复复测 | 15 分钟内派单 |
| 4 | lead 集成验收 | 归档 docs/test + artifacts |

> 规格到位后架构补字段/迁移并通知；`Sprint4SeedData.inc` 自动生成，不手改；上传目录 `assets/uploads/` 已加入 gitignore（仅保留说明）。

## 8. 规格对齐（Sprint4_功能规格_v0.1 已到位，2026-11-24）
- 架构已按规格对齐并更新：v0.4 **§11 规格对齐记录**（M5 5 列/4 操作/引用禁用/1000×614 弹窗/项目子表 300 滚动；M6 8 列/500×432/5 必填；M7 精确尺寸/状态映射/D-41~45；M4 级联只取空闲床 + 床位费 join；上传 5MB/2MB 限制；数据问题处理）。
- 契约新增/变更：`NursingPlanRecord.levelRefCount`、`RoomRecord.price`、`IFloorLayoutService::AvailableBeds(roomId)`、`platform::PickImageFile/PickPdfFile(..., maxBytes)`、`platform::OpenFileWithShell`；详情见架构 v0.4 §11。
- 各任务单已追加「规格对齐差异清单」：dev1 §6、dev2 §7、dev3 §7；请按最新字段/尺寸实现，旧开工包以本文件+任务单为准。
- **M7 CRUD（楼层/房间/床位）本期列 P1**：先交付 P0 可视化；若 lead 要求本轮全做，架构补写接口后另行通知（见 v0.4 §11.3）。
- **平台文件选择器更新（2026-11-24 晚）**：原生 `GetOpenFileNameW` 在本机 Shell 初始化挂起（白屏）→ 已改为**应用内 Win32 文件浏览器**，页面调用 `PickImageFile/PickPdfFile` **不变**；支持取消/Esc、大小限制、中文路径；自动化 `--picker-selftest`（`ZZYL_PICKER_AUTOTEST=1` 隐藏直选/`ZZYL_PICKER_SHOWTEST=1` 可见 1.2s 自动选）；详见架构 v0.4 §3.2。dev2/dev3 上传复测按此进行。
