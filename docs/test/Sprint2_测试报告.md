# 中州养老 · C++ 转译端 —— Sprint 2 测试报告（v3 · 终版）

> 被测：Sprint 2 集成版（M0 通用组件 + M1 用户管理 + M2 房型设置 + M3 护理项目 + 外壳路由）
> 双构建口径：① lead 共享 `build\Release\zzyl_desktop.exe`（2026-11-24 13:33，首轮基线）；② **BUG-S2-01 修复版**：dev1 `src/ui/Widgets.cpp` 13:39 修复后，tester 独立目录 `artifacts\test_tester_s2\build_dev1fix` 全新构建（13:44:57，1,057,280 B），用于终版复测
> 测试者：cpp-tester｜首轮 13:35–14:15、补充覆盖 14:18–14:55、修复复测 13:52–14:10｜依据：架构 v0.2、Sprint2 总览 §8、Web UI 规格 v0.2
> 证据根：`artifacts\test_tester_s2\`（`selftest\` 修复前 8 帧、`selftest_fix\` 修复后 8 帧、`fix_retest\` BUG 复测、`m1\` 校验、`live*\` 交互、`toast\`）

## 0. 结论

| 项 | 结果 |
|---|---|
| 用例/检查点 | **61** |
| PASS | **60** |
| FAIL | **0**（BUG-S2-01 已修复并复测通过） |
| 未覆盖 | **1**（M0 表格空态：种子数据非空无入口，非验收阻塞项，建议 Sprint 3 补） |
| 阻塞缺陷 P0/P1 | **0** |
| **结论** | ✅ **可交付**：Sprint 2 四块交付达到验收清单主项；唯一真缺陷 BUG-S2-01 已修复并经 tester 双档位复测通过；构建可复现、无崩溃/ImGui 断言、中文无方块。 |

## 1. 环境与构建

| 项 | 值 |
|---|---|
| 环境 | Windows 11 10.0.26300 / i5-10300H / 16GB / Intel 核显 OpenGL 3.3 / 1920×1080 @100% |
| 共享构建 | lead 13:33，exit 0，1,056,768 B；增量重建 1.2 s 通过、零错误 |
| 修复版构建 | tester 独立 `cmake -S -B artifacts\test_tester_s2\build_dev1fix -G "VS 18 2026"` + Release，构建通过、`复制 assets/` 正常；`--selftest` 8 图 + 三标记位 true、退出码 0 |
| 确定性自测 | 修复前 `selftest\`、修复后 `selftest_fix\`：均 `glyphsOk/commandQueueTimingOk/wicTextureOk = true`、12 系统/17 实体/18 资源 |
| Git | 未 commit；tester 未改源码 |

## 2. 用例总表（v3 · 61 条）

> `PASS`=tester 亲测/复核；`PASS*`=采信开发者实测证据 + tester 抽检/代码核对；`未覆盖`=无入口/无证据（非阻塞）。

### A. 回归（8）

| 编号 | 检查点 | 结果 | 证据 |
|---|---|---|---|
| R-01 登录页 | 与 S1 一致、中文无方块 | PASS | `selftest\01_login…`、`selftest_fix\01…` |
| R-02 看板 5 环/4 图/排行 B 套 | 655/812/100.35w/204/410.81w；331/191/91/42 | PASS | `selftest\02_dashboard…` |
| R-03 1280×720 外壳几何 | sidebar 200×720 / topbar 1080×50 / pageViewport 1041×560@(224,108) | PASS | `selftest\metrics.json` |
| R-04 1024×640 | 缩放成功、重排正常 | PASS | `selftest\03…`、`selftest_fix\03…` |
| R-05 F11 全屏 1920×1080 | 全屏帧正常 | PASS | `selftest\04_fullscreen…` |
| R-06 中文渲染 | glyphsOk=true、8 图无方块 | PASS | metrics + 8 图 |
| R-07 无崩溃/断言 | 多会话 stderr 空、退出码 0 | PASS | `fix_retest\*.log` |
| R-08 命令队列时序 | commandQueueTimingOk=true | PASS | metrics |

### B. 外壳路由（8）

| 编号 | 检查点 | 结果 | 证据 |
|---|---|---|---|
| S-01 用户管理可达（自动展开/高亮/面包屑/标签） | PASS | `selftest\05…`、`live2\04…` |
| S-02 房型设置可达 | PASS | `selftest\06…` |
| S-03 护理项目可达 | PASS | `selftest\07…`、`selftest_fix\07…` |
| S-04 面包屑三级 | PASS | `selftest\05/06/07` |
| S-05 双标签 + 回首页 | PASS | `live2\07…` |
| S-06 不可达菜单 Toast（可叠加） | PASS | `live2\03/09…` |
| S-07 菜单展开自动滚入（D-29 接受） | PASS | `live\02…` |
| S-08 F11/ESC（多 agent 抢前台时键盘偶发失效→环境；以确定性帧为准） | PASS*（局限） | `selftest\04…` |

### C. M0 通用组件（11）

| 编号 | 检查点 | 结果 | 证据 |
|---|---|---|---|
| M0-01 搜索栏 5 项（用户）/2 项（护理） | PASS | `selftest\05/07` |
| M0-02 工具栏 5 类按钮 + 禁用态 | PASS | `selftest\05` |
| M0-03 表格 9 列/行高≈49/无斑马纹/hover | PASS | `selftest\05`、`live2\05…` |
| M0-04 分页（7/6/9 条、10-50/页、上下页、跳页、贴底） | PASS | `live2\05…`、`selftest\06/07`、`fix_retest\02/05…` |
| M0-05 横向滚动（含 Shift+滚轮/拖拽/点击轨道） | PASS（修复后） | `fix_retest\03/06…`、`bug_s2_01_final\02…` |
| M0-06 图片列 WIC 60×60（12 张素材） | PASS | `selftest\07…` |
| M0-07 StatusTag / Switch | PASS | `selftest\05/06/07` |
| M0-08 Actions 逐行文案（4/3 按钮） | PASS | `selftest\05/06` |
| M0-09 表单弹窗：600 宽双列、遮罩（#FFFFFF→#919191）、标题×、取消/确定、空提交不关+红字 | PASS | `live3\02_c02…`、`m1\01_d01_empty_validation…` |
| M0-10 Toast：顶部居中、3 秒自动消失（4.2s 后差异 287）、可叠加、最上层 | PASS | `toast\01/02…`、`live2\09…` |
| M0-11 表格空态 | 未覆盖（种子非空无入口） | — |

### D. M1 用户管理（17）

| 编号 | 检查点 | 结果 | 证据 |
|---|---|---|---|
| M1-01 7 条种子 + 9 列 | PASS | `live2\05…` |
| M1-02 搜索 5 项 + 重置 | PASS | `selftest\05` |
| M1-03 分页共 7 条（下滚可见） | PASS | `live2\05…` |
| M1-04 必填校验（空提交：弹窗不关 + 三处红字） | PASS（tester 实测） | `m1\01_d01_empty_validation…` |
| M1-05 组合校验同屏 4 条红字（名称 2~20/密码 5~20/手机 11 位/邮箱格式） | PASS* | `verify_dev2\s2cov\c01_multi_validation.png` |
| M1-06 密码非法字符（`12<45`）→「不能包含 < > " ' \ | 字符」 | PASS* | `verify_dev2\s2cov\c02_password_illegal.png` |
| M1-07 修改回填 + **隐藏用户名/密码字段** | PASS* | `verify_dev2\s2cov\c03_edit_dialog.png` |
| M1-08 重复用户名 →「用户名称已存在」 | PASS*（代码 `SeedServices.cpp:78-79` + dev2 实测） | 代码行号 + dev2 证据 |
| M1-09 删除确认 + 删除后列表刷新 | PASS* | `verify_dev2\s2auto\a06/a07…` |
| M1-10 重置密码 + 短密码校验（5~20） | PASS*（代码 `SystemUserModule.cpp:960` + dev2 实测 `a08/a09/a10`） | dev2 证据 |
| M1-11 状态开关 + Toast + 刷新 | PASS* | `verify_dev2\s2auto\a05_switch…` |
| M1-12 超管删除保护 | PASS*：确认后 Toast「不允许删除超级管理员」、admin 仍在列表 | `verify_dev2\s2cov\c04/c05…`、`SeedServices.cpp:107` |
| M1-13 超管停用保护 | PASS*：点开关 → Toast「不允许停用超级管理员」、开关保持开 | `verify_dev2\s2cov\c06…`、`SeedServices.cpp:124` |
| M1-14 操作列 4 文字按钮（宽 200，D-26 已批准差异） | PASS | `selftest\05` |
| M1-15 1024×640 页面（内部横向滚动，符合 UI v0.2 §3.3） | PASS* | `verify_dev2\s2cov04\g01_page_1024x640.png` |
| M1-16 1920×1080 页面（9 列 + 4 操作按钮 + 分页「共 7 条」贴底） | PASS*（tester 抽检复核） | `verify_dev2\s2cov04\g02_page_1920x1080.png` |
| M1-17 弹窗 1280/1024/1920（居中不超屏） | PASS* | `verify_dev2\s2cov04\g03/g04/g05…` |

### E. M2 房型设置（7）

| 编号 | 检查点 | 结果 | 证据 |
|---|---|---|---|
| M2-01 6 条 + 8 列 | PASS | `selftest\06…` |
| M2-02 图片占位「无图」（D-23 已批准） | PASS | `selftest\06` |
| M2-03 Switch + 逐行修改/删除/禁用 | PASS | `selftest\06` |
| M2-04 分页共 6 条 | PASS | `selftest\06` |
| M2-05 单列弹窗 6 字段 + 148×148 + 必填校验 | PASS*（代码 `RoomTypeModule.cpp:256-269` + dev3 实测） | `_verify_dev3\13/14/15…` |
| M2-06 保存/删除/启停后刷新 + Toast | PASS* | `_verify_dev3\70/71/73…` |
| M2-07 1024×640 / 1920 页面适配 | PASS*（dev3 `05_live_1024x640.png` + 表格自适应 + 确定性全屏帧；1920 逐像素建议 Sprint3 补） | dev3 证据、`selftest_fix\04_fullscreen…` |

### F. M3 护理项目（8）

| 编号 | 检查点 | 结果 | 证据 |
|---|---|---|---|
| M3-01 9 条 + 10 列 + 横向滚动 | PASS（修复后） | `fix_retest\01/02/03…`、`selftest_fix\07/08…` |
| M3-02 图片 60×60 WIC（12 素材） | PASS | `selftest_fix\07…` |
| M3-03 搜索（名称/状态） | PASS | `selftest_fix\07…` |
| M3-04 分页共 9 条（1280/1024 均可见） | PASS | `fix_retest\02/05…` |
| M3-05 弹窗 7 字段 + 148×148 预览 + 12 素材点击切换 | PASS*（代码 `NursingProjectModule.cpp:46/446/478` + dev3 实测） | `_verify_dev3\35/36/68…` |
| M3-06 删除确认/启停/必填校验 | PASS*（代码 `NursingProjectModule.cpp:373/290/300` + dev3 实测） | `_verify_dev3\62/65/67…` |
| M3-07 1024×640 页面（滚动条不遮行、分页可见） | PASS（tester 实测） | `fix_retest\05…` |
| M3-08 1920×1080 页面 | PASS*（采信确定性全屏帧 + 表格自适应；逐像素建议 Sprint3 补） | `selftest_fix\04_fullscreen…` |

### G. 架构/数据（2）

| 编号 | 检查点 | 结果 | 证据 |
|---|---|---|---|
| AR-01 路由 5 映射 + 不可达判断 | PASS | `src\app\Routes.cpp` |
| AR-02 种子数=页面行数（7/6/9）、图片 assets 复制 + WIC 加载 | PASS | `SeedServices.cpp`、构建日志、`wicTextureOk=true` |

## 3. BUG-S2-01 修复验证（真缺陷闭环）

| 断言 | 1280×720 | 1024×640 | 结论 |
|---|---|---|---|
| ① 横向滚动条不遮最后一行 | ✅ 滚动条在表格底部独立条带（6px 留白），行 4~9 完整可读 | ✅ 同左，滚动条独立条带 | **PASS** |
| ② 分页区可见可点（共 9 条/页码/前往） | ✅ 分页完整显示、未被遮挡 | ✅ 分页完整显示 | **PASS** |
| ③ 横向滚动可用（拖拽 → 操作列） | ✅ 拖到最右后「操作」列（修改/删除/禁用）可见 | ✅ 同左 | **PASS** |
| ④ 末行完整可读 | ✅ 第 9 行「心理咨询 80.00 启用」完整 | ✅ 第 9 行完整 | **PASS** |

- 复测证据：`artifacts\test_tester_s2\fix_retest\f01~f07_*.png`（tester 实测，修复版独立构建）、`artifacts\test_tester_s2\selftest_fix\07/08_nursing_project_1280x720.png`（修复后确定性帧）；
- 开发者证据（对照）：`artifacts\bug_s2_01_final\01~07_*.png`（滚动条修复后 1280/1024、横向拖拽、无横向溢出的用户/房型回归）；
- 回归：用户管理/房型设置 1280 无横向溢出（不出现滚动条）、纵向滚动/勾选/开关/操作列/分页均正常（`selftest_fix\05/06` + `fix_retest`）。

## 4. 缺陷与差异项

### 4.1 缺陷

| 编号 | 页面 | 级别 | 现象 | 状态 |
|---|---|---|---|---|
| BUG-S2-01 | 护理项目（表格横向滚动条压末行） | P2 | 滚动条与最后一行内容重叠 | **已修复 + tester 双档复测通过（§3）** |

### 4.2 已接受差异 / 非缺陷（lead/arch 口径）

| 编号 | 项 | 口径 |
|---|---|---|
| D-23 | 房型图片列「无图」占位 | 已批准差异（本轮无房型图片素材） |
| D-26 | 用户操作列宽 200 | 已批准差异（4 文字按钮完整显示） |
| D-28 | 用户管理整页滚动、工具区随页滚出（fixedHeader=false） | 与 Web 一致，接受不修 |
| D-29 | 展开底部菜单自动滚入可视区 | 接受现状，后续有反馈再优化 |
| — | 不可达菜单点击 Toast（类型警告） | 符合设计 |
| — | 数据库为内存种子 Mock、验证码占位 | Sprint 2 非目标，Sprint 3 接 SQLite |

## 5. 覆盖度声明

- **tester 亲测（31 项）**：双构建可复现；修复前/后各 8 张确定性帧；三页路由/面包屑/标签/高亮；不可达 Toast；菜单自动滚入；分页（7/6/9）；表格 9 列/行高/hover/无斑马纹/工具栏禁用态/WIC 图片/状态标签/Switch；Toast 3 秒消失；表单弹窗遮罩+600 宽+必填校验；**BUG-S2-01 双档位 4 断言复测**；多会话无崩溃/stderr 空。
- **采信开发者证据 + tester 抽检/代码核对（29 项，PASS*）**：dev2 `verify_dev2\s2cov\`（组合校验/非法字符/修改隐藏/C04-C06 超管保护）、`s2cov04\`（1024/1920 页面与弹窗，tester 已抽检 `g02_page_1920x1080.png`）、`s2auto\`（删除/重置密码/开关）；dev3 `_verify_dev3\`（房型/护理弹窗、校验、保存删除启停、素材切换）；dev1 `bug_s2_01_final\`（修复对照）；代码行号核对（校验规则/重名/超管/素材库）。
- **未覆盖（1）**：M0 表格空态（种子非空、无 UI 入口触发）——非验收阻塞项，建议 Sprint 3 加空态注入开关后补测。
- **证据口径修正（dev2 提醒）**：`verify_dev2\real_m0\v03_user_1920x1080.png` 实为首页看板且被遮挡，**不作为**用户页 1920 证据；本报告 1920 采用 `verify_dev2\s2cov04\g02_page_1920x1080.png`。

## 6. 遗留风险

1. 1920×1080 下房型/护理页为「确定性全屏帧 + 自适应实现 + dev3 1024 证据」推断（PASS*），建议 Sprint 3 补 1920 逐像素回归。
2. 数据为内存 Mock，持久化/刷新语义待 Sprint 3 SQLite 接入后回归。
3. 表格空态未覆盖。
4. 多 agent 共用桌面需由 `cpp-control` 串行调度（本终版复测已在授权窗口内完成）。

---
*终点声明：Sprint 2 测试闭环完成；BUG-S2-01 修复验证通过；未改源码、未 commit。报告交 cpp-arch 汇总 → lead 验收。*
