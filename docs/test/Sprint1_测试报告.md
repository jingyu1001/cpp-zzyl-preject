# 中州养老 · C++ 转译端 —— Sprint 1 集成验收测试报告

> 测试对象：`zzyl_desktop.exe`（Release，2026-11-23 23:13 版本，含 cpp-dev1/2/3 最终代码 + cpp-arch CommandQueue 时序修复）
> 测试者：cpp-tester（测试角色，不写业务代码）｜委托：总控 lead
> 报告时间：2026-11-23 23:45｜工程目录：`C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject`
> 依据：`docs/tasks/Sprint1_总览_文件边界与协作规则.md` §7 验收清单、`docs/architecture/Cpp架构设计_v0.1.md`、Web UI 规格 `UI复刻规格_Cpp_1280x720_v0.1.md`

---

## 0. 结论摘要

| 项 | 结果 |
|---|---|
| 用例/检查点总数 | **88**（真机自动化 60 + 确定性自测 4 + 构建 4 + 代码审读 6 + 开发者证据复核 3 + 数据等价性核对，部分合并计数见 §2） |
| 通过 | **88** |
| 失败 | **0** |
| 阻塞缺陷（P0/P1） | **0** |
| 一般缺陷（P2） | 6 条（均为口径/遗留/非阻塞，见 §3） |
| **结论** | **可交付**：Sprint 1 目标（骨架 + 外壳 + 登录页 + 首页看板）功能与视觉达标，构建可复现、无崩溃、无 ImGui 断言、中文无方块；P2 项建议 Sprint 2 处理，不阻塞本次验收。 |

---

## 1. 环境与构建信息

| 项 | 值 |
|---|---|
| 操作系统 | Windows 11 家庭中文版 10.0.26300（x64） |
| CPU / 内存 | Intel Core i5-10300H @2.50GHz / 15.8 GB |
| 显卡 / OpenGL | Intel 核显，OpenGL **3.3.0** / GLSL 3.30（驱动 build 26.20.100.7985）；显示 1920×1080 @100% DPI |
| 工具链 | VS2026（MSVC 19.51 / v145）、CMake 4.3.1-msvc1、生成器 Visual Studio 18 2026 |
| 三方库 | GLFW 3.4（静态链）、glad、Dear ImGui 1.92.2b（`C:\Users\ASUS\Desktop\C++`） |
| 被测产物 | `build\Release\zzyl_desktop.exe`，846,336 B，2026-11-23 23:13:58，SHA256 `5A94CC99…929711` |
| 共享构建 | **由 lead 于 23:13 完成**（三名程序员最终代码 + 架构师 CommandQueue 时序修复） |
| 可复现性验证 | ① 增量重建（共享 build）0.8 s 无改动通过；② **独立干净构建**（`artifacts\test_tester\build_clean`，全新目录 configure+build）**40.9 s 成功**，产物同尺寸 846,336 B，自测输出/行为一致；③ 两产物 PE 哈希不同，系 MSVC 内嵌构建路径/时间戳差异，**不属于可复现性缺陷** |
| Git 状态 | 分支 `main` **尚无任何提交**；工程文件全部未跟踪（`?? .gitignore / CMakeLists.txt / README.md / _tools/ / docs/ / include/ / src/`）——符合"测试不 commit、git 由总控统一处理"约定 |

---

## 2. 用例总表

> 结果标注：`PASS`＝本次实测通过；`PASS*`＝采信开发者实测证据 + tester 复核截图（自动化模拟输入不稳定的用例，按 lead 指示不判 FAIL）；`PASS**`＝代码审读 + 规格/契约核对。
> 证据简写：`live/`=`artifacts/test_tester/live/`，`st/`=`artifacts/test_tester/selftest/`，`dev2/`=`artifacts/verify_dev2/realfinal/`，`dev1/`=`artifacts/shell_qa_dev1/`，`dev3/`=`artifacts/selftest_dev3/`。

### A. 构建 / 环境（4）

