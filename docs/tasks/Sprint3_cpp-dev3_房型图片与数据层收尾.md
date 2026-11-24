# Sprint 3 任务单 · cpp-dev3 · 房型图片接入 + SQLite 数据层收尾 + CSV 导出（可选）

> 负责人：cpp-dev3
> 可写文件：`src/pages/RoomTypeModule.cpp`（图片接入）、`src/data/SqliteData.cpp`（数据层收尾/迁移，**改前与 cpp-arch 对齐**）、`src/utils/CsvExport.cpp` + `include/zzyl/utils/CsvExport.h`（可选，架构已预置文件与 CMake 登记）
> 契约（只读）：`include/zzyl/data/SqliteData.h`、`include/zzyl/data/BusinessServices.h`、`include/zzyl/render/Texture.h`、`include/zzyl/ui/Widgets.h`
> 必读：架构 v0.3 §2（SQLite）/§4（图片）/§5/§7；Sprint3 总览
> 骨架现状：SQLite 已可建库/种子/CRUD（架构骨架）；5 张房型图已复制 `assets/roomtypes/` 且数据库种子 `photo_path` 已指向；房型列表**已能显示真实图**（见 `artifacts/selftest_s3/06_room_type_1280x720.png`）。

## 1. 交付目标（按优先级）
### P0 房型图片接入（`src/pages/RoomTypeModule.cpp`）
- **列表**：图片列已通过 `TextureCache` 显示种子图（无需改）；确认新增/编辑后 `photo_path` 不被清空；
- **表单弹窗**：148×148 预览区显示当前图片（`DrawImageContain`）+「从素材库选择」：列出 `assets/roomtypes/` 5 张（+ 无图选项），点击切换预览并写回 `record.photoPath`；
- **修复**：现有代码保存时 `record.photoPath.clear()`（RoomTypeModule.cpp:285 附近）会丢图 → 改为保留/按选择设置；
- 无图记录（标准双人间）显示「无图」占位；中文文件名由 WIC 支持（已实测）。

### P1 SQLite 数据层收尾（`src/data/SqliteData.cpp`，与架构协调）
- 与架构确认后补充/优化：
  - 种子幂等与迁移：`app_meta.schema_version` 迁移入口（v3→v4 时新增字段；当前仅占位）；
  - 事务包裹批量写（多选删除/批量状态）避免半成功；
  - `PRAGMA`/错误日志完善（打开失败回退已有）；
  - **不改接口签名/表名/列名**；表结构变更必须由架构先加迁移与头文件变更；
- 验证：默认库/`--db`/`ZZYL_DB_PATH`、二次启动种子不重复、`--empty-data` 空集合。

### P2 CSV 导出（可选，时间允许）
- `include/zzyl/utils/CsvExport.h` + `src/utils/CsvExport.cpp`：UTF-8 BOM、逗号转义、写 `exports/<name>_<timestamp>.csv`（自动建目录）；
- 接到你负责的房型页工具栏「导出」（导出当前查询结果）；其他页由各自 dev 后续按契约接入；
- 不做不阻塞验收（导出按钮可置灰+Toast「二期支持」）。

## 2. 数据/接口
```cpp
// 房型
auto* services = world.TryResource<DataServices>();
services->roomType->List(data::RoomTypeQuery{});
// 图片
auto* textures = world.TryResource<render::TextureCache>();
const render::Texture& tex = textures->Get("assets/roomtypes/单人套房.png");
render::DrawImageContain(dl, tex, previewRect);
// SQLite（架构已实现）
world.TryResource<std::shared_ptr<data::SqliteStore>>();
```
- 页面只经 `DataServices` 接口；SqliteData.cpp 内可直接用 `SqliteStore::Execute/Query`；
- 不改 `BusinessServices.h`/`SqliteData.h`（需变更找架构）。

## 3. 硬约束
- 只改上述文件；不 commit；不新增其他 .cpp；
- 布局比例化；字体作用域 RAII；中文文案；可见调试先向 cpp-control 申请（构建/隐藏 selftest 不需要）；
- `SqliteData.cpp` 改动前必须与 cpp-arch 对齐范围（避免与架构迁移冲突），改后跑通默认库+`--db`+`--empty-data` 三组。

## 4. 自测与验收
```bat
cmake -S . -B build-dev3 -G "Visual Studio 18 2026" -A x64
cmake --build build-dev3 --config Release --parallel
build-dev3\Release\zzyl_desktop.exe --auto-login           :: 房型设置：5 图+无图占位；编辑保存后图不丢
build-dev3\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev3
build-dev3\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev3 --empty-data
```
- 房型列表 5 张真实图 + 标准双人间「无图」；弹窗 148×148 预览 + 素材库切换；编辑保存后图片保留；
- SQLite：默认库二次启动计数不变；`--db`/`ZZYL_DB_PATH` 生效；`--empty-data` 正常；
- CSV（若做）：`exports/` 下文件生成、UTF-8 中文可读、行数正确；
- 截图/证据给 tester；汇报 lead（抄架构师）。

## 5. Bug 归属
- 房型图片/表单问题 → **cpp-dev3**；
- SQLite 数据层问题 → **cpp-dev3（协调架构）**；表结构/接口/迁移冲突 → **cpp-arch**；
- CSV 问题 → cpp-dev3；
- 通用组件问题 → cpp-dev1（先报架构）。

## 6. v4 完成通知（2026-11-24 晚，架构已落地，请重建）
- **v4 迁移已完成**：`CheckInRecord` 30 字段扩展、`check_ins` 24 列补齐、`check_in_family` 子表、`nursing_levels` 字典、5 条入住种子清洗补全；`schema_version=4`；旧库自动迁移；Release 全量构建已通过（你之前看到的 `NursingLevels` 未声明是 v4 中间态，头文件已同步）。
- **请你立即**：清/重建 `build-dev3` → 恢复 P1（按 §1 P1 范围：批量 Remove 事务包裹、PRAGMA/错误日志完善；**迁移入口不改**）→ 三组验证（默认库/`--db`/`ZZYL_DB_PATH` + `--empty-data` + 二次启动种子不重复）→ 结果发架构+tester。
- 你的 P0/P2（RoomTypeModule 图片、CsvExport）已在工作区，全量构建已验证可编译；可见验收向 cpp-control 申请窗口。
- 注意：`CheckInRecord` 新增字段会影响 `SqliteData.cpp` 的 check-in 实现（架构已改完并测试样例：id=1 张三 104-1 状态0 家属1）；你只改事务/日志，不要动 v4 字段/迁移。
