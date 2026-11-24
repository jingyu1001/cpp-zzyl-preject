# Sprint 2 总览 · 文件边界与协作规则

> 迭代：Sprint 2（M0 通用组件 + M1 用户管理 + M2 房型设置 + M3 护理项目 + 外壳路由扩展）
> **当前状态：✅ 可交付（tester v3 终版报告 2026-11-24：61 用例 PASS 60 / FAIL 0 / 未覆盖 1；BUG-S2-01 已闭环；详见架构 v0.2 §11.3）**
> 架构基线：`docs/architecture/Cpp架构设计_v0.2.md`（增量；v0.1 未变部分继续生效）
> 必读输入：`web-zzyl-preject/docs/architecture/UI复刻规格_Cpp_v0.2.md`、`Web功能清单_v0.2.md`、`assets_v0.2/`、`素材/服务项目图片/`
> 架构师：cpp-arch；总控：lead；测试：cpp-tester

## 1. 本迭代交付目标（M0~M3）

| 模块 | 内容 | 负责人 |
|---|---|---|
| M0 通用组件 | 搜索栏 / 工具栏 / 数据表格 / 分页 / 表单弹窗 / 确认框 / Toast / 状态标签 / 图片列 | cpp-dev1 |
| M1 用户管理 | `/system/user`：7 条种子、搜索、9 列表格、新增/编辑双列弹窗、删除/重置密码/状态开关 | cpp-dev2 |
| M2 房型设置 | `/liveIn/houseSet`：6 条种子、图片列、单列弹窗、增删改/启停 | cpp-dev3 |
| M3 护理项目 | `/nursing/project`：9 条种子 + 12 张素材图标、搜索/表格/弹窗/增删改 | cpp-dev3 |
| 外壳 | 菜单/路由扩展 5 页、目录自动展开、面包屑、标签栏、不可达提示 | cpp-dev1 |

## 2. 文件边界（硬规则）

| 负责人 | 唯一可写文件 | 绝对不要动 |
|---|---|---|
| cpp-dev1 | `src/ui/Widgets.cpp`、`src/shell/ShellModule.cpp` | 他人的文件、CMakeLists、include/zzyl 下契约头（Widgets.h/Routes.h/AppState.h/BusinessServices.h/Texture.h） |
| cpp-dev2 | `src/pages/SystemUserModule.cpp` | 同上 |
| cpp-dev3 | `src/pages/RoomTypeModule.cpp`、`src/pages/NursingProjectModule.cpp` | 同上 |
| cpp-arch | `CMakeLists.txt`、`docs/**`、`include/zzyl/**`、`src/{main,core,platform,layout,render,data,app}/**`、`assets/**` | 三个业务模块实现文件 |

- 所有 `.cpp` 已在 CMake 预置（含 3 个新页面模块与 `src/ui/Widgets.cpp`），**不需要也不允许新增 .cpp / 改 CMake**；
- 新增头文件仅允许 `include/zzyl/pages/SystemUserTypes.h`、`include/zzyl/pages/RoomTypeTypes.h`、`include/zzyl/pages/NursingProjectTypes.h`（仅放页面私有组件/资源结构）；
- 不改契约头字段/函数签名（确需扩展先找架构师）；不改 `assets/`。
- 不 commit（git 由 lead 统一）；构建目录请用 `build-dev1/`、`build-dev2/`、`build-dev3/`（已在 .gitignore）。

## 3. 启动包（骨架已就绪，dev 直接开工）

- `Release` 可构建、可运行；`--selftest` 已通过（8 图 + `commandQueueTimingOk`/`wicTextureOk`）。
- 新增基础设施（直接用，无需自行实现）：
  - `include/zzyl/ui/Widgets.h`：M0 契约（实现由 dev1 填 `src/ui/Widgets.cpp`）
  - `include/zzyl/data/BusinessServices.h` + `src/data/SeedServices.cpp`：用户 7 条 / 房型 6 条 / 护理项目 9 条种子 + CRUD 接口
  - `include/zzyl/app/Routes.h`：5 条路由映射；`RouteState.path` 已是路由唯一来源
  - `include/zzyl/render/Texture.h`：WIC 图片加载/绘制/缓存（`TextureCache` 资源已注册）
  - `ui::ToastState` 资源已注册：页面 PushToast → 外壳/页面 DrawToasts