| 编号 | 操作 | 期望 | 实测 | 结果 | 证据 |
|---|---|---|---|---|---|
| AC-01 | 增量重建共享 build | 编译通过 | 0.8 s 通过，无新增错误 | PASS | 控制台记录 |
| AC-02 | 独立目录全新 configure+build | 从零可编译、产物一致 | 40.9 s 成功；exe 同为 846,336 B | PASS | `artifacts/test_tester/build_clean/Release/` |
| AC-03 | `--selftest artifacts\test_tester\selftest` | 4 张确定性截图 + metrics、无崩溃 | 4 图 + metrics；`commandQueueTimingOk=true`、`glyphsOk=true`、退出码 0 | PASS | `st/01…04.bmp/.png`、`st/metrics.json` |
| AC-04 | git 工作树检查 | 无提交、不动 git | `main` 无任何 commit，全部未跟踪 | PASS | `git status` 输出 |

### B. 窗口（16）

| 编号 | 操作 | 期望 | 实测 | 结果 | 证据 |
|---|---|---|---|---|---|
| W-01 | 启动读窗口标题 | 「中州养老 - C++ 转译端」 | 实际同左（连字符 `-`，与验收清单 §7 一致；任务书写的「·」疑为笔误→见 P2-04） | PASS | 控制台 GetWindowText |
| W-02 | `GetClientRect` | 1280×720 | 客户区 1280×720（外框 1296×759） | PASS | `live/01_…` |
| W-03 | 读窗口样式 | 可缩放 | `WS_THICKFRAME+MAXIMIZEBOX`，style=0x16CF0000 | PASS | 控制台 |
| W-04 | SetWindowPos 请求 800×600 | 被最小尺寸钳制为 1024×640 | 实际 1024×640 | PASS | `live/02_…1024x640.png` |
| W-05 | 缩放到 1024×640 | 布局正常 | 1024×640 通过 | PASS | `live/03_…` |
| W-06 | 缩放到 1600×900 | 布局正常 | 1600×900 通过 | PASS | `live/04_…1600x900.png` |
| W-07 | 还原 1280×720 | 还原成功 | 1280×720 | PASS | 控制台 |
| W-08 | 按 F11 | 全屏 1920×1080 | 客户区=1920×1080、窗口=1920×1080 | PASS | `live/05_…fullscreen_F11.png` |
| W-09 | 全屏下按 ESC | 退出全屏并还原 | 回到 1280×720 | PASS | `live/06_…esc_back.png` |
| W-10 | 按 F1 | 显示调试浮层 | 与无浮层帧差异 3040 采样点，浮层含 FPS/窗口/路由/ECS/中文字形 | PASS | `live/08_…with_F1.png` |
| W-11 | 再按 F1 | 隐藏浮层 | 与初始帧差异 0 | PASS | `live/09_…hide_F1.png` |
| W-12 | WM_CLOSE 关窗 | 退出码 0 | exit=0 | PASS | 控制台 |
| W-13 | 查启动日志 | 中文字体加载成功 | `中文字体加载成功：Microsoft YaHei (msyh.ttc)` | PASS | stdout |
| W-14 | 查字体就绪 | regular+bold 双字体 | `regular=msyh.ttc bold=msyhbd.ttc` | PASS | stdout |
| W-15 | 查 stderr | 无异常 | 全程 stderr 为空 | PASS | stdout/stderr 日志 |
| W-16 | 查 OpenGL 上下文 | 3.3 Core | `OpenGL 3.3.0 / GLSL 3.30` | PASS | stdout |

### C. 登录页（20）

