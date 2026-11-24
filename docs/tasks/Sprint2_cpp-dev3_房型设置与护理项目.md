# Sprint 2 任务单 · cpp-dev3 · M2 房型设置 + M3 护理项目

> 负责人：cpp-dev3
> 可写文件：`src/pages/RoomTypeModule.cpp`、`src/pages/NursingProjectModule.cpp`（可各新增 `include/zzyl/pages/RoomTypeTypes.h`、`include/zzyl/pages/NursingProjectTypes.h`）
> 契约（只读）：`include/zzyl/ui/Widgets.h`、`include/zzyl/app/Routes.h`、`include/zzyl/app/AppState.h`、`include/zzyl/data/BusinessServices.h`、`include/zzyl/render/Texture.h`
> 必读：`UI复刻规格_Cpp_v0.2.md` §2/§3/§4.2/§4.3/§5/§6；`Web功能清单_v0.2.md` §3.2/§3.3；截图 `assets_v0.2/05_在住管理-房型设置.png`、`07_服务管理-护理项目.png`、`53/54`；素材 `assets/icons/`（12 张已复制）
> 骨架现状：两个页面均为占位；`DataServices.roomType`（6 条）/`services.nursingProject`（9 条）+ `TextureCache` 已就绪。

## 1. M2 房型设置（`/liveIn/houseSet`）

### 页面结构
- 无搜索栏（D-20 保持原样）；顶部工具栏只有「新增」（80×32 浅青底主色字）；
- 数据表格（`ui::DrawDataTable`）：序号(50) / 房间图片(100，60×60 cover；无素材时显示占位块) / 房型类型(auto) / 床位费用(auto，显示 `¥` 或金额) / 房型介绍(180，省略+tooltip 可先省略) / 创建时间(160) / 状态(Switch) / 操作(Actions：修改|删除|禁用·启用)；
- 分页：共 6 条、10 条/页；
- 新增/修改弹窗（`ui::BeginFormDialog`，500 宽、单列、label 80 右对齐）：
  房型名称* / 床位数量*（整数）/ 床位费用*（数字）/ 介绍（多行，可用单行代替并标注）/ 照片（148×148 选择框；无上传能力时显示素材库/占位，D-23）/ 备注 / 确定+取消；
- 交互：新增/修改 → `Add/Update` → Toast「新增成功/修改成功」→ 刷新；删除 → 确认框「是否确认删除房型名称为“X”的数据项？」→ `Remove`；启用/禁用 → 确认框（可选）→ `SetStatus` → Toast + 刷新。

### 数据/状态
- 页面 State（在 `RegisterRoomTypeModule` 注册）：rows/checkedIds/pagination/dialog/editing/confirm 等，参考 dev2 任务单结构；
- 只经 `DataServices.roomType` 接口；图片列使用 `world.TryResource<render::TextureCache>()->Get(photoPath)`，失败显示 `ui::Card` 占位块 + 居中“暂无图片”（不能崩）。
- **交互回传（v0.2 契约）**：操作列用 `result.clickedAction + result.actionRowId`（行内标签写在 `cells[操作列]`，如 `"修改|删除|启用"` / `"修改|删除|禁用"`，随状态变化）；状态开关用 `result.switchToggles{rowId,enabled}`；表单弹窗用 `BeginFormDialog`+`EndFormDialog()`（组件画标题栏/底部按钮；Ok 不自动关，调用方校验成功后 `state.open=false`）。

## 2. M3 护理项目（`/nursing/project`）

### 页面结构
- 搜索栏：名称（Text）/ 状态（Select：全部/启用/禁用）+ 搜索/重置 80×32；
- 工具栏：新增（+ 图标）；
- 数据表格：编号(120) / 名称(150) / 排序号(100) / 单位(100，次/件/餐/小时) / 价格(150，元) / 图片(150，60×60 cover，使用 `assets/icons/*.png`) / 护理要求(auto，省略) / 状态(120，StatusTag) / 创建时间(180) / 操作(200：修改|删除|启用·禁用)；
- 分页：共 9 条；
- 新增/修改弹窗（500 宽、单列、label 80）：名称* / 排序号（数字）/ 单位 / 价格*（数字）/ 状态（单选 启用/禁用）/ 图片（148×148 预览 + “从素材库选择” 列表，数据来自 `services->nursingProject->ImageLibrary()` 12 张；点击切换预览）/ 护理要求（多行可单行）/ 确定+取消；
- 交互：同 M2；删除确认框「确定删除该护理项目吗？」。

### 数据/状态
- `DataServices.nursingProject`（`SeedNursingProjectService`，9 条，图片按 UI 规格 §6.2 映射）；
- 图片纹理全部来自 `TextureCache::Get()`（相对路径 `assets/icons/xxx@2x.png`）；表格图片列用 `DrawImageCover`，弹窗预览用 `DrawImageContain`；
- **交互回传（v0.2 契约）**：同 M2：操作列 `clickedAction+actionRowId`（行内标签 `cells[操作列]` 动态“启用/禁用”）、Switch `switchToggles`、弹窗 `EndFormDialog()` 结果。

## 3. 硬性约束
- 只改上述两个文件（+ 可选页面私有头文件）；不改 CMake/契约头/他人文件、不改 `assets/`。
- 布局由 `ShellLayout::pageViewport` 推导；表格/分页/弹窗尺寸由 `ui::` 契约负责；页面不写死屏幕坐标。
- 渲染与数据分离：只经 `DataServices` 接口读写；操作后必须重新 `List` 刷新；不得 include `SeedServices.h`。
- 图片缺失/加载失败必须优雅回退（占位块），不能崩溃；中文文案；字体 RAII 作用域；不 commit。

## 4. 自测与验收
```bat
cmake -S . -B build-dev3 -G "Visual Studio 18 2026" -A x64
cmake --build build-dev3 --config Release --parallel
build-dev3\Release\zzyl_desktop.exe --auto-login      :: 在住管理→房型设置；服务管理→护理项目
build-dev3\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev3
```
- 房型：6 条、图片列（占位或素材）、新增/修改/删除/启停、校验（名称/床位/费用必填）、弹窗 500 单列；
- 护理项目：9 条、12 张素材可显示、搜索名称/状态、新增/修改/删除/启停、图片选择预览、弹窗校验；
- 1024×640 / 1280×720 / 1920×1080：表格列宽比例适配、分页贴底、弹窗居中不超屏、中文正常；
- 截图 `06_room_type_1280x720.png`、`07_nursing_project_1280x720.png`；`metrics.json` 三标记位 true；
- 汇报 lead（抄架构师）：完成内容、改动文件、selftest 路径、未完成/风险。

## 5. Bug 归属
- 房型页功能/图片列/弹窗 → **cpp-dev3**（`src/pages/RoomTypeModule.cpp`）；
- 护理项目页功能/图片映射/弹窗 → **cpp-dev3**（`src/pages/NursingProjectModule.cpp`）；
- 通用组件（搜索/表格/分页/弹窗/确认/Toast）规格不符 → 先报架构师，转 **cpp-dev1**；
- 图片加载/纹理/种子数据/路由问题 → **cpp-arch**。
