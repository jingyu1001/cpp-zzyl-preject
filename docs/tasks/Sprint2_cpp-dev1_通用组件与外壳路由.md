# Sprint 2 任务单 · cpp-dev1 · M0 通用组件 + 外壳路由扩展

> 负责人：cpp-dev1
> 可写文件：`src/ui/Widgets.cpp`（M0 实现）、`src/shell/ShellModule.cpp`（外壳路由扩展）。
> 契约（只读，不可改）：`include/zzyl/ui/Widgets.h`、`include/zzyl/app/Routes.h`、`include/zzyl/app/AppState.h`、`include/zzyl/data/MenuCatalog.h`
> 必读：`docs/architecture/Cpp架构设计_v0.2.md` §5/§6；`UI复刻规格_Cpp_v0.2.md` §2/§3/§4；截图 `assets_v0.2/50_通用_用户管理列表_1280x720.png`、`51/52/53/54/55`
> 骨架现状：`src/ui/Widgets.cpp` 为空实现（函数返回空结果）；外壳已具备 10 组菜单/折叠/首页路由（v0.1），Sprint 2 需扩到 5 页可达。

## 1. 交付目标

### M0 通用组件（优先，dev2/3 依赖）
按 `include/zzyl/ui/Widgets.h` 契约实现 `src/ui/Widgets.cpp`：

| 组件 | 规格要点（UI v0.2 §3） |
|---|---|
| `DrawSearchBar` | 白底；label 68 右对齐 14px #606266；控件 240×32、边框 #DCDFE6、圆角 4、placeholder #A8ABB2；行距 28；搜索 80×32 #00B8A0 白字、重置 80×32 白底 #606266；Enter 触发 search |
| `DrawToolbar` | 80×32 圆角 4、14px：新增 `#E5F7F5/#00B8A0`、修改 `#F0F9EB/#B3E19D`、删除 `#FEF0F0/#FAB6B6`、导入 `#F4F4F5/#909399`、导出 `#FDF6EC/#E6A23C`；按钮间距 10；返回点击下标 |
| `DrawDataTable` | 表头 40 高、`#F8F8F9` 底、13px `#515A6E`、下边框 `#EBEEF5`；数据行 49 高、14px `#606266`、行 hover `#F5F7FA`；勾选列 50；`CellKind`：Text/Image（60×60 cover 圆角 2，用 `TextureCache`）/StatusTag（`#F0F9EB`/`#67C23A`、`#FEF0F0`/`#F56C6C`，高 24 圆角 4 12px）/Switch（40×20，开 `#00B8A0`）/Actions（主色文字按钮，间距 8）；空态居中「暂无数据」；列宽按 `width` 比例适配可用宽，超宽横向滚动；**返回 `clickedRow / clickedAction + actionRowId / switchToggles / checkedIds`（v0.2 契约定稿，见 `Widgets.h` 注释）**：Actions 列逐行标签 = `TableRow.cells[操作列]` 用 `'|'` 分割（如“修改|删除|启用”，随行状态变化），`clickedAction` 为该行标签下标；Switch 切换回传 `{rowId,enabled}` |
| `DrawPagination` | 高 32、右对齐；`共 N 条`、每页条数下拉（10/20/30/50）、页码 32×32 方形圆角 2（当前页 `#00B8A0` 白字）、`前往 N 页`；改页/改条数返回 changed |
| `BeginFormDialog/EndFormDialog` | 宽 500/600、圆角 8、距顶 6vh、标题栏 40 高 18px、遮罩半透明黑、**底部确定/取消由组件绘制**；`EndFormDialog()` 返回 `DialogResult{None/Ok/Cancel}`：Ok 不自动关闭（调用方校验成功后再 `state.open=false`）、Cancel/× 自动 `state.open=false`；调用方只画内容区 |
| `DrawConfirmDialog` | 420 宽、圆角 4、标题 18、内容 14 #606266、取消/确定（60×32，确定主色）、返回 Ok/Cancel/None |
| `PushToast/DrawToasts` | 顶部居中 top 16、高 38、圆角 4、14px；成功 `#F0F9EB/#67C23A`、警告 `#FDF6EC/#E6A23C`、错误 `#FEF0F0/#F56C6C`；3 秒自动消失 |
| 尺寸辅助 | `SearchBarHeight/ToolbarHeight/PaginationHeight` 与实现一致（页面据此排版） |

