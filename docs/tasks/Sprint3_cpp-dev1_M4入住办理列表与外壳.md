# Sprint 3 任务单 · cpp-dev1 · M4 入住办理列表 + 外壳路由

> 负责人：cpp-dev1
> 可写文件：`src/pages/CheckInListModule.cpp`（唯一页面实现文件）、`src/shell/ShellModule.cpp`（M4 菜单/路由/面包屑/标签）
> 契约（只读）：`include/zzyl/data/BusinessServices.h`（`ICheckInService`/`CheckInRecord`）、`include/zzyl/app/AppState.h`（`CheckInSelection`）、`include/zzyl/app/Routes.h`、`include/zzyl/ui/Widgets.h`
> 必读：架构 v0.3 §3.2/§5/§7；Sprint3 总览；Web 截图 `assets_v0.2/04_入退管理-入住办理.png`；M4 规格到位后对齐
> 骨架现状：`CheckInListModule.cpp` 为占位卡片；SQLite `check_ins` 已灌 5 条种子；路由 `/enterQuit/checkIn` 可达。

## 1. 交付目标
按 Web 入住办理列表复刻：
- **搜索栏**：老人姓名（Text）、身份证号（Text）+ 搜索/重置 80×32；查询 `DataServices.checkIn->List(CheckInQuery)`；
- **工具栏**：「发起入住申请」主按钮 → 入队 `Navigate /enterQuit/checkInForm`（Form 页由 dev2 实现）；
- **数据表格**（`ui::DrawDataTable`）9 列：序号(50) / 老人姓名 / 身份证号(160) / 入住床位(100) / 护理等级名称(150) / 入住期限(180，`start ~ end` 拼接) / 状态(100，StatusTag 已入住) / 创建时间(160) / 操作(80，「查看」文字按钮)；
- **「查看」**：写 `world.TryResource<CheckInSelection>()->id = row.id`，再入队 `Navigate /enterQuit/checkInInfo`；
- **分页**：共 5 条、10 条/页、贴底（`DrawPagination`）；
- **外壳**：点「入退管理 → 入住办理」自动展开父目录、高亮/面包屑/标签正确；路由已有 `/enterQuit/checkIn`（MenuCatalog 已更新，dev1 只需确认 Shell 逻辑兼容新 Route 枚举）。

## 2. 数据与交互
```cpp
auto* services = world.TryResource<DataServices>();
std::vector<data::CheckInRecord> rows = services->checkIn->List(data::CheckInQuery{});
// 每帧/查询变化时刷新；写操作（本期列表无 CRUD）无需写库
```
- 页面私有状态（`RegisterCheckInListModule` 中 `world.EmplaceResource<CheckInListPageState>()`）：rows / 搜索字段 / 分页 / checkedIds；
- 不得直接 `#include SqliteData.h/SeedServices.h`；只用 `DataServices` 接口；
- 空态/加载态由表格组件负责（空数据时显示「暂无数据」；`--empty-data` 可验证）。

## 3. 硬约束
- 只改上述 2 个文件；不改 CMake/契约头/数据层/他人页面；不 commit；
- 布局由 `ShellLayout::pageViewport` 推导；表格列宽由组件按比例适配；不写死屏幕坐标；
- 字体作用域 RAII，`PushFont` 在 `ImGui::End()` 前 Pop；
- 可见窗口/模拟输入/前台抓屏前先 `send_message` 给 **cpp-control** 申请，结束发「释放」；构建/隐藏 `--selftest` 不需要。

## 4. 自测与验收
```bat
cmake -S . -B build-dev1 -G "Visual Studio 18 2026" -A x64
cmake --build build-dev1 --config Release --parallel
build-dev1\Release\zzyl_desktop.exe --auto-login          :: 菜单进入入住办理
build-dev1\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev1
```
- 5 条种子显示、搜索姓名/身份证过滤、分页「共 5 条」、查看跳详情（页面为 dev2 占位可先验证路由+选中 id）；
- 1024×640 / 1920 全屏不重叠；截图 `09_checkin_list_1280x720.png` 与 Web 截图对比；
- 汇报 lead（抄架构师）：完成内容/文件/构建与截图/未完成/风险。

## 5. Bug 归属
- 列表页/外壳路由问题 → **cpp-dev1**（`CheckInListModule.cpp` / `ShellModule.cpp`）；
- 通用组件（表格/分页/Toast/StatusTag）问题 → **cpp-dev1**（`src/ui/Widgets.cpp`，Sprint3 已授权你维护，见 §6）；
- 数据接口/种子/DB/路由表问题 → **cpp-arch**。

## 6. v4 契约对齐（2026-11-24 晚 · M4 规格已到位，必读）
1. **可写文件增加 `src/ui/Widgets.cpp`**（通用组件维护）：已批准实现
   - StatusTag 文字：cell 文本非空且不是 "1/0/启用/禁用" 时用其作标签文字，颜色仍由 `row.enabled` 决定（现有页面不受影响）；
   - `TableDrawOptions::rowHeight`（`Widgets.h` 已加，默认 49；M4 列表传 **60**）。
2. **`CheckInRecord.status` 改为 int**：`0`=已入住（绿标签）/ `1`=已退住（红标签）；列表传 `cells[状态]="已入住"/"已退住"`、`row.enabled=(record.status==0)`。
3. 列表字段（`CheckInRecord`）：`elderName / idCard / bedNumber / nursingLevelName / startDate / endDate / status / createTime / id`；搜索：姓名 LIKE、身份证号**精确匹配**。
4. v4 迁移已完成，旧库自动升级；重建 `build-dev1` 后即可编译（如遇旧 wobj 报字段错误，清 build-dev1 重建）。
5. 路由/面包屑：`CheckInForm/CheckInDetail` 不在 MenuCatalog 子项，按任务单「父目录前缀匹配 + 菜单叶子高亮映射」实现。
