# 中州养老 · C++ 转译端

把 Web 端（RuoYi-Vue3 + zznursing）的「可运行页面」逐阶段转译为 Windows 桌面程序。
技术栈：**C++20 / OpenGL 3.3 / GLFW 3.4 / Dear ImGui 1.92.2b / glad**；IDE：**Visual Studio 2026**。
数据层规划：MySQL（远程）+ SQLite3（本地）；当前迭代使用 Mock 数据服务。

> 架构设计见 [`docs/architecture/Cpp架构设计_v0.1.md`](docs/architecture/Cpp架构设计_v0.1.md)
> Sprint 1 任务单见 [`docs/tasks/`](docs/tasks/)
> 当前版本：v0.1.0（Sprint 1 骨架：窗口/中文/外壳/登录页/看板占位均已可运行）

---

## 1. 环境要求

| 依赖 | 本机路径（可用 `ZZYL_DEPS_ROOT` 覆盖） | 说明 |
|---|---|---|
| Visual Studio 2026 | `C:\Program Files\Microsoft Visual Studio\18` | MSVC 19.51 / v145 |
| CMake | `...\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` | 4.3.1-msvc1（不在 PATH，用全路径） |
| GLFW 3.4 | `C:\Users\ASUS\Desktop\C++\glfw-3.4.bin.WIN64` | 静态库 `lib-vc2022\glfw3.lib` |
| glad | `C:\Users\ASUS\Desktop\C++\glad` | `src/glad.c` + `include` |
| Dear ImGui 1.92.2b | `C:\Users\ASUS\Desktop\C++\imgui-1.92.2b\imgui-1.92.2b` | 源码 + GLFW/OpenGL3 backend |
| 中文字体 | `C:\Windows\Fonts\msyh.ttc`（回退 `msyhbd.ttc` / `simhei.ttf`） | 运行时读取，不复制进仓库 |

第三方库**不复制**进本仓库；CMake 只引用路径。若换机器，配置时覆盖依赖根目录即可：
```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DZZYL_DEPS_ROOT="D:\你的\C++依赖目录"
```

---

## 2. 构建

```bat
:: 配置 + 编译（Release 为交付验收配置）
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  -S . -B build -G "Visual Studio 18 2026" -A x64

"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  --build build --config Release --parallel
```
产物：`build\Release\zzyl_desktop.exe`。

Ninja 备选（更快，需要 vcvars64 环境）：
```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-ninja
```
> 也可以直接用 VS2026 打开本目录（CMake 工程），选择 `zzyl_desktop` 启动项。

---

## 3. 运行

```bat
build\Release\zzyl_desktop.exe                 :: 正常启动（默认 1280x720）
build\Release\zzyl_desktop.exe --auto-login    :: 自动 Mock 登录（admin/admin123）直接进首页，演示用
```
命令行参数：
| 参数 | 含义 |
|---|---|
| `--width <px>` / `--height <px>` | 初始窗口尺寸（默认 1280×720） |
| `--hidden` | 隐藏窗口启动（调试/自动化） |
| `--auto-login` | 启动后自动 Mock 登录并进入首页 |
| `--selftest [目录]` | 自测：隐藏窗口渲染并截图 1280×720 / 1024×640 / F11 全屏，输出 BMP + `metrics.json` |
| `--frames <n>` | 自测帧数上限（默认 16） |
| `--help` | 帮助 |

运行时快捷键：**F11** 全屏切换 ｜ **ESC** 退出全屏 ｜ **F1** 调试浮层（帧率/窗口/DPI/实体/中文字形）。

> 演示账号：`admin / admin123`，验证码任意非空（本迭代验证码仅占位）。

---

## 4. 自测与视觉验收（无需人工交互）

```bat
build\Release\zzyl_desktop.exe --selftest artifacts\selftest_release
```
输出：
```
artifacts\selftest_release\
  01_login_1280x720.bmp      登录页
  02_dashboard_1280x720.bmp  登录后首页看板
  03_dashboard_1024x640.bmp  缩放后布局（最小窗口档）
  04_fullscreen.bmp          F11 全屏（本机 1920x1080）
  metrics.json               窗口/帧缓冲/DPI/中文字形/布局矩形/ECS 统计
```
BMP 转 PNG（Windows PowerShell，便于查看/发群）：
```powershell
Add-Type -AssemblyName System.Drawing
Get-ChildItem artifacts\selftest_release\*.bmp | ForEach-Object {
  $png = Join-Path $_.DirectoryName ($_.BaseName + '.png')
  $img = [System.Drawing.Image]::FromFile($_.FullName)
  $img.Save($png, [System.Drawing.Imaging.ImageFormat]::Png); $img.Dispose()
}
```