### 外壳路由扩展（其次）
- 使用 `RouteState.path`（唯一来源）+ `IsRoutablePath/TryRouteFromPath`：
  - 菜单叶子点击：可达 → 入队 `CommandQueue{Type::Navigate, arg0=route}`；不可达 → `PushToast(ToastState, "「X」将在后续迭代接入", Error/Warning)`；
  - 进入子页面时自动展开父目录（`ShellUiState.expandedRoot`），同级唯一展开；
  - 菜单高亮：`RouteState.path` 与 `MenuEntry.route` 前缀匹配，选中文字 `#00B8A0`（无底色块）；
- 面包屑：首页 / 组名 / 页面名（用 `RouteTitle` + 菜单上下文），当前项 `#97A8BE`；
- 标签栏：首页固定标签 + 当前页标签（12px、选中 `#00B8A0` 白字 + 8px 白点、直角）；切换/关闭可留 Sprint 3；
- 顶栏/侧栏视觉延续 v0.1，不改 shell 尺寸 tokens（200/54/50/34）。

## 2. 硬约束
- 只改上述 2 个文件；不改契约头/CMake/他人文件；不新增 .cpp。
- 组件尺寸/颜色全部来自 UI v0.2 §3 规范（可设 `Widgets.cpp` 内部常量，但对外行为/尺寸要符合）；页面内不写死坐标由页面自己负责。
- 字体作用域用 `ui::ScopedFontSize/ScopedBoldFont`，必须在 `ImGui::End()` 前结束；DrawList 裁剪配对。
- 组件不得读取业务数据/数据服务；只做绘制与交互采集。

## 3. 自测与验收
```bat
cmake -S . -B build-dev1 -G "Visual Studio 18 2026" -A x64
cmake --build build-dev1 --config Release --parallel
build-dev1\Release\zzyl_desktop.exe --auto-login          :: 点菜单进 3 个新页面
build-dev1\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev1
```
- 点 系统管理→用户管理 / 在住管理→房型设置 / 服务管理→护理项目可进入（页面由 dev2/3 实现，当前为占位卡片）；
- 不可达菜单（如角色管理）点击出现 Toast 提示且不崩；
- 面包屑显示「首页 / 系统管理 / 用户管理」；菜单组自动展开；
- 自测 8 图 + `metrics.json` 三个标记位 true；1024×640 / 全屏下菜单/面包屑/标签正常；
- 汇报 lead（抄架构师）：完成内容、改动文件、selftest 路径、未完成/风险。

## 4. Bug 归属
- UI v0.2 组件规格不符 / 表格渲染 / 分页 / 弹窗行为 / Toast → **cpp-dev1**（`src/ui/Widgets.cpp`）；
- 菜单/路由/面包屑/标签/高亮 → **cpp-dev1**（`src/shell/ShellModule.cpp`）；
- 契约缺接口 → 找 cpp-arch（不要自行改契约头）。

## 5. 本轮 bug 修复任务（tester Sprint2 首轮 → 架构 triage）

| Bug 编号 | 级别 | 现象/期望 | 责任文件 | 状态 |
|---|---|---|---|---|
| **BUG-S2-01** | P2 | 护理项目表格（10 列、需横向滚动）**横向滚动条与最后一行内容重叠**；期望滚动条独立占位（表格内容区底部预留滚动条高度，不遮行内容/分页） | `src/ui/Widgets.cpp`（`DrawDataTable` 横向滚动条） | 🟡 待修复 |

修复要求：
1. 表格内容区高度 = 可用高度 − 横向滚动条高度（含 4~8px 间距），滚动条绘制在内容区下方，不覆盖最后一行；分页区不受影响；
2. 复测两档：**1280×720 与 1024×640**（护理项目页），确认滚动条不遮行、分页可见、横向滚动可用；
3. Release 重新构建通过；截图/说明发 **cpp-tester 复测**（同时抄架构），tester 复测通过后我在架构 §11.2 把状态改为 ✅；
4. 若修复需调整 `Widgets.h` 契约（预计不需要），先找架构师。

> P2-2（用户管理整页滚动工具区滚出）与 P2-3（菜单展开自动滚动）经 lead/架构判定为**设计一致/接受现状**（D-28/D-29），不在本任务范围，不需要修。
