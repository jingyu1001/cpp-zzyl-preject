# Sprint 1 交付说明（C++ 转译端 · 第一天迭代）

> 迭代：2026-11-23 · 负责人：总控 lead · 架构：cpp-arch · 开发：cpp-dev1/2/3 · 测试：cpp-tester
> 输入：老师下发的 RuoYi-Vue3 前端「可运行页面」（登录页 + 应用框架 + 首页看板）
> 输出：Windows 桌面端（OpenGL 3.3 + GLFW 3.4 + ImGui 1.92.2b，VS2026 / CMake，JDK 无关）

## 1. 本迭代交付内容

| 模块 | 内容 | 主要文件 |
|---|---|---|
| 工程骨架 | CMake + VS2026、依赖路径变量（ZZYL_DEPS_ROOT）、ECS 核心（World/System/SystemScheduler）、五阶段调度 | `CMakeLists.txt`、`include/zzyl/core/*`、`src/core/*` |
| 平台层 | GLFW 窗口（默认 1280×720、最小 1024×640、Per-Monitor V2 DPI、F11 全屏/ESC 退出）、ImGui 中文字体（msyh.ttc）、GL 帧缓冲自测截图 | `src/platform/*` |
| 应用外壳 | 侧栏（200/折叠 54、10 个一级菜单、二级互斥展开、滚轮滚动、折叠 tooltip、首页高亮）、顶栏（汉堡、面包屑、管理员、用户弹窗）、标签栏（首页固定标签）、内容区 | `src/shell/ShellModule.cpp` |
| 登录页 | 三输入框（账号/密码/验证码）+ Tab + 回车、记住密码、验证码刷新、三种校验红字/红框、Mock 登录（admin/admin123）→ 首页 | `src/pages/LoginModule.cpp` |
| 首页看板 | 5 个环形统计卡（老人/床位/服务单/员工/收入，B 套数据）、4 个 DrawList 自绘图表（收益双折线+tab、入退分组柱、服务折线、年龄堆叠柱）、等级排行 4 行、等级分布环形 | `src/pages/DashboardModule.cpp` |
| 数据层 | `IAuthService` / `IDashboardService` + Mock（数据取自 Web constants.js B 套），为 Sprint 2 接 MySQL/SQLite 留接口 | `include/zzyl/data/*`、`src/data/*` |
| 架构与任务文档 | 架构设计 v0.1（分层/ECS/渲染数据分离/Web→C++ 映射/迭代计划）、Sprint1 任务单与文件边界、Sprint2 预备清单 | `docs/architecture/*`、`docs/tasks/*`、`docs/plan/*` |

## 2. 关键约束落实情况

- **ECS 架构**：Entity/Component/System + 资源表；系统按 Input→Simulation→Layout→Render→Overlay 五阶段调度，单向数据流。
- **渲染与数据分离**：渲染系统只读资源与数据快照，交互一律入 `CommandQueue`，由 Simulation 阶段系统消费（本轮修复了帧末误清空队列的时序 bug，并加了回归自测 `commandQueueTimingOk`）。
- **不写死位置**：外壳固定尺寸收敛在 `LayoutTokens`（侧栏 200/54、顶栏 50、标签栏 34），页面内部严格按 viewport 比例/锚定矩形计算；窗口缩放与全屏布局由同一公式解算。
- **自适应 + 全屏**：1280×720 / 1024×640 / 1600×900 / 1920×1080 全屏均验证无重叠溢出；窄屏（<1200）看板单列 + 内容滚动。
- **中文**：msyh.ttc 动态字形，界面中文无方块（selftest `glyphsOk: true`）；支持中文粘贴与微软拼音输入法上屏。

## 3. 构建与运行

```bat
set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
%CMAKE% -S . -B build -G "Visual Studio 18 2026" -A x64
%CMAKE% --build build --config Release --parallel

build\Release\zzyl_desktop.exe              :: 正常启动（登录页）
build\Release\zzyl_desktop.exe --auto-login :: 直接进首页（演示）
build\Release\zzyl_desktop.exe --selftest artifacts\selftest_release :: 自测截图
```

快捷键：F11 全屏 / ESC 退出全屏 / F1 调试浮层（帧率、窗口、实体、中文字形）。

## 4. 验证证据

| 证据 | 路径 |
|---|---|
| 1280×720 登录页 | `artifacts/selftest_dev2/01_login_1280x720.png` |
| 1280×720 首页看板 | `artifacts/selftest_dev3/02_dashboard_1280x720.png` |
| 1024×640 窄屏（单列+滚动） | `artifacts/selftest_dev3/03_dashboard_1024x640.png` |
| 1920×1080 全屏 | `artifacts/selftest_dev3/04_fullscreen.png` |
| 外壳交互 QA（菜单/折叠/用户弹窗/滚动） | `artifacts/shell_qa_dev1/` |
| 登录交互（成功/四类校验/中文输入） | `artifacts/verify_dev2/realfinal/` |
| 正式测试报告 | `docs/test/Sprint1_测试报告.md`（cpp-tester 出具） |

## 5. 已知限制（列入 Sprint 2）

1. 图片资源（登录背景、菜单图标）暂用渐变/几何占位 → Sprint 2 用 WIC 解码系统 API + 资源复制（无需第三方库）。
2. 登录为 Mock（验证码占位、记住密码未持久化、登录失败不自动刷新验证码）→ Sprint 2 接后端接口 + SQLite 本地存储。
3. 看板数据为 Web constants B 套静态快照，收益 tab 仅外观态 → 后续若接后端需补统计接口/周期数据。
4. 数据库缺 device/device_data/alert_rule/alert_data/reservation/family_member 等业务表（老师后续补 SQL），不影响本迭代。
5. Debug 构建 glfw Release 库导致 LNK4098 警告（仅警告）；交付验收统一 Release。

## 6. 下一步（Sprint 2 预备）

见 `docs/plan/Sprint2_预备事项清单.md`：图片/纹理（WIC）、SQLite3 本地存储、MySQL 数据接入、后台/管理员业务页（住养/护理/设备等）、通用表格-表单-弹窗组件、菜单与路由从后端 `/getRouters` 动态化。
