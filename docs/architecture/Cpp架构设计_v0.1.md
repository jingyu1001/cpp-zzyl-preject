# 中州养老 · C++ 转译端 架构设计 v0.1（已对齐 Web 端规格）

> 项目：学校实践周「中州养老」双项目 · B. C++ 转译端（桌面软件）
> 技术栈：C++20 / OpenGL 3.3 / GLFW 3.4 / Dear ImGui 1.92.2b / glad / VS2026
> 数据层规划：MySQL（远程）+ SQLite3（本地）；本迭代全部走 Mock 数据服务
> 工程根目录：`C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject`
> 作者：总控 lead 下属 · C++ 转译端架构师（cpp-arch）
> 状态：v0.1（2026-11-23）· 骨架已在 Debug/Release 双配置编译通过并完成运行验证（见第 10 章）
>
> **输入依据**（本文档第 7 章的映射均以此为准）：
> - Web 端《UI复刻规格_Cpp_1280x720_v0.1.md》（Web 架构师产出，路径 `web-zzyl-preject/docs/architecture/`）
> - Web 端《Web架构提取_v0.1.md》（菜单/路由/登录流程/看板数据/颜色 token/数据库表清单）
> - Web 端源码 `zznursing-vue3-master`（逐行核对，重点 login.vue / layout/* / dashboard/*）

---

## 1. 目标、范围与硬约束

### 1.1 目标
把 Web 端（RuoYi-Vue3 + zznursing）的「可运行页面」逐阶段转译为 Windows 桌面程序：同一套业务语义、同等视觉风格、可离线运行的桌面体验；同时搭好可长期扩展的 ECS 骨架，为后续接 MySQL / SQLite3 留出干净的数据层接口。

### 1.2 本迭代（Sprint 1）范围
| 编号 | 内容 | 说明 |
|---|---|---|
| A | 窗口与框架 | 默认 1280×720、可缩放（最小 1024×640）、F11 全屏、DPI 感知、中文显示 |
| B | 应用外壳 | 侧边栏 200（折叠 54）+ 顶栏 50 + 标签栏 34 + 内容区（RuoYi 布局），10 个一级菜单静态版 |
| C | 登录页 | 登录卡 400×407：账号/密码/验证码占位/记住密码/登录按钮 + 模拟登录跳转 |
| D | 首页看板 | 4 统计卡 + 4 图表（自绘）+ 排行列表（先按总控范围；Web 完整结构见 §1.6） |
| E | 工程骨架 | CMake 可编译、可运行、可自测截图；给 3 名程序员下发任务单 |

### 1.3 硬约束（不可妥协）
1. **窗口默认 1280×720**，可缩放、可全屏。
2. **任何组件位置/尺寸不得写死**：必须每帧按窗口尺寸的比例或布局约束计算（外壳固定尺寸属 Web 规格明确的"布局约束"，页面内元素用归一化锚定 + 公式推导）。
3. **渲染与数据分离**：UI 状态/数据资源与绘制代码分离；页面不直接读写数据库/网络，数据层为后续 MySQL/SQLite 留接口。
4. **总体 ECS 架构**（Entity / Component / System）；局部按需使用合适的设计模式，禁止过度设计。
5. 简体中文界面与注释；交付物全部落在本项目目录；不擅自安装依赖（需要时先报 lead）。

### 1.4 非目标（本迭代不做）
- 不连真实后端、不接 MySQL/SQLite3（仅保留接口与 Mock）。
- 不实现业务页面（来访/入退/在住/服务/监测/系统管理/监控/工具），Sprint 2+。
- 不做图片资源系统（暂无解码能力；登录背景/图标用几何占位，Sprint 2 用 WIC 解码 Web 现有 png/jpg）。
- 不做多窗口 / 多视口 / 主题切换面板 / WebSocket 报警弹层。

### 1.5 口径说明：外壳固定尺寸 vs "不得写死"
Web 端 UI 规格明确规定：侧边栏 200（折叠 54）、顶栏 50、标签栏 34、内容 padding 24/15/52/24 **都是固定值**，窗口缩放时由"内容区 + 卡片公式"重排。本实现将上述数值集中在 `LayoutTokens`（唯一来源），页面元素不允许出现绝对坐标：
- 外壳：固定尺寸（布局约束）→ 窗口变化时 `ShellLayout` 每帧重算各区域矩形；
- 页面：`AnchorLayout` 归一化锚定（比例）+ `ScreenRect` 每帧解算；卡片宽度按 UI 规格 §8.1 公式从内容区宽推导（`A = W − S − 24 − 15`，两栏 17/24 : 7/24，三栏 7/24 : 7/24 : 10/24，含 gutter 修正）。
本判断已报 lead 确认；若 lead 要求"全部比例化"，只需改 `LayoutTokens` 一处，页面无需改动。

### 1.6 看板结构口径（lead 已拍板 2026-11-23）
| 优先级 | 内容 | 状态 |
|---|---|---|
| **P1（优先，主口径）** | Web 真实页面：**5 个环形统计卡**（老人/床位/服务单/员工/收入）+ 4 图表（收益折线+柱/入退柱/服务折线/年龄分布）+ 4 行等级排行 + **内容滚动**；数据驱动、stats 数量自适应 | dev3 实现中（任务单已更新） |
| P1.5（时间够再补） | 个人卡 / 常用功能 8 宫格 / 老人等级·年龄·预约总览三卡 | Sprint 2 或余量补充 |
| 数据 | `MockDashboardService` 已按 Web UI 规格 §8.3 **B 套**（2026-11-23 实测）补齐 5 项统计（老人 655 / 床位 812 / 服务单 100.35w / 员工 204 / 收入 410.81w），顺序与 Web 一致 | 已完成 |

---

## 2. 总体架构分层

```
┌───────────────────────────────────────────────────────────────────────────┐
│ 应用层 App（src/app/App.cpp）                                              │
│   主循环：事件 → 更新 WindowState → ImGui NewFrame → ECS 系统调度          │
│           → ImGui Render → 交换缓冲；自测模式下截图/缩放/全屏              │
├───────────────────────────────────────────────────────────────────────────┤
│ 表现层（Render Systems，Phase=Render）                                     │
│   ShellModule.cpp（外壳：侧栏/顶栏/标签栏/内容区，dev1）                   │
│   LoginModule.cpp（登录页，dev2） / DashboardModule.cpp（看板，dev3）      │
│   只读：ShellLayout / WindowState / RouteState / AuthSession /             │
│         MenuCatalog / DashboardData；只画，不改业务数据                     │
├───────────────────────────────────────────────────────────────────────────┤
│ 应用逻辑层（Simulation/Layout Systems）                                    │
│   RouteSystem（消费 Navigate/Logout 命令）  LayoutSystem（每帧重算布局）   │
│   DataBootstrapSystem（启动时通过数据服务加载 DashboardData）              │
│   —— 页面交互通过 CommandQueue 上抛，不在渲染层直接改状态 ——              │
├───────────────────────────────────────────────────────────────────────────┤
│ ECS 核心（include/zzyl/core）                                              │
│   World：实体注册表 + 组件池（类型擦除）+ 资源表                            │
│   ISystem / SystemScheduler：按 Phase 排序的确定性系统流水线               │
├───────────────────────────────────────────────────────────────────────────┤
│ 平台层（include/zzyl/platform）                                            │
│   Window（GLFW/OpenGL3.3、DPI、F11 全屏、最小窗口 1024×640）               │
│   ImGuiLayer（上下文、中文字体、主题样式）  Screenshot（GL 读回 BMP）      │
├───────────────────────────────────────────────────────────────────────────┤
│ 数据层（include/zzyl/data）                                                │
│   IAuthService / IDashboardService（抽象）                                 │
│   MockAuthService / MockDashboardService（本迭代实现）                     │
│   预留：MySQL 实现（远程 RuoYi/直连）、SQLite3 实现（本地缓存/会话）       │
└───────────────────────────────────────────────────────────────────────────┘
```

### 2.1 依赖方向（单向，禁止反向）
```
data ← app/systems ← pages/shell/render
platform ← app
core（无业务依赖，仅标准库）
```
- `pages/`、`shell/` **禁止** `#include` 任何 `MockServices.h` / 未来的 MySQL/SQLite 实现头；只能读资源、发命令。
- `render/UiDraw`、`layout/Layout`、`render/Theme` 不依赖 ECS 与数据层。
- 数据服务只在 `app/ModuleRegistry.cpp`（创建）与 `app/Systems.cpp`（调用）出现。

---

## 3. ECS 设计

### 3.1 设计原则
- 规模小（< 1000 实体）、以 UI 元素为主，因此**不引入**稀疏集 / 原型 / 并行调度 / 反射注册。
- 只有两类数据：**Entity + Component**（挂在实体上的可组合数据）与 **Resource**（全局单例数据）。
- 系统由 `SystemScheduler` 按阶段排序执行；同阶段内按注册顺序，先布局后渲染，保证每帧结果确定。

### 3.2 实体与组件（当前）
| 组件 | 定义位置 | 用途 | 归属 |
|---|---|---|---|
| `AnchorLayout` | `include/zzyl/app/AppState.h` | 归一化锚定（空间 + 比例 + 像素内边距 + min/max） | 公共 |
| `ScreenRect` | 同上 | 每帧由 LayoutSystem 解算出的绝对屏幕矩形 | 公共 |
| `NavItem` | 同上 | 侧边栏一级菜单项数据 | dev1 |
| `DashboardPanelView`（骨架占位） | `src/pages/DashboardModule.cpp` | 看板占位面板（dev3 会替换为正式组件） | dev3 |

实体总数（骨架）：19 个 = 10 个一级菜单 + 4 统计卡 + 4 图表 + 1 排行列表；组件类型 4 种。

### 3.3 资源（全局单例）
| 资源 | 写入者 | 读取者 | 说明 |
|---|---|---|---|
| `WindowState` | App（每帧） | LayoutSystem、外壳/页面、调试浮层 | 窗口/帧缓冲/DPI/帧号 |
| `RouteState` | RouteSystem、dev2 登录系统 | 全部渲染系统 | 当前页面（Login/Dashboard） |
| `AuthSession` | dev2 的 AuthSystem、自测钩子 | 外壳、看板 | 登录会话 |
| `AuthFeedback` | dev2 的 AuthSystem | 登录页 | 错误提示文案 |
| `UiState` | 外壳（折叠/调试开关）、App（F1） | 外壳、调试浮层 | 纯表现层状态 |
| `ShellUiState`（外壳私有） | dev1 外壳 | dev1 外壳 | 菜单展开项（unique-opened） |
| `CommandQueue` | 渲染系统（点击时入队） | RouteSystem、dev2 AuthSystem | 渲染层 → 应用层唯一通道 |
| `MenuCatalog` | ModuleRegistry（启动） | 外壳 | 静态菜单（10 项，含子菜单；后续接 getRouters） |
| `DashboardData` | DataBootstrapSystem / 自测钩子 | 看板 | 看板数据快照（只读） |
| `DataServices` | ModuleRegistry（启动） | 应用层系统 | IAuthService / IDashboardService 实例 |
| `ShellLayout` | LayoutSystem（每帧） | 外壳、页面、自测截图记录 | 外壳与页面可用区域 |

### 3.4 系统（执行顺序）
| 阶段（Phase） | 系统 | 顺序 | 职责 | 归属 |
|---|---|---|---|---|
| Simulation(100) | RouteSystem | 0 | 消费 Navigate / Logout 命令 | 架构 |
| Simulation(100) | DataBootstrapSystem | 10 | 启动时加载 DashboardData | 架构 |
| Layout(200) | LayoutSystem | 0 | 重算 ShellLayout + 解算全部 AnchorLayout → ScreenRect | 架构 |
| Render(300) | ShellRenderSystem | 100 | 侧栏/顶栏/标签栏/内容区底板与交互 | dev1 |
| Render(300) | LoginPageRenderSystem | 200 | 登录页绘制 | dev2 |
| Render(300) | DashboardRenderSystem | 200 | 看板绘制 | dev3 |

每帧时序：
```
PollEvents（F11/ESC）
→ 写 WindowState
→ ImGui NewFrame
→ Simulation：RouteSystem / DataBootstrapSystem（消费上一帧命令）
→ Layout：LayoutSystem（外壳布局 + 锚定解算）
→ Render：外壳、页面（只读资源）
→ App：调试浮层（可选）、ImGui Render、SwapBuffers
→ 清空"上一帧遗留"命令（只删帧首快照的 pending 条；本帧 Render 新入队的命令保留给下一帧）
```
> **CommandQueue 生命周期（关键，勿改）**：渲染系统在 Render 阶段入队命令 → 命令在**下一帧** Simulation 阶段被消费 → 帧末只删除帧首记录的那批 pending 命令，本帧新入队的必须保留。若帧末无条件 `Clear()`，会把本帧 Render 刚入队的 Login/Navigate/Logout 清掉，导致所有交互失效（已实际踩坑）。`--selftest` 内置了该时序的回归检查（`"commandQueueTimingOk": true`）。
>
> 禁止在 Render 阶段修改 `AuthSession`/`RouteState`/`DashboardData`；需要变更时向 `CommandQueue` 入队，由 Simulation 阶段的系统在下一帧执行（单向数据流）。

### 3.5 局部设计模式（按需，不过度）
| 场景 | 模式 | 落点 |
|---|---|---|
| 数据访问抽象 | Repository / Facade | `IAuthService`、`IDashboardService` + `Mock*` |
| 渲染层 → 应用层解耦 | Command / 消息队列 | `CommandQueue` |
| 字体作用域 | RAII（ScopedFontSize / ScopedBoldFont） | `render/UiDraw.h` |
| 系统流水线 | 模板方法（ISystem 固定生命周期） | `core/System.h` |
| 模块装配 | 模块注册钩子 | `app/ModuleHooks.h`（3 个 Register*Module） |
| 主题 | 单例 + 常量表 | `render/Theme.h` |

---

## 4. 目录结构与文件职责

```
cpp-zzyl-preject/
├─ CMakeLists.txt                     # 构建（依赖路径变量、源文件清单、MSVC /utf-8）
├─ README.md                          # 构建/运行/验证说明
├─ .gitignore                         # 忽略 build/ artifacts/ .vs/ 等
├─ docs/
│  ├─ architecture/Cpp架构设计_v0.1.md  # 本文档
│  └─ tasks/                          # 3 名程序员任务单（Sprint1）
├─ include/zzyl/                      # 公共头（接口契约）
│  ├─ version.h                       # 产品名/版本/设计分辨率
│  ├─ core/World.h                    # ECS World（实体/组件/资源）        ★架构
│  ├─ core/System.h                   # ISystem / FrameContext / Phase     ★架构
│  ├─ core/SystemScheduler.h          # 系统调度器                         ★架构
│  ├─ platform/Window.h               # GLFW 窗口/全屏/DPI/最小尺寸        ★架构
│  ├─ platform/ImGuiLayer.h           # ImGui 上下文/中文字体/中文字形检查 ★架构
│  ├─ platform/Screenshot.h           # GL 帧缓冲读回 BMP（自测）          ★架构
│  ├─ layout/Layout.h                 # Rect/LayoutTokens/ShellLayout      ★架构
│  ├─ render/Theme.h                  # 调色板（Web 规格 token）+ ImGui 样式 ★架构
│  ├─ render/UiDraw.h                 # 绘制工具（卡片/文字/字体作用域/固定窗口）★架构
│  ├─ app/AppState.h                  # 资源/组件/命令定义（公共契约）     ★架构
│  ├─ app/App.h                       # 应用主循环 + 自测模式              ★架构
│  ├─ app/Systems.h                   # 应用层系统工厂                    ★架构
│  ├─ app/ModuleRegistry.h            # 所有模块注册入口                  ★架构
│  ├─ app/ModuleHooks.h               # 3 个 Register*Module 声明         ★架构
│  ├─ data/Model.h                    # 纯数据模型                        ★架构
│  ├─ data/Services.h                 # IAuthService / IDashboardService  ★架构
│  ├─ data/MockServices.h             # Mock 数据服务                     ★架构
│  └─ data/MenuCatalog.h              # 静态菜单数据（10 项）             ★架构
├─ src/
│  ├─ main.cpp                        # 命令行参数 → App
│  ├─ core/SystemScheduler.cpp
│  ├─ platform/{Window,ImGuiLayer,Screenshot}.cpp
│  ├─ layout/Layout.cpp
│  ├─ render/{Theme,UiDraw}.cpp
│  ├─ data/{MockServices,MenuCatalog}.cpp
│  ├─ app/{App,Systems,ModuleRegistry}.cpp
│  ├─ shell/ShellModule.cpp           # dev1：应用外壳（当前为可运行骨架）
│  ├─ pages/LoginModule.cpp           # dev2：登录页（当前为可运行骨架）
│  └─ pages/DashboardModule.cpp       # dev3：首页看板（当前为可运行骨架）
└─ artifacts/                         # 自测截图/metrics（gitignore）
```

### 4.1 文件级职责边界（3 条任务线互不冲突）
| 负责人 | 可写文件 | 只读依赖 |
|---|---|---|
| cpp-dev1 | `src/shell/ShellModule.cpp`（+ 可选 `include/zzyl/shell/ShellTypes.h`） | AppState.h、MenuCatalog.h、Theme/UiDraw、ShellLayout、RouteState、AuthSession、UiState、CommandQueue |
| cpp-dev2 | `src/pages/LoginModule.cpp`（+ 可选 `include/zzyl/pages/LoginTypes.h`） | AppState.h、data/Services.h、data/Model.h、Theme/UiDraw、ShellLayout、RouteState、AuthSession、AuthFeedback、CommandQueue、DataServices |
| cpp-dev3 | `src/pages/DashboardModule.cpp`（+ 可选 `include/zzyl/pages/DashboardTypes.h`） | AppState.h、data/Model.h、Theme/UiDraw、layout/Layout.h、ShellLayout、DashboardData、AnchorLayout/ScreenRect |
| cpp-arch（本人） | CMakeLists、README、docs、include/zzyl 全部、src/{main,core,platform,layout,render,data,app}/** | — |

**规则**：不修改他人文件；不改 CMakeLists（所有源文件已预置）；不改 `AppState.h` 公共定义（确需扩展先找架构师）；所有 PushFont 作用域必须在 `ImGui::End()` 前结束。

---

## 5. 渲染与数据分离方案

### 5.1 三层数据流
```
[数据层] IAuthService / IDashboardService（Mock，后续 MySQL/SQLite）
      ↓ 只在启动时或登录时加载一次
[资源层] DashboardData / AuthSession / MenuCatalog（World::Resource，只读）
      ↓ 每帧读取（Render Systems 不允许写业务资源）
[渲染层] ImDrawList / ImGui 控件绘制
      ↑ 用户交互只入队 CommandQueue（Login / Navigate / Logout）
[应用层] Simulation Systems 消费命令 → 调数据服务 → 写回资源
```

### 5.2 具体约束
- 页面渲染系统不得直接调用 `MockAuthService::Login` 等数据方法；登录动作必须走 `CommandQueue{Type::Login}`，由 dev2 的 AuthSystem 在 Simulation 阶段调用 `DataServices.auth->Login(...)`。
- 页面不得直接写 `RouteState`；导航必须入队 `CommandQueue{Type::Navigate}`。
- `DashboardData` 只在启动时加载；后续接真实数据库后，刷新由数据系统整体替换资源。
- 渲染系统内不得做 IO（文件/数据库/网络）。

### 5.3 数据层演进（后续迭代）
```
IAuthService                      IDashboardService
├─ MockAuthService（当前）        ├─ MockDashboardService（当前）
├─ HttpAuthService（Sprint 2）    ├─ MysqlDashboardService / HttpDashboardService
└─ SqliteSessionStore（本地会话）  └─ SqliteCacheRepository（本地缓存快照）
```
- **MySQL**：优先通过 RuoYi 后端 REST 接口访问（与 Web 端同源业务语义）；若课程要求直连，再新增 MySQL Connector 实现（需 lead 安装依赖）。
- **SQLite3**：本地库用于会话持久化、离线缓存、菜单缓存；通过 `IRepository` 接口与 UI 隔离。Web 端数据库现状见《Web架构提取》§9（35 张表：17 系统表 + 14 业务表 + 4 其他；`sys_user/sys_menu/sys_role/sys_dept` 与业务表 `elder/bed/room/check_in/nursing_*` 已列出字段契约）。
- **依赖安装纪律**：sqlite3 / MySQL 客户端等本迭代未使用；到 Sprint 2 需要时先向 lead 报「名称/版本/用途/期望路径」。

### 5.4 数据与渲染分离的"可验证"证据（本迭代）
- 看板数据来自 `MockDashboardService`（Web 端 `dashboard/constants.js` 的真实数值 A/B/C 三套），渲染层对数据层的依赖只有资源类型。
- 登录自测通过数据服务完成：`IAuthService::Login(admin/admin123)` → 写 `AuthSession` → 切路由 → 看板渲染，全过程无 UI 代码参与数据获取。

---

## 6. 响应式布局系统

### 6.1 外壳布局（`ComputeShellLayout`，固定尺寸来自 Web 规格）
| 区域 | 公式 | 1280×720 | 1024×640 | 1920×1080 |
|---|---|---|---|---|
| 侧边栏 | 宽 `sidebarWidth`（200；折叠 54），高 = H | 200×720 | 200×640 | 200×1080 |
| 顶栏 | x=S, y=0, 宽 W−S, 高 50 | 1080×50 | 824×50 | 1720×50 |
| 标签栏 | x=S, y=50, 宽 W−S, 高 34 | 1080×34 | 824×34 | 1720×34 |
| 内容区 | x=S, y=84, 宽 W−S, 高 H−84 | 1080×636 | 824×556 | 1720×996 |
| 页面可用区 | 内容区 inset(24,24,15,52) | 1041×560 @(224,108) | 785×480 | 1681×920 |

> `LayoutTokens` 另含：Logo 区 50、一级菜单 56、二级菜单 50、卡片圆角 4、登录卡圆角 6、卡头 41、行间 16、两栏 gutter 24、三栏 gutter 12、基础字号 14。见 `include/zzyl/layout/Layout.h`。

### 6.2 页面内元素（两条实现路径）
1. **AnchorLayout（默认）**：
```
base = 父实体 ScreenRect（无父实体时取 AnchorSpace 区域：Viewport/PageViewport/Sidebar/SidebarMenu/TopBar）
rect = { base.x + x*base.w + inset.left,  base.y + y*base.h + inset.top,
         w*base.w - inset.left - inset.right, h*base.h - inset.top - inset.bottom }
再按 min/max 像素 clamp
```
2. **UI 规格公式**（看板等有明确公式的页面）：`A = W−S−24−15`；两栏 `rowW2=A+24`，左 `rowW2*17/24−24`、右 `rowW2*7/24−24`；三栏 `rowW3=A+12`，`c1=c2=rowW3*7/24−12`、`c3=rowW3*10/24−12`（W≥1920 时三栏等宽 `8/24`）；W<1200 单列。行高 237/278/278 固定，内容区纵向滚动（Sprint 2 接入滚动）。

### 6.3 看板锚定基线（骨架实现）
| 元素 | x | y | w | h |
|---|---|---|---|---|
| 统计卡 1~4 | 0 / 0.25 / 0.50 / 0.75 | 0 | 0.25 | 0.17 |
| 图表 1（收益） | 0 | 0.19 | 0.34 | 0.385 |
| 图表 2（入退） | 0.36 | 0.19 | 0.34 | 0.385 |
| 图表 3（服务） | 0 | 0.595 | 0.34 | 0.385 |
| 图表 4（年龄分布） | 0.36 | 0.595 | 0.34 | 0.385 |
| 排行列表 | 0.73 | 0.19 | 0.27 | 0.79 |

---

## 7. Web → C++ 映射表

### 7.1 页面 / 组件映射
| Web 源 | 关键内容 | C++ 对应 | 归属 | 状态 |
|---|---|---|---|---|
| `src/views/login.vue` | 表单卡 400×407、标题 16.4 #707070、输入 350×40、验证码 221+119、记住密码、按钮 350×40 #00B8A0、页脚 12px | `src/pages/LoginModule.cpp`：白卡 + 渐变背景占位 + 上述规格布局 | dev2 | 骨架（视觉规格已对齐） |
| `src/layout/index.vue` + `Navbar.vue` + `Sidebar/*` + `TagsView/*` | 侧栏 200 深色、Logo 50、菜单 56/50、顶栏 50、标签栏 34、内容区 | `src/shell/ShellModule.cpp` | dev1 | 静态骨架（10 菜单 + 子菜单展开 + 折叠 + 首页导航 + 标签栏） |
| `src/layout/components/AppMain.vue` | router-view | `ShellLayout::pageViewport` + 页面渲染系统 | dev1/dev3 | 已有 |
| `src/views/dashboard/*` | 5 环形 + 个人卡 + 3-tab 图表 + 常用功能 + 等级/年龄/预约三卡（见 §1.6） | `src/pages/DashboardModule.cpp`：骨架按总控 4+4+排行；P1 按 Web 规格升级 | dev3 | 骨架 |
| `dashboard/constants.js` | 老人/床位/服务单/员工/收入 三套 A/B/C 数据；收益/入退/服务/等级/年龄/预约 | `src/data/MockServices.cpp` → `DashboardData` | 架构 | 已实现（B 套，2026-11-23 对应） |
| `dashboard/index1.js` | ECharts option：折线 #4D8DFF/#00B8A0、环形轨道 #D7F1ED、等级 4 色、年龄男/女 | `render/Theme.h` 的图表色板（`chartLineBlue/Teal`、`ringTrack`、`ringMain[5]`、`elderLevel[4]`、`ageMale/Female`） | 架构/dev3 | 色板已入主题 |
| `src/api/login.js`（/login、/captchaImage） | 登录、验证码 | `data/IAuthService::Login`；验证码本迭代仅占位 | 架构/dev2 | Mock 已实现 |
| `src/router/index.js` + `getRouters` + `sys_menu` | 动态路由/菜单 | `data/MenuCatalog`：10 个一级菜单 + 子项（来源 Web架构提取 §4.4） | 架构 | 静态版已实现 |
| `assets/styles/*` | `$menuBg #304156`、品牌主色 #00B8A0、文字灰阶、卡片 4px 圆角 | `render/Theme.h` Palette | 架构 | 已实现 |
| `assets/images/login-background.jpg`、`logo.png`、`grzxhz.jpg`、`icon_*.png` | 背景/图标 | 暂无图片解码 → 几何占位；Sprint 2 用 WIC（系统 API，无需第三方库）解码并复制资源 | 架构 | 已知限制（D-06） |

### 7.2 功能映射（业务语义）
| Web 功能 | C++ 功能点 | 本迭代 |
|---|---|---|
| 登录/验证码/记住密码 | 登录卡片 + Mock 登录（admin/admin123）+ 失败提示 | dev2 完成 |
| 布局/菜单/面包屑/折叠/标签栏 | 外壳 + 10 一级菜单 + 子菜单展开 + 折叠 + 首页标签 | dev1 完成 |
| 首页数据概览 | 4 统计卡（真实 Mock 数值） | dev3 完成 |
| 首页图表 | 4 个 ImDrawList 自绘图表（折线/柱/堆叠柱/环形） | dev3 完成 |
| 首页排行/预约列表 | 排行列表（老人等级分布） | dev3 完成 |
| 其余业务模块 | 菜单占位 + "后续迭代接入" 提示 | Sprint 2+ |
| 全屏按钮 | F11 / ESC | 已实现 |
| 主题切换（亮/暗） | 本迭代固定亮色主题 | Sprint 3 |
| WebSocket 报警弹层 | 未实现 | Sprint 3（若需要） |

### 7.3 Web 端已知缺陷（不照抄，来源《Web架构提取》§7.5）
| 缺陷 | C++ 处理 |
|---|---|
| BUG-01 个人卡昵称取错层级显示空 | C++ 正常显示 `nickName` |
| BUG-02 时间筛选"本周"但区间显示今天 | 按选中项正确计算区间 |
| BUG-03 中间图表被 278 高卡片裁切 | 图表区按 200~210 高度实现，保证坐标轴完整 |
| BUG-04 五个环形图被压扁 | 环形保持正方形（按可用宽高等分） |
| BUG-05 图表不随窗口重排 | 每帧按容器矩形重算（ECS 布局天然满足） |
| BUG-09 菜单图标字段错乱 | 按语义映射图标（SVG 资源 Sprint 2 接入） |

---

## 8. 主题与 UI 规范（依据 Web 规格 §2/§8）

### 8.1 颜色
| 语义 | 色值 | 用途 |
|---|---|---|
| 品牌主色 | `#00B8A0` | 按钮/选中/高亮/标签栏选中 |
| 主色 hover/active | `#33C6B3` / `#00977F` | 按钮交互 |
| 链接 | `#008D71` | 链接文字 |
| 页面/卡片底 | `#FFFFFF` | 看板页面白底；卡片白底 + 极浅阴影 |
| 侧栏底 | `#304156` | 侧边栏 |
| Logo 区 | `#2B2F3A` | 侧栏顶部 |
| 二级菜单底 | `#1F2D3D` | 展开的子菜单 |
| 菜单文字 / hover | `#BFCBD9` / `#263445` | — |
| 菜单选中 | `#00B8A0`（文字+图标，无底色块） | — |
| 文字阶 | `#303133` / `#262626` / `#595959` / `#909399` / `#999999` / `#BFBFBF` | 主/标题/正文/图表副标题/次要/占位 |
| 边框/坐标轴 | `#E8E8E8` / `#DCDCDC` / `#D8DCE5` | 卡片/图表轴/标签栏 |
| 浅灰块 | `#F3F8F9` | 日期框、预约行 |
| 图表折线 | `#4D8DFF`（收入/入住）、`#00B8A0`（退款/退住/服务） | — |
| 环形轨道/主弧 | `#D7F1ED` + `#46CDBC / #689FFF / #ADA5EE / #FFB056 / #FE8585` | — |
| 老人等级 4 色 | `#47CBBA / #9891DF / #8EBEF3 / #FFB057` | — |
| 老人年龄男/女 | `#27C2AE` / `#FE6E6E` | — |
| 参观/预约标签 | `#27BA9B` / `#FE6E6E` | — |

### 8.2 字体与字号
| 场景 | 字号 | 字体 |
|---|---|---|
| 正文/菜单/输入框/按钮 | 14 | Microsoft YaHei（`C:\Windows\Fonts\msyh.ttc`，FontNo=0，动态字形按需加载） |
| 辅助信息/标签/更新时间 | 12 | 同上 |
| 卡片标题 | 16 粗体 | `msyhbd.ttc` |
| 环形图中心数字 | 18 粗体 | 同上 |
| 登录页标题 | 16.5（Web 16.38） | 常规，色 #707070 |
| 昵称（顶栏） | 14 粗体 | 同上 |

中文实现：ImGui 1.92 动态字体系统按需光栅化（无需 glyph ranges）；启动日志与 `--selftest` 的 `metrics.json` 均会输出中文字形验证结果（本机 `glyphsOk: true`）。

### 8.3 圆角/阴影/间距
| 元素 | 规格 |
|---|---|
| 卡片 | 圆角 4；阴影 `rgba(0,0,0,0.03) 0 2px 4px`（白底上叠两层极浅阴影保证可辨识）；无边框 |
| 登录卡 | 圆角 6；无阴影；内边距 25 |
| 输入框/按钮 | 圆角 4；输入框边框 #DCDFE6；按钮 #00B8A0 白字 |
| 标签栏标签 | 直角；选中 #00B8A0 + 白字 + 8px 白圆点 |
| 间距 | 页面 padding 24/15/52/24；卡片行间 16；两栏 24；三栏 12；卡头 41 |

---

## 9. 构建、运行与验证

### 9.1 依赖（本机已核实，未复制进仓库）
| 依赖 | 路径 | 用法 |
|---|---|---|
| VS2026 | `C:\Program Files\Microsoft Visual Studio\18` | MSVC 19.51 / v145 |
| CMake | `...\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`（4.3.1-msvc1） | 生成器 `Visual Studio 18 2026` / `Ninja` |
| GLFW 3.4 | `C:\Users\ASUS\Desktop\C++\glfw-3.4.bin.WIN64` | `lib-vc2022/glfw3.lib` 静态链 |
| glad | `C:\Users\ASUS\Desktop\C++\glad` | `src/glad.c` |
| Dear ImGui 1.92.2b | `C:\Users\ASUS\Desktop\C++\imgui-1.92.2b\imgui-1.92.2b` | 源码 + GLFW/OpenGL3 backend |
| 中文字体 | `C:\Windows\Fonts\msyh.ttc` / `msyhbd.ttc` | 运行时加载 |

CMake 变量 `ZZYL_DEPS_ROOT` 指向 `C:\Users\ASUS\Desktop\C++`（可 `-D` 覆盖）。

### 9.2 构建命令
```bat
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  -S . -B build -G "Visual Studio 18 2026" -A x64
"C:\...\cmake.exe" --build build --config Release --parallel
```
Ninja 备选：vcvars64 后 `-G Ninja -DCMAKE_BUILD_TYPE=Release`。

### 9.3 运行
```bat
build\Release\zzyl_desktop.exe              :: 正常启动（1280×720）
build\Release\zzyl_desktop.exe --auto-login :: 自动 Mock 登录进入首页（演示）
```
快捷键：`F11` 全屏 / `ESC` 退出全屏 / `F1` 调试浮层。

### 9.4 自测（无人工交互验证渲染）
```bat
build\Release\zzyl_desktop.exe --selftest artifacts\selftest_release
```
流程：第 2 帧截图登录页 → 第 3 帧 Mock 登录 → 第 5 帧截图看板 → 第 6 帧缩放到 1024×640 → 第 9 帧截图 → 第 10 帧 F11 全屏 → 第 13 帧截图 → 输出 `metrics.json`（窗口/帧缓冲/DPI/中文字形/布局矩形/ECS 统计）。BMP 转 PNG 命令见 README。

### 9.5 本迭代验证结果（2026-11-23 实测，Release）
| 验证项 | 结果 | 证据 |
|---|---|---|
| Debug 编译 | 通过（固有 LNK4098 警告，见风险 12.4） | `build\Debug\zzyl_desktop.exe` |
| Release 编译 | 通过、无警告 | `build\Release\zzyl_desktop.exe` |
| 1280×720 启动 | 通过，标题「中州养老 - C++ 转译端」；可见窗口冒烟通过 | `artifacts/desktop_smoke.png` |
| 外壳规格 | 侧栏 200×720、顶栏 1080×50、标签栏 1080×34、内容区 1080×636、页面区 1041×560@(224,108) | `artifacts/selftest_release/metrics.json` |
| 中文显示 | `glyphsOk=true`；字体 msyh.ttc + msyhbd.ttc，fontSize 14；截图无方块 | `metrics.json`、`01_login_1280x720.png` |
| 缩放自适应 | 1024×640：侧栏 200、顶栏 824×50、内容区 824×556、页面区 785×480；无重叠 | `03_dashboard_1024x640.png` |
| F11 全屏 | 1920×1080：内容区 1720×996、页面区 1681×920 | `04_fullscreen.png` |
| ECS | 19 实体 / 4 组件类型 / 11 资源 / 6 系统 | `metrics.json` |
| 自测截图 | 4 张 BMP（登录/看板/缩放/全屏），可复核 | `artifacts/selftest_release/` |
| **CommandQueue 时序修复** | ✅ 已修复并通过回归检查：selftest `"commandQueueTimingOk": true`；**cpp-dev2 真实工程复测通过**（回车/点击登录 → 首页、错误提示、中文 IME 输入均正常，见 `artifacts/verify_dev2/realfinal/`） | `src/app/App.cpp`、`artifacts/selftest_dev2/metrics.json` |

---

## 10. 迭代计划

### Sprint 1（当前，1 天）——骨架 + 外壳 + 登录 + 看板
| 交付 | 负责人 | 验收 |
|---|---|---|
| 架构文档 / 工程骨架 / 构建与自测 | cpp-arch | Debug+Release 编译通过；selftest 4 图 + metrics |
| 应用外壳（侧栏/顶栏/标签栏/子菜单/折叠/首页导航） | cpp-dev1 | 视觉对齐 Web 规格 §3/§4/§5/§6；缩放不崩 |
| 登录页（输入/验证码/模拟登录/错误提示/跳转） | cpp-dev2 | admin/admin123 可进首页；错误有提示；Enter 提交 |
| 看板页（4 卡 + 4 图 + 排行；P1 升级 Web 完整结构） | cpp-dev3 | 数值与 constants.js 一致；三类图自绘 + 环形；缩放自适应 |
| 集成验收 | cpp-tester / lead | 端到端：启动→登录→首页→缩放→全屏；无崩溃 |

### Sprint 2（2~3 天）——业务模块 + 数据层落地
- 护理/来访/入退/在住/服务等业务页面第一批（老人管理 CRUD、床位、护理等级等）：通用表格、搜索栏、分页、弹窗组件。
- SQLite3 本地库接入（会话持久化 + 菜单缓存 + 看板缓存）；MySQL 访问方案定稿。
- 图片资源接入（WIC 解码登录背景/logo/图标）；菜单图标与品牌化。
- 看板按 Web UI 规格 §8 升级（5+1+1+3 + 内容滚动 + tabs/时间筛选）。

### Sprint 3（2~3 天）——全模块覆盖 + 打磨
- 系统管理/系统监控/系统工具/AI 咨询页面；动态菜单（getRouters 映射）。
- 主题切换（亮/暗）、图表交互 tooltip、页面过渡。
- 全链路回归 + 性能（大表格/图表帧率）+ Release 打包。

---

## 11. 任务拆分与文件边界（Sprint 1）

| 任务单 | 负责人 | 可写文件 | 依赖接口 |
|---|---|---|---|
| `docs/tasks/Sprint1_cpp-dev1_应用外壳.md` | cpp-dev1 | `src/shell/ShellModule.cpp` | `MenuCatalog`、`ShellLayout`、`RouteState`、`AuthSession`、`UiState`、`CommandQueue`、`theme::Palette`、`ui::*` |
| `docs/tasks/Sprint1_cpp-dev2_登录页.md` | cpp-dev2 | `src/pages/LoginModule.cpp` | `DataServices`、`IAuthService`、`AuthSession`、`AuthFeedback`、`RouteState`、`CommandQueue` |
| `docs/tasks/Sprint1_cpp-dev3_看板页.md` | cpp-dev3 | `src/pages/DashboardModule.cpp` | `DashboardData`、`AnchorLayout`/`ScreenRect`、`layout::Layout`、`theme` 色板、`ui::*` |
| 总览 | cpp-arch → lead | — | `docs/tasks/Sprint1_总览_文件边界与协作规则.md` |

约束：三份任务单文件集合两两不相交；所有页面/外壳源文件已加入 `CMakeLists.txt`，不需要也不允许修改 CMake 或新增 .cpp。

---

## 12. 风险与决策记录

### 12.1 关键决策
1. **为何 ImGui 而非 Qt/MFC**：老师指定技术栈含 ImGui；ImGui 适合"快速高保真复刻 + DrawList 自绘图表"，与 OpenGL 直接集成。
2. **ECS 做多轻**：只做 World + 有序系统调度；不做 archetype/并行/反射。桌面 UI 实体量小，过度设计违反约束且增加理解成本。
3. **外壳固定尺寸 + 页面比例/公式布局**：严格按 Web UI 规格（侧栏 200/54、顶栏 50、标签栏 34、padding 固定）；页面元素用 AnchorLayout 与 §8.1 公式，代码中不出现绝对坐标。见 §1.5。
4. **数据层先 Mock 后替换**：接口先定（IAuthService/IDashboardService），Mock 数据取自 Web 端 constants.js（B 套与 2026-11-23 公式一致），Sprint 2 换 MySQL/SQLite 时渲染零改动。
5. **中文用系统字体 msyh.ttc + 动态字形加载**：不复制字体进仓库；启动与自测均有验证。
6. **DPI 策略**：进程 Per-Monitor V2 感知（不糊），窗口固定 1280×720 物理像素，`GLFW_SCALE_TO_MONITOR=FALSE`；高 DPI 屏上窗口物理偏小属已知取舍。
7. **验证工具**：自测模式 `glReadPixels + BMP` 输出真实帧，供 lead/测试无交互复核。

### 12.2 风险清单
| 风险 | 影响 | 对策 |
|---|---|---|
| 看板结构口径未定（总控 4+4+排行 vs Web 规格 5+1+1+3） | dev3 返工风险 | 任务单按"P0 总控范围 + P1 Web 规格"两级下发，等 lead 拍板（§1.6） |
| 登录背景图/图标缺失（无图片解码能力） | 视觉还原度下降 | 骨架渐变占位；Sprint 2 用 WIC 解码（系统 API，无需安装第三方库）并复制 Web 现有图片 |
| Debug 构建 LNK4098（glfw3.lib Release /MD vs Debug /MDd） | 仅警告，运行正常 | 交付验收统一 Release；Debug 用于断点；必要时换 glfw3_mt.lib 或统一运行库 |
| PushFont 未在 End 前 Pop | 触发 "Missing PopFont()" 崩溃（已踩坑） | 统一 `ui::ScopedFontSize/ScopedBoldFont`；任务单红线 |
| 组件遍历顺序非稳定 map 顺序 | 若有顺序依赖会抖 | 布局按父链深度排序；渲染顺序由系统顺序保证 |
| 中文 IME 输入 | 中文账号输入可能异常（Mock 账号为英文） | Sprint 2 验证；ImGui 1.92 支持平台 IME 数据 |
| 依赖路径绑定本机绝对路径 | 换机器需改 CMake 变量 | `ZZYL_DEPS_ROOT` 可覆盖；README 写明 |
| Web 端数据库缺 6+ 张业务表（Web抽取 §9.3） | 后续业务页接口会 500 | Sprint 2 前找老师要完整 SQL；本迭代三目标不依赖这些表 |

---

## 13. 附录：核心接口契约速查

```cpp
// core/World.h
using Entity = std::uint32_t;
Entity World::CreateEntity();
template <typename T, typename... Args> T& World::Add(Entity, Args&&...);
template <typename T> T* World::TryGet(Entity);
template <typename T, typename F> void World::Each(F&&);              // 单组件遍历
template <typename T, typename U, typename F> void World::Each(F&&);  // 双组件遍历
template <typename T> T* World::TryResource();
template <typename T> T& World::Resource();
template <typename T, typename... Args> T& World::EmplaceResource(Args&&...);

// core/System.h
struct FrameContext { float deltaTime; double totalTime; std::uint64_t frameIndex; };
enum class SystemPhase : int { Input=0, Simulation=100, Layout=200, Render=300, Overlay=400 };
class ISystem { virtual const char* Name() const; virtual SystemPhase Phase() const;
                virtual void OnStart(World&); virtual void Update(World&, const FrameContext&); };
void SystemScheduler::Add(std::unique_ptr<ISystem>, int order = 0);

// layout/Layout.h
struct Rect { float x, y, w, h; float Right/Bottom/CenterX/CenterY/...; bool Contains(...); };
struct EdgeInsets { float left, top, right, bottom; };
struct LayoutTokens { float topbarHeight, tagsViewHeight, sidebarWidth, sidebarCollapsedWidth,
                      sidebarLogoHeight, sidebarItemHeight, sidebarSubItemHeight,
                      contentPaddingTop/Right/Bottom/Left, cardRadius, loginCardRadius,
                      cardHeaderHeight, baseFontSize, rowGap, twoColumnGap, threeColumnGap; };
struct ShellLayout { Rect viewport, sidebar, sidebarLogo, sidebarMenu, topbar, tagsView, content, pageViewport;
                     LayoutTokens tokens; float contentScale; bool sidebarCollapsed; };
ShellLayout ComputeShellLayout(float w, float h, float contentScale, bool sidebarCollapsed);
Rect GridCell(const Rect& area, int col, int row, int cols, int rows, float gapX, float gapY);

// data/Services.h
class IAuthService { virtual data::LoginResult Login(const data::LoginRequest&) = 0; virtual void Logout(); };
class IDashboardService { virtual data::DashboardSnapshot LoadDashboard(const data::UserSession&) = 0; };

// app/AppState.h
enum class Route { Login, Dashboard };
struct RouteState { Route current; };
struct AuthSession { bool loggedIn; data::UserSession user; };
struct AuthFeedback { bool hasError; std::string message; };
struct WindowState { int width, height, framebufferWidth, framebufferHeight; float contentScale, deltaTime;
                     double totalTime; std::uint64_t frameIndex; bool fullscreen, minimized; };
struct UiState { bool debugOverlay; bool sidebarCollapsed; };
struct CommandQueue { enum class Type { Login, Logout, Navigate };
                      struct Command { Type type; std::string arg0, arg1, arg2; };
                      std::vector<Command> commands; void Clear(); };
struct AnchorLayout { AnchorSpace space; Entity parent; float x, y, w, h; EdgeInsets inset;
                      float minWidth, minHeight, maxWidth, maxHeight; };
struct ScreenRect { layout::Rect rect; };
struct NavItem { std::string title, route, icon; int index, depth; bool active, hasChildren; };

// render/UiDraw.h
class ScopedFontSize { explicit ScopedFontSize(float pixelSize); ~ScopedFontSize(); };
class ScopedBoldFont { explicit ScopedBoldFont(float pixelSize); ~ScopedBoldFont(); };
void Fill/FillRounded/StrokeRounded/Line/Card/VerticalGradient(...);
bool BeginFixedWindow(const char* id, const layout::Rect& rect);  void EndFixedWindow();
bool InvisibleButtonAt(const char* id, const layout::Rect& rect);
void Text/TextCentered/TextLeftMiddle/TextRightMiddle(...);
```

---

*文档维护：架构变更必须同步更新本文档并在 `docs/architecture/` 记录版本；任务单只描述"怎么做"，接口契约以本文档第 13 章与头文件为准。*