---

## 5. 目录结构

```
include/zzyl/        公共头（ECS 核心、平台层、布局、主题、绘制工具、数据接口、应用状态）
src/
  main.cpp           命令行 → App
  core/              ECS：SystemScheduler
  platform/          Window(GLFW/OpenGL/DPI/全屏)、ImGuiLayer(中文字体/主题)、Screenshot
  layout/            LayoutTokens / ComputeShellLayout（响应式比例布局）
  render/            Theme（RuoYi 配色 + ECharts 色板）、UiDraw（卡片/文字/字体作用域/固定窗口）
  data/              Model / IAuthService / IDashboardService / Mock 服务 / 静态菜单
  app/               App 主循环、应用层系统（路由/布局/数据引导）、模块注册
  shell/ShellModule.cpp          应用外壳（cpp-dev1）
  pages/LoginModule.cpp          登录页（cpp-dev2）
  pages/DashboardModule.cpp      首页看板（cpp-dev3）
docs/architecture/   架构设计文档
docs/tasks/          Sprint 1 任务单
artifacts/           自测截图（gitignore，不入库）
```

## 6. 架构要点（详见架构文档）

- **总体 ECS**：`World`（实体 + 类型擦除组件池 + 资源表）、`ISystem`、`SystemScheduler`（Input/Simulation/Layout/Render/Overlay 五阶段）。
- **渲染与数据分离**：数据层接口（`IAuthService`/`IDashboardService`）→ 资源（`DashboardData`/`AuthSession`…）→ 渲染系统只读资源；交互只入队 `CommandQueue`，由 Simulation 系统处理。
- **响应式布局**：外壳尺寸严格按 Web 端 UI 规格（侧边栏 200/折叠 54、顶栏 50、标签栏 34，内容区 padding 24/15/52/24）；页面元素用 `AnchorLayout` 归一化锚定 + `ScreenRect` 每帧解算，卡片宽度按 UI 规格 §8.1 公式从内容区宽推导。代码中无 1280/720 页面绝对坐标。
- **可扩展**：`ModuleHooks.h` 暴露 3 个模块注册钩子；数据层换 MySQL/SQLite 实现时渲染代码零改动。

## 7. 当前状态（v0.1.0 / Sprint 1 骨架）

| 能力 | 状态 |
|---|---|
| 1280×720 窗口、可缩放（最小 1024×640）、F11 全屏、Per-Monitor V2 DPI | ✅ 已验证 |
| 中文显示（微软雅黑 14px 基准，动态字形） | ✅ 已验证（selftest `glyphsOk=true`） |
| 应用外壳（侧栏 200/54 + 顶栏 50 + 标签栏 34 + 内容区，10 菜单） | ✅ 静态骨架（dev1 打磨中） |
| 登录页（400×407 白卡，规格已对齐） | ✅ 占位骨架（dev2 接入真实输入/Mock 登录） |
| 首页看板（4 统计卡 + 4 图表 + 排行，规格配色） | ✅ 占位骨架（dev3 接入真实数据与自绘图表） |
| 数据层接口 / Mock 数据 / 静态菜单 | ✅ 已就绪 |
| MySQL / SQLite3 | ⏳ 未接入（Sprint 2，按纪律先向 lead 报安装需求） |

## 8. 常见问题

- **Debug 构建出现 `LNK4098: 默认库“MSVCRT”冲突`**：`glfw3.lib` 是 Release `/MD` 编译的静态库，Debug 用 `/MDd` 会告警；当前运行正常。交付验收统一用 **Release**；Debug 用于断点调试。
- **中文显示方块**：确认 `C:\Windows\Fonts\msyh.ttc` 存在；启动日志会打印 `中文字体加载成功：Microsoft YaHei`，`--selftest` 的 `metrics.json` 有 `"glyphsOk": true`。
- **ImGui 断言 `Missing PopFont()`**：在 `ImGui::End()` 之前有未弹出的 `PushFont`。统一用 `ui::ScopedFontSize` / `ui::ScopedBoldFont`，并确保作用域在 `EndFixedWindow()` 前结束。
- **改代码后布局没变**：确认改动在 `--build ... --config Release` 后重新运行的是 `build\Release\zzyl_desktop.exe`（不是旧的可执行文件）。