- 3 个新页面模块当前为占位 Render System（路由可达时画占位卡片），dev 替换为真实页面即可。

## 4. 公共契约速查

```cpp
// 路由
world.Resource<RouteState>();                    // current: Login/Dashboard/SystemUser/RoomType/NursingProject；path
TryRouteFromPath(path, route); RoutePath(route); RouteTitle(route); IsRoutablePath(path);
// 数据（接口见 include/zzyl/data/BusinessServices.h）
auto* services = world.TryResource<DataServices>();   // services->user / roomType / nursingProject
// 图片
auto* textures = world.TryResource<render::TextureCache>();
const render::Texture& tex = textures->Get("assets/icons/修剪指甲@2x.png");
render::DrawImageContain(dl, tex, rect);  render::DrawImageCover(dl, tex, rect);
// 通用组件（dev1 实现）
ui::DrawSearchBar / DrawToolbar / DrawDataTable / DrawPagination / BeginFormDialog / EndFormDialog
ui::DrawConfirmDialog / PushToast / DrawToasts
// 表格事件（v0.2 契约已定稿）：result.clickedRow / result.clickedAction + result.actionRowId
//   result.switchToggles[{rowId, enabled}]（状态开关）；Actions 列逐行标签 = TableRow.cells[列] 用 '|' 分隔
// 表单弹窗：组件画标题栏+底部确定/取消；EndFormDialog() 返回 Ok/Cancel/None；
//   Ok 不自动关闭（调用方校验成功后再 state.open=false），Cancel 自动关闭
// 布局
world.Resource<layout::ShellLayout>();          // pageViewport 为页面绘制区（已含 padding）
ui::Card / Fill / TextCentered / ScopedFontSize / ScopedBoldFont / BeginFixedWindow
```
> 页面统一布局骨架：`搜索栏 → 工具栏 → 表格（占满剩余高度）→ 分页（贴底）`；空状态/加载态按 UI v0.2 §3.3；弹窗 500/600 宽、表单项间距 28、必填红星、错误红字。

## 5. 构建 / 自测（每个 dev 都要会）

```bat
:: 编译（各自构建目录）
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B build-devX -G "Visual Studio 18 2026" -A x64
"C:\...\cmake.exe" --build build-devX --config Release --parallel

:: 运行（自动 Mock 登录进入首页，然后点菜单进入自己页面）
build-devX\Release\zzyl_desktop.exe --auto-login

:: 自测（8 张截图 + metrics；每人用自己的输出目录）
build-devX\Release\zzyl_desktop.exe --selftest artifacts\selftest_devX
```
- 自测截图：`01_login / 02_dashboard / 03_dashboard_1024x640 / 04_fullscreen / 05_system_user / 06_room_type / 07_nursing_project / 08_texture_wic`（1280×720）；
- 自测 `metrics.json` 必须：`glyphsOk=true`、`commandQueueTimingOk=true`、`wicTextureOk=true`；
- 页面验证：`--auto-login` 后点「系统管理→用户管理 / 在住管理→房型设置 / 服务管理→护理项目」，或等 dev1 接好菜单/路由后点菜单。

## 5.1 调试资源调度：cpp-control（2026-11-24 新规，立即生效）

> 目的：串行化「可见窗口交互/模拟输入/前台抓屏」资源，避免多 agent 抢控制台造成调试失真。

| 必须申请（默认独占 8~10 分钟） | 无需申请 |
|---|---|
| 启动 `zzyl_desktop.exe` **可见窗口** | 各自 `build-devN` 构建 |
| 模拟鼠标/键盘（F11 全屏点验、点击/输入脚本） | `--selftest` 隐藏窗口自测（同刻不要开多个实例） |
| 前台抓屏（CopyFromScreen / PrintWindow / 可见窗口截图） | 代码审查、文档、独立临时程序 |

