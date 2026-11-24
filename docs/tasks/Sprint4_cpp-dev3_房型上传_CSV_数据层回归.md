# Sprint 4 任务单 · cpp-dev3 · 房型本地上传 + CSV 扩展 + 数据层回归

> 负责人：cpp-dev3
> 可写文件：`src/pages/RoomTypeModule.cpp`、`src/utils/CsvExport.cpp`（+ `include/zzyl/utils/CsvExport.h` 仅实现扩展时保持契约）、`src/data/SqliteData.cpp`（仅事务/日志回归，改前与架构对齐）
> 契约（只读）：`platform/FileDialog.h`、`render/Texture.h`、`BusinessServices.h`、`SqliteData.h`
> 必读：架构 v0.4 §3.2/§5、Sprint4 总览；上一轮你的房型图片/CSV 实现作为基线。

## 1. 房型图片本地上传（`RoomTypeModule.cpp`）
- 保留现有「素材库 5 张 + 无图」选择；
- 新增「本地文件」按钮：调 `platform::PickImageFile("uploads/roomtype")`；成功把 `relativePath` 写入表单 `photoPath`，148×148 预览用 `TextureCache` + `DrawImageContain`；取消不报错，失败 Toast；
- 保存/编辑保留 `photo_path`（已实现，回归即可）；重启后图片仍在；
- 无图仍显示「无图」占位。

## 2. CSV 扩展（可选，时间允许）
- `CsvExport` 已实现（UTF-8 BOM/RFC4180/`exports/`）；本轮扩展目标（按优先级）：
  1. 护理计划列表导出（3 行：名称/状态/创建时间）——需在 `NursingPlanModule.cpp` 接入，属 dev2 文件，**不越界**：提供 `CsvExport` 契约 + 给 dev2 的接入示例；
  2. 护理等级列表导出（7 列）——同理；
  3. 用户列表导出——`SystemUserModule.cpp` 属 dev2，不越界；
  4. **你自己页面**：房型导出已做，可补「床位预览楼层/床位导出」若 M7 页面 owner dev1 需要接入，则提供契约示例。
- 结论：本轮 CSV 扩展以 **工具能力 + 接入示例** 为主，实际页面接入由各页 owner 做；若 lead 要求集中实现，再协调。
- 导出文件名 `<name>_<timestamp>.csv`；失败 Toast 红字；不阻塞验收。

## 3. 数据层 v5 回归（`SqliteData.cpp`，协调）
- 架构已完成 v5：floors/rooms/beds/nursing_plans/nursing_project_plan/nursing_levels 扩展 + check_ins 级联列 + 种子 172 条 + v4→v5 迁移；
- 你负责回归验证（不改结构）：
  - 默认库首启/二次幂等（7/6/9/5/3/3/11；rooms 60/beds 89）；
  - `--db` / `ZZYL_DB_PATH` / `--empty-data`；
  - stderr 0 告警；
  - 事务/日志（你上轮实现的 RunInTransaction/PRAGMA）在新表操作下无异常；
  - 如发现 SQL 问题报架构，不自行改表/迁移。
- 可补：批量删除事务覆盖新服务（护理计划/护理等级）——需在 `SqliteData.cpp` 加事务，改前与架构对齐。

## 4. 硬约束
- 只改上述文件；不改 CMake/契约头/他人页面；不 commit；
- 可见窗口/模拟输入/前台抓屏先向 cpp-control 申请；构建/隐藏 selftest 免申请；
- 不复制大文件进仓库；上传目录 `assets/uploads/` 为运行产物（gitignore）。

## 5. 自测与验收
```bat
cmake -S . -B build-dev3 -G "Visual Studio 18 2026" -A x64
cmake --build build-dev3 --config Release --parallel
build-dev3\Release\zzyl_desktop.exe --auto-login
build-dev3\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev3
build-dev3\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev3 --db artifacts\dev3\zzyl.db
```
- 房型：本地上传→预览→保存→重启仍在（截图 + `assets/uploads/` 文件）；
- CSV：如接入，`exports/` 文件 BOM/行数/中文正确；
- v5 回归：默认库/自定义库/环境变量/空态/二次幂等/0 告警；
- 汇报 lead（抄架构师）。

## 6. Bug 归属
- 房型上传/CSV → **cpp-dev3**；
- 数据层结构/迁移/接口 → **cpp-arch**（dev3 协调）；
- 页面接入问题 → 对应页面 owner（dev1/dev2）。

## 7. 规格对齐差异清单（Sprint4 功能规格 v0.1，必读）
1. **上传限制**：房型图片 `platform::PickImageFile("uploads/roomtype", 5*1024*1024)`（png/jpg/jpeg，≤5MB）；超限平台层在复制前返回 error，页面 Toast 红字即可（无需清理副本）。预览 148×148 用 `DrawImageContain` + 虚线框 #CDD0D6/底 #FAFAFA；保存/编辑/重启保留（Sprint3 已实现，回归）。
2. **PDF「查看」**：如需在房型/其他页面展示 PDF，用 `platform::OpenFileWithShell(path)`（ShellExecute 默认程序）；房型本期无 PDF。
3. **数据层 v5 已对齐（架构完成）**：plans 3 / project_plan 6 / levels 3 / floors 11 / rooms 60（有效）/ beds 89 行（布局 87）；`NursingPlanRecord.levelRefCount`、`RoomRecord.price`、`AvailableBeds` 已加。**现在可以动 `SqliteData.cpp` 做你计划的「新服务批量 Remove 事务包裹」**（只加 `RunInTransaction`，不改表/字段/迁移）；改前发我一句范围即可。
4. **CSV 扩展**：按「工具能力+接入示例」口径；示例发 dev2/我；不越界改 dev2 页面。若 lead 要求集中实现，等明确扩边界。
5. v5 回归项：默认库首启/二次幂等（7/6/9/5/3/3/11/60/87）、`--db`、`ZZYL_DB_PATH`、`--empty-data`、stderr 0 告警、21 帧；上传目录 `assets/uploads/` 为运行产物（gitignore）。