| 编号 | 操作 | 期望 | 实测 | 结果 | 证据 |
|---|---|---|---|---|---|
| L-01 | 启动即登录页 | 白卡 + 中州养老标题 + 预填 admin/admin123、验证码空、记住密码、登录按钮、版权 | 全部符合；中文无方块 | PASS | `live/10_…login_default.png`、`st/01_login_1280x720.png` |
| L-02 | 空验证码点登录 | 红字「请输入验证码」、不跳转 | 红字出现（错误区红像素 104），仍在登录页 | PASS | `live/11_l2_02_empty_captcha.png` |
| L-03 | 空验证码红框 | 验证码输入框红框 | 红框像素 451 | PASS | 同上 |
| L-04 | 空账号提交 | 红字「请输入您的账号」、不跳转 | 红字 120 像素，仍在登录页 | PASS | `live/12_l2_03_empty_username.png` |
| L-05 | 空账号红框 | 账号输入框红框 | 红框像素 706 | PASS | 同上 |
| L-06 | 空密码提交 | 红字「请输入您的密码」、不跳转 | 红字出现，仍在登录页 | PASS | `live/13_…` |
| L-07 | 空密码红框 | 密码输入框红框 | tester 自动化在连续错误态后偶发无法激活密码框（harness 局限），采信 dev2 实测图：密码框红框+红字 | PASS* | `dev2/08_error_short_password.png`（dev2 实测、tester 复核）、`dev2/_zoom_0708.png` |
| L-08 | 短密码（`123`） | 红字「密码长度不能少于 6 位（演示密码 admin123）」 | 红字出现，不跳转 | PASS | `live/14_l2_05_short_password.png`、`dev2/08_…` |
| L-09 | 错误口令（`wrong123`） | 红字「账号或密码错误（演示账号：admin / admin123）」 | 红字出现，不跳转 | PASS | `live/15_l2_06_wrong_password.png`、`dev2/10_…` |
| L-10 | admin/admin123+验证码点「登录」 | 进入首页看板 | 首轮真机点击实测成功；路径与回车一致（渲染层入队 Login→AuthSystem→写会话/路由）；dev2 实测同结论 | PASS* | `dev2/15_dashboard_after_login.png`、`dev2/_sheet_interactions.png` |
| L-11 | 回车提交 | 进入首页看板 | 侧栏 200 出现，进入看板 | PASS | `live/17_l2_08_enter_dashboard.png` |
| L-12 | 勾选记住密码 | 复选框变 #00B8A0 | 白 `#FFFFFF` → 青 `#00B8A0` | PASS | `live/19_l2_10_remember_on.png`、`dev2/03_…` |
| L-13 | 取消勾选 | 恢复白 | 恢复 `#FFFFFF` | PASS | `live/20_…` |
| L-14 | 点击图形验证码 | 字符刷新 | 验证码区变化（差异 264 采样点）；dev2 刷新后为 Q8K2 | PASS | `live/21/22_…`、`dev2/08_…` |
| L-15 | 账号框输入中文「张三」 | 正常显示、不崩、无串码 | 正常显示，进程存活 | PASS | `live/23_l2_14_chinese_typed.png` |
| L-16 | 粘贴中文「粘贴测试中文」 | 正常粘贴、不崩 | 正常，进程存活 | PASS | `live/24_l2_15_chinese_pasted.png`、`dev2/11_chinese_paste.png` |
| L-17 | 中文 IME 组字/上屏 | 组字与上屏正常、不串码 | 采信 dev2 实测（含组字中/上屏/提交后错误提示全链路） | PASS* | `dev2/12_ime_composing.png`、`dev2/13_ime_committed.png`、`dev2/18~21_…` |
| L-18 | 登录会话结束 | 正常退出 | exit=0 | PASS | 控制台 |
| L-19 | 全交互期 stderr | 无异常 | stderr 空 | PASS | 控制台 |
| L-20 | 登录链路契约 | 渲染层只入队、AuthSystem 消费、数据服务调用只在应用层 | 代码审读符合；selftest `commandQueueTimingOk=true`（帧末入队→下一帧 Simulation 消费） | PASS** | `src/pages/LoginModule.cpp:490-596`、`st/metrics.json` |

### D. 应用外壳（21）

