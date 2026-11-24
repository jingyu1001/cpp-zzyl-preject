# Sprint 2 任务单 · cpp-dev2 · M1 系统管理-用户管理

> 负责人：cpp-dev2
> 可写文件：`src/pages/SystemUserModule.cpp`（可新增 `include/zzyl/pages/SystemUserTypes.h` 放页面私有组件/状态）
> 契约（只读）：`include/zzyl/ui/Widgets.h`、`include/zzyl/app/Routes.h`、`include/zzyl/app/AppState.h`、`include/zzyl/data/BusinessServices.h`
> 必读：`UI复刻规格_Cpp_v0.2.md` §2/§3/§4.1/§7/§8；`Web功能清单_v0.2.md` §3.1；截图 `assets_v0.2/14_系统管理-用户管理.png`、`50~52`、`55`
> 骨架现状：`SystemUserModule.cpp` 为占位页面（路由可达时画标题卡）。数据服务 `DataServices.user`（`SeedUserService`，7 条）已就绪。

## 1. 交付目标（按 Web 用户管理页复刻）

页面结构（`ShellLayout::pageViewport` 内，顶部 search → toolbar → table → pagination）：
1. **搜索栏**（`ui::DrawSearchBar`，字段顺序）：
   - 部门（Select，选项来自 `services->user->DeptNames()`，整行/宽 240）
   - 用户名称（Text）、手机号码（Text）、状态（Select：全部/正常/停用）、创建时间（DateRange，占位实现可只存字符串）
   - 搜索/重置按钮 80×32；搜索触发 `List(UserQuery)`；重置清空
2. **工具栏**（`ui::DrawToolbar`）：新增（可用）/修改（需勾选 1 条）/删除（需勾选）/导入（禁用+Toast“二期支持”）/导出（禁用或导出 CSV 可留二期）；右侧 3 个圆图标可先省略或画占位
3. **数据表格**（`ui::DrawDataTable`，列顺序）：勾选(50) / 用户编号 / 用户名称 / 用户昵称 / 部门 / 手机号码(120) / 状态(Switch) / 创建时间(160) / 操作(Actions：修改|删除|重置密码|分配角色)
   - 数据源：`services->user->List(query)`（分页本地切片，10 条/页，共 7 条）
   - 状态开关点击：`SetStatus` → 成功 Toast「修改成功」→ 刷新；失败 Toast 错误
   - 操作列：修改→打开回填弹窗（标题“修改用户”，隐藏用户名称/密码）；删除→确认框「确定删除该用户吗？」→ `Remove({id})`；重置密码→弹窗输入（5~20）→ `ResetPassword`；分配角色→Toast「跳转分配角色（二期）」
4. **分页**（`ui::DrawPagination`）：共 N 条 / 10 条/页 / 页码 / 前往 N 页；贴内容区底部（长页面可整体滚动）
5. **新增/修改弹窗**（`ui::BeginFormDialog`，`DialogState.width=600`，双列；**组件负责标题栏+底部确定/取消**，页面只画表单内容）：
   - 字段：用户昵称*（maxlength 30）/ 归属部门（下拉或文本）/ 手机号码（11 位校验）/ 邮箱 / 用户名称*（仅新增，2~20）/ 用户密码*（仅新增，5~20，禁 `< > " ' \ |`）/ 性别（下拉 男/女/未知）/ 状态（单选 正常/停用）/ 岗位 / 角色 / 备注
   - 校验：必填空 → 字段下方 12px 红字 + 边框变红；用户名称重复 → 服务返回错误 Toast；确定调用 `Add/Update`；
   - **提交流程（v0.2 契约）**：`if (state.dialog.open) { if (ui::BeginFormDialog(page, state.dialog)) { 画字段; } auto r = ui::EndFormDialog(); if (r==Ok) { 校验; 成功→服务调用+Toast+state.open=false+刷新; 失败→保持 open 显示红字 } if (r==Cancel) { 清理编辑态 } }`
6. **Toast**：统一 `PushToast(*world.TryResource<ui::ToastState>(), text, type)`，由外壳/页面每帧 `DrawToasts`；操作成功/失败都要提示
7. **表格事件（v0.2 契约）**：操作列用 `result.clickedAction + result.actionRowId` 定位行与动作（Actions 列逐行标签写在 `TableRow.cells[操作列]`，如 `"修改|删除|重置密码|分配角色"`）；状态开关用 `result.switchToggles`（遍历 `{rowId, enabled}` 调 `SetStatus`）；勾选用 `result.checkedIds`

## 2. 数据与状态（渲染与数据分离）
- **页面私有资源**（在 `RegisterSystemUserModule` 里 `world.EmplaceResource<SystemUserPageState>()`）：
  ```cpp
  struct SystemUserPageState {
      std::vector<data::UserRecord> rows;          // 当前查询结果
      std::vector<int> checkedIds;
      ui::SearchField dept, userName, phone, status, createTime;
      ui::PaginationState pagination;
      ui::DialogState dialog;
      data::UserRecord editing;  bool isEdit=false; bool showDialog=false;
      ui::DialogState resetDialog; char resetPassword[32] = {};
      bool confirmDelete = false; std::string confirmText;
  };
  ```
- 页面系统（Render 阶段）：`if (route->current != Route::SystemUser) return;`；首帧/查询变化时调用服务 List（不要每帧查）；渲染只读 `state`；交互更新 `state` 或调服务（服务是内存实现，同步返回）。
- **禁止**直接 `#include "zzyl/data/SeedServices.h"`（只用接口/`DataServices`）；禁止把 7 条数据写死在页面里。

## 3. 硬性约束
- 只改 `src/pages/SystemUserModule.cpp`（+ 可选 `include/zzyl/pages/SystemUserTypes.h`）；不改 CMake/契约头/他人文件。
- 所有位置/尺寸由 `pageViewport` 与组件高度辅助函数推导；弹窗 600 宽、居中、距顶 6vh；表格列宽由组件按比例适配（页面传基准 width）。
- 颜色/字号用 `theme::Colors()` + `ui::` 辅助；字体作用域 RAII；`PushFont` 必须在 `ImGui::End()` 前 Pop。
- 交互成功后必须刷新表格数据（重新 `List`），不能只改本地拷贝。
- 中文文案；不 commit。

## 4. 自测与验收
```bat
cmake -S . -B build-dev2 -G "Visual Studio 18 2026" -A x64
cmake --build build-dev2 --config Release --parallel
build-dev2\Release\zzyl_desktop.exe --auto-login     :: 系统管理→用户管理
build-dev2\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev2
```
- 7 条数据、分页「共 7 条」；搜索名称/手机号/状态/部门可过滤；重置恢复；
- 新增（校验空值/重复用户名）→ 列表 +1；修改/删除/重置密码/状态开关均 Toast 并刷新；
- 弹窗 600 双列、字段完整、必填红星/红字校验；1024×640 与全屏下弹窗/表格/分页正常；
- 截图 `05_system_user_1280x720.png` 与 Web `assets_v0.2/14_系统管理-用户管理.png` 对比（允许差异见 UI v0.2 §10 D-20~D-25）。
- 汇报 lead（抄架构师）：完成内容、改动文件、selftest 路径、未完成/风险。

## 5. Bug 归属
- 用户管理页功能/校验/数据交互 → **cpp-dev2**（`src/pages/SystemUserModule.cpp`）；
- 通用组件（搜索/表格/分页/弹窗/Toast）规格不符 → 先报架构师，转 **cpp-dev1**；
- 数据接口/种子数据问题 → **cpp-arch**。
