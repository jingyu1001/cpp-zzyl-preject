# Sprint 1 任务单 · cpp-dev1 · 应用外壳（侧边栏 + 顶栏 + 标签栏 + 内容区）

> 负责人：cpp-dev1
> 唯一可写文件：`src/shell/ShellModule.cpp`（可新增 `include/zzyl/shell/ShellTypes.h` 头文件）
> 必读：`docs/architecture/Cpp架构设计_v0.1.md` 第 6/7/8 章；Web 端《UI复刻规格_Cpp_1280x720_v0.1.md》§3/§4/§5/§6（路径 `C:\Users\ASUS\Desktop\zzyl\web-zzyl-preject\docs\architecture\`）；Web 端截图 `assets/02_首页看板_1280x720.png`、`06_侧边栏展开_1280.png`
> 骨架现状：**可运行静态外壳已就绪**——侧栏 200（折叠 54）、Logo 区 50、10 个一级菜单（来自 Web 实际菜单）+ 子菜单展开（unique-opened）、hover/选中、顶栏 50（汉堡/面包屑/昵称/头像）、标签栏 34（首页固定标签 #00B8A0 + 白点）、内容区底板。**你的任务是在此基础上做交互完善 + 视觉 QA。**

## 1. 目标（验收画面）

1280×720：侧栏 200 深蓝灰 #304156 + Logo 区 50 + 菜单 56/50 → 顶栏 50 白底（汉堡 + 首页面包屑 + 右侧用户区）→ 标签栏 34（首页固定标签）→ 内容区白底；点汉堡折叠为 54；点一级目录展开子菜单（同时只展开一个）；点「首页」保持看板；窗口缩放 1024×640 / F11 1920×1080 下外壳尺寸不变、内容区正确重排、菜单可滚动不溢出。

## 2. 现状盘点（先读代码再动手）

`ShellRenderSystem` 已实现：
- 侧栏：底色/Logo（32×32 主色方块 + “中州养老”）/10 个一级菜单（56 高）/子菜单展开（50 高，#1F2D3D 底）/hover #263445/选中文字 #00B8A0（无底色块）/右侧展开箭头；汉堡键折叠为 54（图标居中）。
- 顶栏：白底 + 底部 1px 阴影线、汉堡键 50×50、面包屑“首页”、右侧“F11 全屏 · F1 调试”提示 + 头像 30 + 昵称 14/700。
- 标签栏：白底 + 下边框 #D8DCE5、“首页”固定标签（#00B8A0 底、12px 白字、8px 白点、无圆角、左边距 15）。
- 状态：`ShellUiState { int expandedRoot; }` 资源（在 `RegisterShellModule` 注册）；点击首页/子项发 `CommandQueue{Type::Navigate}`。

## 3. 本任务必须完成（P0）

1. **菜单滚动**：10 个一级菜单 + 展开的子菜单总高可能超过侧栏菜单区（720 时为 670），当前超出部分被直接裁掉。请实现菜单区独立纵向滚动（推荐 `ImGui::BeginChild("##menu_scroll", menuRect, false, ImGuiWindowFlags_NoScrollbar)` + 手动滚动条绘制，或 `InvisibleButton` + 滚轮/拖动切换偏移）；滚动条 6px、滑块 #C0C0C0，不影响侧栏整体视觉。
2. **折叠态弹层**：折叠成 54 时，一级目录 hover 弹出子菜单（可用 ImGui popup/自绘浮层），叶子项 hover 显示 tooltip（当前只有 tooltip）。
3. **头像下拉**：顶栏头像/昵称区域可点击，弹出下拉菜单「个人中心（占位提示）」「退出登录」；退出登录向 `CommandQueue` 入队 `Logout`（App/RouteSystem 会处理回登录页）。
4. **全屏按钮**（可选 P1，需先向架构师提接口）：顶栏右侧可加“全屏”按钮；**不要自己改 AppState/App**——若要做，向架构师说明需要的字段（例如 `UiState::requestToggleFullscreen`），由架构师加接口后再用。
5. **视觉 QA（对照 Web 截图逐项）**：菜单项左内边距 20、图标 14×14、文字起点 x=50；一级 56 / 二级 50；二级底色 #1F2D3D；选中文字 #00B8A0 且背景不变；hover #263445；Logo 与标题间距 12；标签栏标签尺寸/颜色/白点；顶栏汉堡 50×50、面包屑“首页”颜色 #97A8BE。
6. **图标**：目前是 14×14 圆角方块占位。请在**不引入图片资源**的前提下，用 DrawList 画简单的语义化几何图标（如首页=四宫格、系统=齿轮圆环、监测=圆点波纹…），或用不同形状/透明度区分；Sprint 2 架构层接入 WIC 图片/图标后再替换为真实 SVG/PNG。
7. **多分辨率检查**：1024×640、1280×720、1920×1080（F11）三档下：侧栏固定 200/54、顶栏 50、标签栏 34；菜单不溢出/可滚动；顶栏右侧元素不被挤出。

## 4. 可用接口（只读；不要改）

```cpp
// 每帧布局（LayoutSystem 已算好）
world.Resource<layout::ShellLayout>()  // viewport/sidebar/sidebarLogo/sidebarMenu/topbar/tagsView/content/pageViewport/tokens
world.TryResource<WindowState>()      // width/height/fullscreen/...
world.TryResource<RouteState>()       // Route::Login / Route::Dashboard
world.TryResource<AuthSession>()      // loggedIn/user.nickName/user.roleGroup/user.avatarText
world.TryResource<UiState>()          // sidebarCollapsed（可写）/ debugOverlay
world.TryResource<ShellUiState>()     // expandedRoot（可写，你的私有资源）
world.TryResource<CommandQueue>()     // 入队 Navigate / Logout
world.TryResource<data::MenuCatalog>()// roots[10]：title/icon/route/children
// 绘制/交互
theme::Colors()  theme::F()  ui::Card/Fill/Text...  ui::ScopedFontSize/ScopedBoldFont
ui::BeginFixedWindow(id, rect) / ui::EndFixedWindow()
ui::InvisibleButtonAt(id, rect); ImGui::IsItemHovered()/IsItemClicked()/SetTooltip(...)
// 布局工具
layout::Rect / EdgeInsets；若做菜单滚动，可用 ImGui::BeginChild + SetCursorScreenPos
```

## 5. 硬性约束

- 只改 `src/shell/ShellModule.cpp`；新增头文件仅限 `include/zzyl/shell/ShellTypes.h`；**不要改 CMakeLists / 公共头 / 其他模块文件**。
- 外壳尺寸必须来自 `shell.tokens`（200/54/50/34/56/50），不得出现魔法数；菜单行位置由 `shell.sidebarMenu` 推导。
- 颜色一律用 `theme::Colors()`；不新增颜色常量。
- 不直接写 `RouteState/AuthSession`；导航/退出登录入队 `CommandQueue`。
- **字体作用域红线**：所有 `ui::ScopedFontSize`/`ScopedBoldFont` 必须在 `ui::EndFixedWindow()` 之前结束（曾触发 ImGui "Missing PopFont()" 断言）。
- 中文注释/文案；不要给窗口加 ImGui 自带的标题栏/滚动条影响外壳观感。

## 6. 自测与验收

```bat
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject\build" --config Release --parallel
build\Release\zzyl_desktop.exe --auto-login
build\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev1
```
- `--auto-login` 检查：首页高亮、点各一级目录展开/收起、展开“服务管理”后可见 4 个子项、菜单可滚动；折叠 54 后图标居中且 tooltip/popup 正常；头像下拉能退出回登录页。
- 截图：`artifacts\selftest_dev1\02_dashboard_1280x720.png`（BMP 转 PNG 命令见 README），与 Web `assets/02_首页看板_1280x720.png` 对比侧栏/顶栏/标签栏。
- 汇报 lead：完成内容、改动文件、编译/自测结果、截图路径、未完成项。

## 7. 提示 / 常见坑

- 菜单滚动如果用 `BeginChild`，注意 child 的坐标是屏幕坐标（`SetCursorScreenPos`），且 child 内不能再画到 child 外面。
- 折叠态弹层：用 `ImGui::BeginPopup` 需要 ID 上下文；也可以自绘浮层到 `GetForegroundDrawList()`（简单可控）。
- 菜单项点击检测在 `InvisibleButtonAt` 后立即取 `IsItemHovered/IsItemClicked`，再画文字，避免状态被后续 item 覆盖。
- 隐藏的 ImGui 窗口不会自动裁剪背景 drawlist 绘制，若滚动请用窗口内 DrawList（`GetWindowDrawList()`）。
