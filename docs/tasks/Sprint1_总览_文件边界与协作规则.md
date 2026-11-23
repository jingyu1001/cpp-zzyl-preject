# Sprint 1 总览 · 文件边界与协作规则

> 迭代：Sprint 1（骨架 + 应用外壳 + 登录页 + 首页看板）
> 架构基线：`docs/architecture/Cpp架构设计_v0.1.md`（接口契约以该文档第 13 章与头文件为准）
> Web 端规格（必读）：`C:\Users\ASUS\Desktop\zzyl\web-zzyl-preject\docs\architecture\UI复刻规格_Cpp_1280x720_v0.1.md`
> 架构师：cpp-arch；总控：lead；测试：cpp-tester

## 0. 已对齐的 Web 端规格（所有人必须遵守）

| 项 | 规格值 | 实现位置 |
|---|---|---|
| 侧边栏 | 宽 200（折叠 54），深色 #304156，Logo 区 50，一级菜单 56 / 二级 50，选中文字 #00B8A0（无底色块） | `layout/Layout.h` tokens + `ShellModule.cpp` |
| 顶栏 | 高 50，白底，汉堡 50×50，面包屑“首页” | 同上 |
| 标签栏 | 高 34，白底，首页固定标签（#00B8A0 白字 + 8px 白点） | `ShellModule.cpp` |
| 内容区 | padding 上 24 / 右 15 / 下 52 / 左 24；页面可用区从 (224,108) 起 | `layout/Layout.cpp` |
| 品牌主色 | #00B8A0（按钮/选中/高亮），hover #33C6B3，active #00977F | `render/Theme.h` |
| 字体 | 基准 14px（微软雅黑），标题 16 粗体，辅助 12 | `ImGuiLayer.cpp` + `UiDraw` |
| 卡片 | 圆角 4、白底、极浅阴影；行间 16、两栏 24、三栏 12 | `UiDraw::Card` + `layout` tokens |
| 窗口 | 默认 1280×720；最小 1024×640；可缩放；F11 全屏 | `platform/Window.cpp` |

> 说明：外壳固定尺寸是 Web 规格明确规定的"布局约束"，不属于"写死"；页面内元素必须用 `AnchorLayout` 比例锚定或从父矩形/公式推导，禁止绝对屏幕坐标。

## 1. 本迭代共同目标

把老师给的 RuoYi-Vue3 前端「可运行页面」转译为 Windows 桌面程序（OpenGL + GLFW + ImGui），本迭代交付：

1. 可编译、可运行、可自测截图的工程骨架（已完成）；
2. 应用外壳（侧边栏 200/54 + 顶栏 50 + 标签栏 34 + 内容区，10 个一级菜单、可折叠、可导航首页）；
3. 登录页（输入 + 验证码占位 + 模拟登录 + 错误提示 + 跳转首页）；
4. 首页看板（4 统计卡 + 4 个 ImDrawList 自绘图表 + 排行列表）。

## 2. 文件边界（硬规则）