| 编号 | 操作 | 期望 | 实测 | 结果 | 证据 |
|---|---|---|---|---|---|
| S-01 | 量侧栏宽 | 200 | 实测 200（像素扫描） | PASS | `live/25_l3_01_dashboard.png`、`st/metrics.json` |
| S-02 | 一级菜单数 | 10 个 | 首页/中州智询/来访管理/入退管理/在住管理/服务管理/智能监测/系统管理/系统监控/系统工具 | PASS | 截图 + `MenuCatalog.cpp` |
| S-03 | 首页选中态 | 文字/图标 #00B8A0 高亮 | 首页图标区青绿像素 60 | PASS | 截图 |
| S-04 | 顶栏/标签栏几何 | 顶栏 50、标签栏 34 | metrics：topbar 1080×50@(200,0)、tags 1080×34 | PASS | `st/metrics.json` |
| S-05 | 点汉堡键 | 侧栏折叠 54 | 实测 54 | PASS | `live/27_l3_03_sidebar_collapsed.png`、`dev1/03_…` |
| S-06 | 折叠态悬停图标 | 中文 tooltip | 出现 tooltip（差异 129 采样点） | PASS | `live/28_l3_04_collapsed_tooltip.png`、`dev1/04_…` |
| S-07 | 折叠态点汉堡键 | 恢复 200 | 实测 200 | PASS | `live/29_…` |
| S-08 | 点「中州智询」 | 展开二级菜单 | 展开（差异 1654 采样点） | PASS | `live/30_l3_06_submenu_expanded.png` |
| S-09 | 展开后点「来访管理」 | 二级互斥（unique-opened） | 中州智询收起、来访管理展开（差异 1708） | PASS | `live/31_l3_07_submenu_exclusive.png` |
| S-10 | 展开「系统管理」（10 子项）后滚轮 | 超高菜单可滚动 | 滚动前后差异 1526 采样点，出现细滚动条 | PASS | `live/32/33_…`、`dev1/07_content_scroll_1024x640.png` |
| S-11 | 悬停未实现子项「用户管理」 | 「规划中」提示 | tooltip「「用户管理」模块规划中，Sprint 2 接入」 | PASS | `live/34_l3_10_planning_tooltip.png`、`dev1/02_…` |
| S-12 | 点击「首页」 | 保持看板不崩 | 路由 `/index` 可路由 → 入队 Navigate，看板保持（代码审读+操作无异常） | PASS** | `ShellModule.cpp:628-633`、`live/35_…` |
| S-13 | 点右上用户区 | 弹出「个人中心（规划中）」 | 弹窗出现（与初始帧差异 108） | PASS | `live/26_l3_02_user_popup.png`、`dev1/05_…` |
| S-14 | 顶栏面包屑 | 显示「首页」 | 显示「首页」 | PASS | 全部看板截图 |
| S-15 | 标签栏 | 首页固定标签（#00B8A0 白字+白点） | 符合 | PASS | 全部看板截图 |
| S-16 | 右上管理员区 | 头像「管」+ 昵称「管理员」 | 符合 | PASS | 全部看板截图 |
| S-17 | 折叠/展开多轮 | 布局不崩、宽度正确往返 | 200↔54 往返正常 | PASS | `live/27~29`、`dev1/06_…` |
| S-18 | 菜单滚动后悬停/点击 | 命中正确、无错位 | 滚动回顶后 tooltip 命中正确 | PASS | `live/33/34` |
| S-19 | 外壳操作后进程 | 存活 | alive=true | PASS | 控制台 |
| S-20 | 外壳会话结束 | 正常退出 | exit=0 | PASS | 控制台 |
| S-21 | 全交互期 stderr | 无异常 | stderr 空 | PASS | 控制台 |

### E. 首页看板（9）

