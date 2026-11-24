# Sprint 4 任务单 · cpp-dev1 · M7 床位预览 + 外壳适配

> 负责人：cpp-dev1
> 可写文件：`src/pages/BedPreviewModule.cpp`、`src/shell/ShellModule.cpp`（仅新路由面包屑/标签/菜单适配，如需）
> 契约（只读）：`include/zzyl/data/BusinessServices.h`（`IFloorLayoutService`/`FloorRecord`/`RoomRecord`/`BedRecord`）、`ui::*`、`AppState`、`Routes`
> 必读：架构 v0.4 §4（M7 行）、Sprint4 总览；Web 参考 `views/nursing/floor/index.vue` + `components/List.vue` + `FloorInfo.vue`（楼层 Tab、房间卡片、床位卡、图例 空闲/已入住/请假）
> 骨架现状：`20_bed_preview_1280x720` 为占位页；SQLite v5 已灌 11 floors / 60 rooms / 89 beds；服务接口已就绪。

## 1. 交付目标
页面 `/liveIn/floor`（菜单「在住管理 → 床位预览」）：
- **楼层 Tab**：横向 Tab（约 11 个：1楼~12楼），选中态主色下划线/高亮；点击切换楼层并加载房间；
- **房间卡片**（当前楼层）：grid 自适应（1280 约 3~4 列；1024 约 3 列；1920 约 6 列），每卡显示：房间编号（如 101）、房型（typeName）、该房间的床位卡列表；
- **床位卡**：床位号（如 101-1）+ 状态：
  - `bedStatus==0` 空闲（灰/浅绿底，文字「空闲」）
  - `bedStatus==1` 已入住（绿底/绿字，显示 `elderName`；空则「已入住」）
  - `bedStatus==2` 请假（橙/黄底，显示「请假」）
- **图例**：空闲 / 已入住 / 请假中（右上或房间区上方）；
- **点击床位**：弹出只读详情（床位号/房间/房型/状态/老人姓名）或右侧信息面板；不需要编辑/删除（本期只读）；
- **数据加载**：`Floors()` 一次；切楼层 `Rooms(floorId)`；每房间 `Beds(roomId)`（可按房间懒加载）；不一次性查询全部床位；
- 空态：无楼层/无房间/无床位显示「暂无数据」；
- 1280/1024/1920 自适应；页面纵向滚动只作用于内容区。

## 2. 数据/交互
```cpp
auto* layoutSvc = world.TryResource<DataServices>()->floorLayout.get();
std::vector<data::FloorRecord> floors = layoutSvc->Floors();
std::vector<data::RoomRecord> rooms = layoutSvc->Rooms(floorId);
std::vector<data::BedRecord> beds = layoutSvc->Beds(roomId);   // 含 elderName/bedStatus
```
- 页面私有状态资源（`RegisterBedPreviewModule` 中 `world.EmplaceResource<BedPreviewState>()`）：floors/rooms/beds 缓存、当前 floorId/roomId、选中床位；
- 只经 `DataServices.floorLayout`，不 include `SqliteData.h`；不做写操作。

## 3. 硬约束
- 只改上述 2 个文件；不改 CMake/契约头/数据层/他人页面；不 commit；
- 卡片/网格按 `pageViewport` 比例布局；字体作用域 RAII；中文文案；
- 可见窗口/模拟输入/前台抓屏前向 cpp-control 申请；构建/隐藏 selftest 不需要。

## 4. 自测与验收
```bat
cmake -S . -B build-dev1 -G "Visual Studio 18 2026" -A x64
cmake --build build-dev1 --config Release --parallel
build-dev1\Release\zzyl_desktop.exe --auto-login        :: 菜单进入床位预览
build-dev1\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev1
```
- 11 楼层切换正常；房间/床位数量与数据一致（1楼 9 房；全场 60 房；床位表 89 行、有效布局 87，2 条 orphan room_id=74 已被过滤，属数据清洗预期）；
- 状态色/图例正确（5 张已入住床 + 其余空闲 + 请假色可造 1 条测试或说明）；
- 点击床位详情正确；1024/1920 不重叠；截图 20_bed_preview_1280x720 与 Web 对比；
- 汇报 lead（抄架构师）。

## 5. Bug 归属
- 床位预览页面/外壳 M7 路由 → **cpp-dev1**；
- 数据/楼层房间床位/种子/迁移 → **cpp-arch**；
- 通用组件 → cpp-dev1（`Widgets.cpp`）先报架构。

## 6. 规格对齐差异清单（Sprint4 功能规格 v0.1，必读）
1. **布局精确值**：楼层 Tab 行高 57（选中 #008D71 文字 + 底部 2px 下划线）；图例图标 26×26、文字 14px #262626；房间卡 1280 **510.5×139** / 1024 **382.5×139**（2 列，列距 20、行距 20）；床位卡 **235.3×72**（2 列，列距 12、行距 14；1024 约 174×72）；房间号 18px/500 + 房型标签（浅青底）+ 编辑/删除 20×20 + 右侧「+」24×24；床位卡底色统一 **#F5F5F5**，hover 半透明遮罩 + 2 个 22×22 圆形按钮；页面总高约 1080 需纵向滚动。
2. **数据源**：M7 显示**全部床位状态**（`floorLayout->Rooms(floorId)` + `Beds(roomId)`），不是只空床；房型用服务已 join 的 `RoomRecord.typeName`（修 D-41），床位卡显示床位号+`elderName`/「空闲」。
3. **状态色/图例**：0 空闲（灰）、1 已入住（绿/老人名）、2 请假中（橙，明确映射 D-44）；图例三态常显。
4. **响应式**：2 列为主；建议 ≥1600 宽可 3 列（修 Web D-43 的 1605–2051 区间异常）；1024 两列不重叠；Tab 超出可横向滚动或左右箭头（修 D-42）。
5. **M7 CRUD 本期 P1（lead 已裁定接受）**：楼层/房间/床位增删改排 Sprint 5；本期「编辑/删除/加床位/加房间」按钮**保留外观**，点击 Toast「后续迭代接入」；不实现写操作/弹窗。
6. 点击床位交互：形式（tooltip 或弹层）由你定，建议显示「床位号 + 状态 + 老人姓名」；tester 验收按你实现口径检查。
7. 点击床位详情只读字段：床位号/房间/房型/状态/老人姓名；不做编辑。