| 负责人 | 唯一可写文件 | 可选新增头文件 | 绝对不要动 |
|---|---|---|---|
| cpp-dev1 | `src/shell/ShellModule.cpp` | `include/zzyl/shell/ShellTypes.h`（仅头文件） | 其他人的文件、CMakeLists、include/zzyl 下架构头 |
| cpp-dev2 | `src/pages/LoginModule.cpp` | `include/zzyl/pages/LoginTypes.h`（仅头文件） | 同上 |
| cpp-dev3 | `src/pages/DashboardModule.cpp` | `include/zzyl/pages/DashboardTypes.h`（仅头文件） | 同上 |
| cpp-arch | CMakeLists、docs、include/zzyl/**、src/{main,core,platform,layout,render,data,app}/** | — | 三个业务模块文件 |

- 所有 `.cpp` 已在 `CMakeLists.txt` 预置，**不需要也不允许新增 .cpp / 修改 CMake**；新增 `.h` 头文件不会影响构建。
- 不改 `include/zzyl/app/AppState.h` 中的公共资源/组件定义；确需新公共能力（如全屏按钮请求字段）先向架构师/lead 提，不要自己改。
- 不 commit（git 操作由总控统一处理）；构建产物与 `artifacts/` 已被 gitignore。

## 3. 公共契约速查（详见架构文档第 3、13 章）

| 资源 | 读/写 | 说明 |
|---|---|---|
| `ShellLayout` | 只读 | 每帧由 LayoutSystem 计算：viewport/sidebar/sidebarLogo/sidebarMenu/topbar/content/pageViewport + tokens |
| `WindowState` | 只读 | 窗口/帧缓冲/DPI/全屏/帧号 |
| `RouteState` | **只读**（切页入队 Navigate 命令） | 当前页面 Login/Dashboard |
| `AuthSession` | 登录系统可写 | 登录会话 |
| `AuthFeedback` | 登录系统可写 | 登录错误提示 |
| `UiState` | 外壳可写（dev1） | sidebarCollapsed / debugOverlay |
| `CommandQueue` | 渲染层入队 | `Login` / `Navigate` / `Logout`，由 Simulation 阶段系统消费，App 每帧末清空 |
| `MenuCatalog` | 只读 | 静态菜单树（title/icon/route/children） |
| `DashboardData` | 只读 | 看板数据快照（stats/charts/ranks/updateTime） |
| `DataServices` | 应用层系统使用 | `auth` / `dashboard` 服务接口 |

常用工具：
```cpp
// 绘制（include/zzyl/render/UiDraw.h）
ui::Card(dl, rect, 8.0f);  ui::Fill/StrokeRounded/VerticalGradient/Line
ui::TextCentered/TextLeftMiddle/TextRightMiddle(dl, rect, color, "中文");
ui::ScopedFontSize font(15.0f);      // RAII：必须在 ImGui::End() 之前结束作用域！
ui::ScopedBoldFont bold(17.0f);
ui::BeginFixedWindow("##id", rect); ... ui::EndFixedWindow();
ui::InvisibleButtonAt("##btn", rect);  // 需要交互时
// 布局（include/zzyl/layout/Layout.h）
layout::Rect / EdgeInsets / GridCell / Inset / CutTop...
```

## 4. 通用红线

1. **禁止写死位置/尺寸**：任何绘制坐标必须从 `ShellLayout`/`LayoutTokens`/锚定矩形推导；重复出现的间距用 tokens。
2. **渲染与数据分离**：渲染系统里不直接调用数据服务、不直接写 `RouteState/AuthSession/DashboardData`；交互入 `CommandQueue`。
3. **字体作用域**：所有 `PushFont` 必须在 `ImGui::End()` 前 `PopFont`（一律用 `ui::ScopedFontSize`/`ScopedBoldFont`）。曾因此触发 ImGui 断言崩溃。
4. **中文**：源码 UTF-8（已配 `/utf-8`）；注释/界面文案用简体中文；确认中文不出现方块。
5. **只改自己的文件**；遇到接口不够用，先向架构师提需求（说明缺什么、为什么、建议签名）。
6. 完成后必须自测：编译 + `--auto-login` 运行 + `--selftest` 截图，再汇报 lead。

## 5. 构建 / 运行 / 自测（每人都要会用）

```bat
:: 1) 编译（Release 用于交付验收；Debug 用于断点）
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject\build" --config Release --parallel

:: 2) 正常启动（自动 Mock 登录进首页，观察交互）
build\Release\zzyl_desktop.exe --auto-login

:: 3) 自测截图（每人用自己的输出目录，避免互相覆盖）
build\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev1
```
- 自测输出：`01_login_1280x720.bmp`、`02_dashboard_1280x720.bmp`、`03_dashboard_1024x640.bmp`、`04_fullscreen.bmp`、`metrics.json`；PNG 转换命令见 README。
- 快捷键：F11 全屏 / ESC 退出全屏 / F1 调试浮层（含帧率/窗口/实体/中文字形信息）。

## 6. 汇报格式（发给 lead，抄送架构师可省略）

```
【cpp-devX · Sprint1 完成汇报】
1. 完成内容：<关键点 3-5 条>
2. 改动文件：<绝对路径列表>
3. 验证：编译=通过/失败；selftest 截图=<路径>；关键场景=<登录/导航/缩放/全屏是否通过>
4. 未完成/风险：<如需接口支持、已知缺陷>
5. 下一步建议：<可选项>
```

## 7. 验收清单（lead / cpp-tester 使用）

- [ ] `cmake --build build --config Release` 成功，无新增错误。
- [ ] `zzyl_desktop.exe` 启动：窗口 1280×720、标题「中州养老 - C++ 转译端」。
- [ ] 登录页：中文正常；输入 admin/admin123 + 任意验证码可进首页；错误密码有红色提示；空值有校验提示。
- [ ] 外壳：侧边栏 200（折叠 54）/顶栏 50/标签栏 34 尺寸与配色符合 Web 规格；首页菜单高亮 #00B8A0；汉堡键可折叠；F11 全屏后布局正常。
- [ ] 看板：4 统计卡数值与 Web `constants.js` 一致；4 个图表类型正确（折线/柱状/堆叠柱/环形）；排行列表有 4 行且占比正确。
- [ ] 缩放 1024×640 与全屏 1920×1080：无重叠、无溢出、图表不糊、中文不出现方块。
- [ ] 无崩溃、无 ImGui 断言；F1 调试浮层显示正常。

## 8. 时间与顺序

| 顺序 | 任务 | 预计 | 依赖 |
|---|---|---|---|
| 1 | dev1 外壳（可与 dev2/3 并行） | 0.5 天 | 骨架已就绪 |
| 1 | dev2 登录页（可与 dev1/3 并行） | 0.5 天 | `DataServices` 已就绪 |
| 1 | dev3 看板页（可与 dev1/2 并行） | 0.5 天 | `DashboardData` 已就绪 |
| 2 | 三人自测 + 汇报 lead | 0.2 天 | 各自完成 |
| 3 | 集成验收（lead + cpp-tester） | 0.3 天 | 全部汇报 |