- **流程**：`send_message` → **cpp-control**，格式 `申请者 | 目的 | 涉及页面/功能 | 预计时长(≤15分钟) | 需要资源(交互窗口/模拟输入/截图/构建)` → FIFO 批准，同一时刻仅 1 人；批准后独占前台（默认 10 分钟，有人排队则 8 分钟）；结束立即发「释放」；**禁止同时开多个 `zzyl_desktop.exe` 实例**；调度日志由 cpp-control 维护。
- **适用**：dev1/2/3、cpp-tester、cpp-arch 一视同仁。
- 本迭代收尾（BUG-S2-01 复测、三页 1920 证据）与后续所有 Sprint 的 bug 修复/验收调试，**先申请再动**；架构派单时会写明此要求。

## 6. 汇报格式（给 lead + 抄架构师）

```
【cpp-devX · Sprint2 完成汇报】
1. 完成内容：<M0/M1/M2/M3 对应项>
2. 改动文件：<绝对路径>
3. 验证：编译=通过/失败；selftest 8 图=<路径>；页面功能自测=<列表/搜索/弹窗/增删改/缩放>
4. 未完成/风险：<契约缺接口、依赖阻塞>
5. 下一步：<需要 tester 重点测什么>
```

## 7. Bug Triage 流程（Sprint 2 起，重要）

```
cpp-tester 测试 → 测试报告只交 cpp-arch → 架构按文件归属判定责任人 →
架构派单给 dev（附复现步骤/预期/日志/截图）→ dev 修复后直接与 cpp-tester 复测 →
全部通过后测试报告移交 lead
```
**文件 → 负责人映射表**：
| 范围 | 负责人 |
|---|---|
| 通用组件（搜索/工具栏/表格/分页/弹窗/确认/Toast）`src/ui/Widgets.cpp` | cpp-dev1 |
| 外壳/菜单/路由/面包屑/标签栏 `src/shell/ShellModule.cpp` | cpp-dev1 |
| 用户管理 `src/pages/SystemUserModule.cpp` | cpp-dev2 |
| 房型设置 `src/pages/RoomTypeModule.cpp` | cpp-dev3 |
| 护理项目 `src/pages/NursingProjectModule.cpp` | cpp-dev3 |
| 登录页 `src/pages/LoginModule.cpp`（遗留） | cpp-dev2 |
| 首页看板 `src/pages/DashboardModule.cpp`（遗留） | cpp-dev3 |
| 数据/纹理/路由/布局/平台/构建/资源/文档 | cpp-arch |

> 测试报告格式建议：编号 / 页面 / 严重级 / 复现步骤 / 实际 vs 预期 / 截图路径 / 疑似文件。架构收到后 15 分钟内派单。

## 8. 验收清单（lead/tester）

- [ ] 5 页面可达：登录→首页→用户管理→房型设置→护理项目；菜单自动展开、面包屑/标签/高亮正确；不可达菜单有提示；
- [ ] 通用组件逐项符合 UI v0.2 §3（尺寸/颜色/交互/空态/loading）；
- [ ] M1：7 条数据、搜索 5 项、9 列、新增/修改双列校验、删除确认、重置密码、状态开关、Toast；
- [ ] M2：6 条数据、图片列、单列弹窗 6 字段、增删改、启停；
- [ ] M3：9 条种子、12 张素材图片列、搜索/表格/弹窗/校验、增删改；
- [ ] 1024×640 / 1280×720 / 1920×1080：列宽比例自适应、分页贴底、弹窗居中不超屏、中文正常；
- [ ] `--selftest` 8 图 + 3 个标记位 true；无 ImGui 断言/崩溃；数据交互后刷新及时。

## 9. 时间与顺序

| 顺序 | 任务 | 说明 |
|---|---|---|
| 1 | dev1 先完成 M0 基础组件（搜索/表格/分页/弹窗/Toast） | dev2/3 依赖契约，可并行先搭页面骨架 |
| 1 | dev2/dev3 先搭页面结构（路由可达 + 占位），再逐块接组件/数据 | 独立文件，不阻塞 |
| 2 | dev1 外壳路由/面包屑/标签 | 依赖 `Routes.h`（已就绪） |
| 3 | 各自自测 + 汇报 lead（抄架构师） | 用独立构建目录与 selftest 输出目录 |
| 4 | cpp-tester 正式测试 → 报告交架构 triage → dev 修复复测 → lead 验收 | 按 §7 流程 |