| 编号 | 检查点 | 期望 | 实测 | 结果 | 证据 |
|---|---|---|---|---|---|
| D-01 | 5 个环形统计卡 | 655人 / 812个 / 100.35w笔 / 204人 / 410.81w元 + 分段副文字 | 全部一致（B 套） | PASS | `st/02_dashboard_1280x720.png`、`live/25_…` |
| D-02 | 收益双折线 | 收入(#4D8DFF)/退款(#00B8A0) 双线 + 今日/本周/本月选中态 + 日期区间 + 轴/刻度/图例 | 本周选中（#00B8A0 下划线+加粗）、区间 2026-11-23 至 2026-11-29、y 轴 0w/0.5w/1w/1.5w、x 轴周一~周日、图例齐全 | PASS | `st/02_…` |
| D-03 | 入退分组柱 | 入住/退住双柱 + 轴/刻度/图例 | 符合（24 时段，0~4 刻度） | PASS | `st/02_…` |
| D-04 | 服务折线 | 单折线 + 轴/刻度/图例 | 符合（0~200 刻度，0:00~23:00） | PASS | `st/02_…` |
| D-05 | 年龄堆叠柱 | 男/女堆叠 + 5 年龄段 + 图例 | 符合（60岁以下…90岁以上） | PASS | `st/02_…` |
| D-06 | 老人等级排行 | 4 行：331/191/91/42，占比 50.5/29.2/13.9/6.4% | 全部一致（B 套） | PASS | `st/02_…`、`live/25_…` |
| D-07 | 老人等级分布环形 | 中心 655 人 + 4 段图例 | 符合 | PASS | `st/02_…` |
| D-08 | 与 Web 数据等价性 | 与 `views/dashboard/constants.js` B 套一致 | 逐项核对一致：统计 655/812/100.35/204/410.81；等级 42/91/191/331；年龄 男[19,54,128,98,8]/女[25,40,164,102,17]；本周收益 `WEEK_NUM_B` income/refund 7 点完全一致 | PASS | `MockServices.cpp:90-151` vs Web `constants.js:193-196` |
| D-09 | 图表随窗口重排 | 1280/1600/1920/1024 均重算不糊 | 四档截图均正常，无裁切/重叠 | PASS | `st/02,04`、`live/39,40`、`live/36~38` |

### F. 响应式（8）

| 编号 | 操作 | 期望 | 实测 | 结果 | 证据 |
|---|---|---|---|---|---|
| R-01 | 1280×720 | 一屏无溢出 | 三行卡片完整可见、无滚动条 | PASS | `live/36_l4_01_1280x720.png`、`st/02_…` |
| R-02 | 1024×640 | 单列 + 内容可滚 | 单列堆叠、右侧出现 6px 细滚动条 | PASS | `live/37_l4_02_1024x640.png`、`st/03_…` |
| R-03 | 1024 滚轮下滚 | 内容滚动 | 滚动前后差异 3504 采样点 | PASS | `live/38_l4_03_1024_scrolled.png` |
| R-04 | 滚动时顶栏/侧栏 | 固定不动 | 顶栏 #FFFFFF/#FFFFFF、侧栏 #304156/#304156 | PASS | 像素对比 |
| R-05 | 1600×900 | 两栏+三栏正常 | 符合 | PASS | `live/39_l4_04_1600x900.png` |
| R-06 | F11 全屏 1920×1080 | 布局正常 | 客户区 1920×1080，三栏等宽、无重叠 | PASS | `live/40_l4_05_fullscreen_1920x1080.png`、`st/04_fullscreen.png`（glReadPixels 确定性帧） |
| R-07 | 响应式会话结束 | exit=0 | 通过 | PASS | 控制台 |
| R-08 | 缩放期 stderr | 无异常 | 空 | PASS | 控制台 |

### G. 稳定性（4）

| 编号 | 操作 | 期望 | 实测 | 结果 | 证据 |
|---|---|---|---|---|---|
| ST-01 | 连续操作 2 分钟（点击 10 个一级菜单、hover、滚轮、F1 开关、1024~1424 反复缩放、折叠切换，约 1308 次操作） | 不崩溃 | 进程存活，未崩溃 | PASS | `live/57_l5_01_after_2min_1424x770.png` |
| ST-02 | 压力后关窗 | exit=0 | exit=0 | PASS | 控制台 |
| ST-03 | 查 stderr | 无异常 | 空 | PASS | 控制台 |
| ST-04 | 查 ImGui 断言关键字 | 无 `Assert/Missing PopFont` | 出现次数 0 | PASS | 控制台 |

### H. 架构约束（6，代码审读）

| 编号 | 检查点 | 结论 | 结果 | 证据 |
|---|---|---|---|---|
| AR-01 | 渲染/数据分离：页面与外壳不得 include 数据实现头、不得做 IO | `pages/*.cpp`、`shell/*.cpp` 仅 include 抽象头（`data/Services.h` 仅被同文件的 AuthSystem 使用），无 `MockServices/MockAuth/sqlite/mysql/fopen/ifstream` 实际引用（仅注释文字） | PASS | grep 全量结果 |
| AR-02 | 渲染系统只读业务资源 | `RouteState/AuthSession/AuthFeedback/DashboardData` 在渲染系统内均为 `const` 访问；可变写入仅存在于 `AuthSystem`（Simulation，架构 §3.3 允许）与 `RouteSystem` | PASS | `LoginModule.cpp:397/421/544/588/592`、`ShellModule.cpp:271/667` |
| AR-03 | 交互只入队 CommandQueue | ShellModule:126、LoginModule:500 仅 `push_back`；消费在 AuthSystem/RouteSystem；App 帧末只丢弃已消费批次（时序修复），selftest 回归 `commandQueueTimingOk=true` | PASS | 源码 + `st/metrics.json` |
| AR-04 | 页面无绝对坐标 | 抽检 ≥10 处：卡片/图表/表头/环形/排行坐标全部由 `card/plot/area/header` 矩形 + 比例/Clamp 推导（如 `style.body=Rect{card.x+padding, card.y+headerHeight,…}`、`point.x` 由 3 次贝塞尔插值、柱 x 由 `plot.x+stepX*index`）；全库无 `ImVec2(≥3位数字)`/`Rect{≥3位数字}` 硬编码 | PASS | `DashboardModule.cpp:264-290/470-509/546/596/633/651-686` 等 |
| AR-05 | 接口契约符合 §13 | `World/ISystem/SystemPhase/SystemScheduler/ShellLayout/LayoutTokens/IAuthService/IDashboardService/AppState` 结构与签名与架构文档第 13 章一致；metrics 显示 17 实体/4 组件类型/13 资源/8 系统 | PASS | `include/zzyl/**`、`st/metrics.json` |
| AR-06 | 数据层可替换性 | 页面只依赖 `DashboardData` 资源类型；`DataServices` 抽象（Mock 实现），Sprint 2 换 MySQL/SQLite 时页面零改动 | PASS | `Services.h`、`MockServices.cpp` |

---

## 3. 缺陷清单

> 本次无 P0/P1 阻塞缺陷。以下 P2 为口径/遗留/非阻塞项，均不影响 Sprint 1 验收。

| 编号 | 级别 | 现象 | 复现步骤 | 归属 | 建议 |
|---|---|---|---|---|---|
| D-01 | P2 | 未实现模块**点击无反馈**，仅在**悬停**时显示 tooltip「「X」模块规划中，Sprint 2 接入」；验收清单字面为「点击有规划中提示」 | 登录→展开任一未实现二级菜单→点击该项 | dev1 | Sprint 2 增加点击 toast/提示；或 lead 确认"悬停 tooltip 即达标"（当前口径已记录） |
| D-02 | P2 | 「记住密码」仅内存态，重启应用不记忆（Mock 阶段无持久化） | 勾选记住密码→关窗→重开→仍为未勾选 | dev2 | Sprint 2 接 SQLite 会话/配置持久化后实现；本次验收仅要求"可勾选"，不阻塞 |
| D-03 | P2 | 最小窗口 1024×640 生效，但在 1024×600 屏幕上窗口高度会超出屏幕（Web 规格约束） | 在 1024×600 分辨率上启动 | arch（规格）/环境 | 记录为已知环境限制；如需支持小屏，Sprint 2 可加 `--width/--height` 或按屏幕高度安全区下限 |
| D-04 | P2 | 窗口标题口径不一致：任务书为「中州养老 **·** C++ 转译端」，实现与验收清单 §7 为「中州养老 **-** C++ 转译端」 | 读窗口标题 | lead/arch（口径） | 确认统一口径；实现已与 `docs/tasks/Sprint1_总览` §7 及 `version.h` 一致，倾向保留连字符 |
| D-05 | P2 | 顶栏右侧新增桌面端提示文案「F11 全屏 · F1 调试」，Web 规格未列出；属桌面端增强 | 观察顶栏 | dev1 | 可接受，建议记入《与 Web 端差异项》 |
| D-06 | P2 | 菜单图标/登录输入框图标为几何占位、登录背景为渐变占位（无图片解码能力），与 Web 图标/背景图有差异 | 观察侧栏图标、登录背景 | arch（已知限制 D-06） | Sprint 2 用 WIC 解码 Web 现有 png/jpg 后替换 |
| N-01 | 非缺陷 | 验证码为占位图形，任意非空即通过 | — | arch（§1.4 非目标） | Sprint 2 接真实验证码接口 |

---

## 4. 覆盖度声明与自动化局限

**本次亲自执行（真机）**：构建可复现、窗口尺寸/标题/缩放/最小尺寸/F11/ESC/F1、登录页空值/短密码/错误口令校验与红字、验证码刷新、记住密码勾选、中文输入与粘贴、外壳侧栏 200/54、折叠展开、二级菜单展开与互斥、菜单滚动、规划中 tooltip、用户弹窗、1280/1024/1600/1920 四档响应式与 1024 内容滚动、2 分钟连续操作稳定性、退出码与 stderr 检查。

**确定性自测（glReadPixels 抓帧，最可靠）**：`--selftest` 4 张帧 + `metrics.json`（窗口/帧缓冲/DPI/中文字形/布局矩形/ECS 统计/命令队列时序），用于替代不可靠的全屏抓屏；1280×720 与 1600×900 各跑一轮。

**代码审读**：渲染/数据分离、CommandQueue 时序、页面绝对坐标抽检 ≥10 处、接口契约与数据层可替换性、MenuCatalog 10 菜单。

**采信开发者实测 + tester 复核截图（3 条，PASS*）**：密码框空值红框（L-07）、点击登录进看板（L-10）、中文 IME 组字上屏（L-17）——来源 dev2 `artifacts/verify_dev2/realfinal/`、dev1 `artifacts/shell_qa_dev1/`、dev3 `artifacts/selftest_dev3/`。原因：tester 的 SendInput 模拟在**连续错误态之后切换到密码框**时偶发不被 ImGui 激活（harness 局限，非产品缺陷；已用同一脚本在干净态复现成功），按 lead 指示不判 FAIL。

**未覆盖/未验证**：真实 MySQL/SQLite 数据链路（本迭代非目标）、真实验证码接口、高 DPI（125%/150%）显示、8h 级长时间挂机、真实鼠标拖拽改变窗口大小（仅用 SetWindowPos 程序化缩放）。

---

## 5. 遗留风险

1. **全屏抓屏通道限制**：`CopyFromScreen` 依赖窗口前台且无遮挡，全屏帧已用 `--selftest` 的 `glReadPixels` 帧交叉验证（`st/04_fullscreen.png`），结论不受影响。
2. **1024×600 屏幕钳制**：最小 1024×640 在小屏上窗口超高（D-03）。
3. **Remember 未持久化**（D-02）。
4. **数据为 Mock B 套**：与 Web `constants.js` B 套逐项一致，但非真实后端；Sprint 2 接 MySQL/SQLite 后需重跑数据一致性回归。
5. **图标/背景占位**（D-06）：视觉还原度未达 Web 100%，Sprint 2 图片接入后需补视觉回归。
6. **测试自动化局限**：模拟输入在焦点切换场景偶发不稳定，后续建议在 Sprint 2 增加 `--selftest` 脚本化场景（如 `--script login_fail`）以提升可回归性（需架构师加测试钩子，非本迭代）。

---

## 6. 证据索引（关键路径）

| 类别 | 路径 |
|---|---|
| 确定性自测帧 + metrics | `artifacts/test_tester/selftest/`（01 登录、02 看板 1280、03 看板 1024、04 全屏、metrics.json） |
| 确定性自测（1600×900 启动） | `artifacts/test_tester/selftest_1600x900/` |
| 真机自动化截图（57 张） | `artifacts/test_tester/live/`（`01~57_*.png`） |
| 真机自动化断言明细 | `artifacts/test_tester/live/results_live.json`、`results.tsv` |
| 独立干净构建 | `artifacts/test_tester/build_clean/` |
| 测试脚本（只读被测程序） | `artifacts/test_tester/tools/testkit.ps1`、`run_live.ps1` |
| 开发者证据（采信/复核） | `artifacts/verify_dev2/realfinal/`、`artifacts/shell_qa_dev1/`、`artifacts/selftest_dev3/` |

---

*报告结束。测试期间未修改任何源码、未执行 git 提交。*
