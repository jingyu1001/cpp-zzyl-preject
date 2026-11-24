# Sprint 4 集成验证包（M5/M6/M7 + M4 数据链/上传 + SQLite v5）

> 打包：cpp-arch｜构建：Release **2026-11-24 22:27**（含 FileDialog 编码修复/应用内 picker、v5 数据层、M5/M6/M7 页面）｜基线：架构 v0.4、Sprint4 功能规格 v0.1、Sprint4 总览
> 用途：交 **cpp-tester** 正式测试的入口材料（构建/帧/数据层/平台/开发者证据/bug 归属/验收差异）
> 当前状态：**无 P0/P1 已知缺陷**；BUG-S4-01（P2）命名链已修复待实机重启复核；BUG-S4-02（P2）M6 校验红字已派 dev2 修复；M7 CRUD 按 lead 裁定 P1（本轮交付可视化 P0）。

## 1. 构建与环境
- 工程：`C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject`；Release：`build\Release\zzyl_desktop.exe`（22:27）；依赖：VS2026 v145 + CMake 4.3 生成器 "Visual Studio 18 2026"、glfw/imgui/glad、SQLite 3.54.1 amalgamation 静态库、Win32 文件上传（应用内浏览器）。
- 构建：`cmake -S . -B build -G "Visual Studio 18 2026" -A x64 && cmake --build build --config Release --parallel`
- DB：默认 `<exe>/data/zzyl.db`；`--db` / `ZZYL_DB_PATH` 覆盖；`--empty-data` 空态；schema **v5**（v4 旧库自动迁移）；干净退出 `PRAGMA wal_checkpoint(TRUNCATE)`。

## 2. 架构集成帧（21 帧 · `artifacts/selftest_s4_final/`，副本 `artifacts/sprint4_integration/frames_arch/`）
- 运行：`zzyl_desktop.exe --selftest artifacts\selftest_s4_final`（CI 建议 `Start-Process` 重定向；勿与前台窗口并发）
- 结果：exit=0、**21 帧**、`glyphsOk/commandQueueTimingOk/wicTextureOk=true`、`stderr_0_warnings.log` **0B**
- 数据日志：`用户=7 房型=6 护理项目=9 入住办理=5 护理计划=3 护理等级=3 楼层=11 房间=60 床位=87`（床位表 89 行，2 orphan room_id=74 过滤；lead 已认可）

| 帧 | 页面 | 说明 |
|---|---|---|
| 01~08 | Sprint1~3 回归 | 登录/看板/1024/全屏/用户/房型(真实图)/护理项目/WIC |
| 09~17 | M4 列表/表单/详情 ×1280/1024/1920 | 级联/上传已接（21 帧同 10 的画面属已知 EV-S4-01；cascade 钩子帧见 `dev_evidence/21_cascade_dev2_hook.png`） |
| **18** | **M5 护理计划** | 搜索 + 5 列表格（3 条种子）+ 分页；被引用行仅查看/禁用 |
| **19** | **M6 护理等级** | 8 列表格（1500/2000/3000）+ 分页 + 操作 |
| **20** | **M7 床位预览** | 11 楼层 Tab/图例 3 态/房间卡/床位卡（截图见 `frames_arch/20…`，QA `dev_evidence/m7v_*`） |
| 21 | M4 表单级联 | 与 10 同画面（EV-S4-01）；dev2 钩子帧 `dev_evidence/21_cascade_dev2_hook.png` 展示三下拉+床位费 |

## 3. 数据层 v5 / 平台 / 开发者证据
| 项 | 结果 | 证据 |
|---|---|---|
| v5 迁移/种子/幂等 | ✅ 默认库/`--db`/`ZZYL_DB_PATH`/`--empty-data`；二次启动 7/6/9/5/3/3/11/60/87；stderr 0 告警 | dev3 `artifacts/_verify_dev3/s3_logs`（Sprint4 版本见其报告）、本包 `stdout_data_v5.log` |
| WAL | ✅ 干净退出主库 77,824B、无 `-wal`/`-shm`（tester O-S4-01 复测通过） | v0.4 §12 |
| 平台文件选择器 | ✅ 应用内浏览器（无 Shell/COM）；中文名 `单人套房.png` 正确；取消/超限/二次运行稳定 | 本包 `platform/`；v0.4 §3.2/§12 |
| M5 真实窗口 | ✅ 列表 3 条→新增弹窗（1000 宽）→9 项目下拉→保存 4 条（未被引用可改） | dev2 `artifacts/verify_dev2/s4/w31~w34` |
| M6/M7 实机 | ✅ M6 8 列/弹窗 500；M7 楼层/图例/房间卡/床位三态 + P1 Toast；BUG-S4-02（M6 校验红字）修复中 | tester `artifacts/test_tester_s4/liveA/`、dev1 `s4_m7_qa/`（本包 `dev_evidence/`） |
| M4 级联/上传影子 | ✅ 三下拉联动+床位费；3 图 100×100 预览、PDF 文件名+查看 | dev2 `artifacts/verify_dev2/s4_shadow/` |

## 4. 已知缺陷/差异
| 编号 | 级别 | 内容 | 状态/归属 |
|---|---|---|---|
| BUG-S4-01 | P2 | 上传文件名中文双重编码 | ✅ **已修复并 tester 全链路复测 PASS**（中文图→预览→保存 7 条→重启仍在；产物中文名正确） |
| BUG-S4-02 | P2 | M6 空提交校验红字悬浮标题下/字段无提示 | ✅ **已修复并 tester 独立复测 PASS**（M6 4 字段就地红字；M5 名称红字；footer 可见可点）；共享 Release 22:31 |
| 观察 footer | P3 | 校验后影子截图 footer 缺失 | 测试截图当帧时序伪影（真实窗口 0/300/900ms 三帧正常、可点）；非缺陷不派单 |
| M7 CRUD | P1 接受差异 | 本轮按钮保留外观+点击 Toast「后续迭代接入」，写接口 Sprint5 | lead 已裁定；按此判 PASS |
| O-S4-01 | — | WAL 未 checkpoint | ✅ 已修（Close checkpoint），tester 复测通过 |
| O-S4-02 | — | selftest 偶发（并发） | ✅ 按环境/测试方式限制关闭；CI 用重定向 stdout/stderr、勿与可见窗口并发 |
| 数据口径 | — | 9/10 楼无房、floor.code 重复、15 间房型不匹配 room_type、2 orphan 床位 | 种子保留 Web 真值；M7 有效 60 房/87 床；房型显示原值、price=0；已接受 |

## 5. Bug 归属映射
| 范围 | 负责人 |
|---|---|
| M7 床位预览 / 外壳路由 | cpp-dev1 |
| M5 护理计划 / M6 护理等级 / M4 表单级联+上传 UI | cpp-dev2 |
| 房型本地上传 / CSV / SqliteData 收尾 | cpp-dev3 |
| 数据契约/v5 迁移/种子/路由/FileDialog/CMake/资源/文档 | cpp-arch |
| 测试报告/测试产物 | cpp-tester |

## 6. 窗口段2 待测清单（DQ-0031）
- M4 上传：中文图→100×100 预览→保存→**重启后仍在**（BUG-S4-01 闭环判据）；PDF 文件名+查看；取消/Esc；
- M4 级联：三下拉联动+床位费（`AvailableBeds` 只列空闲床）；
- M6 校验复测（BUG-S4-02）+ M5 逐项校验对比；
- M7：11 Tab/三态图例/房间卡/床位卡/点击详情/1024/1920；P1 按钮点击 Toast「后续迭代接入」；
- `--empty-data` 21 帧空态、隔离库持久化、回归 Sprint1~3。

## 7. 测试纪律
- 可见窗口/模拟输入/前台抓屏先向 **cpp-control** 申请（FIFO，结束释放）；隐藏 selftest 用 `Start-Process` 重定向，且不与前台窗口并发；
- 报告 `docs/test/Sprint4_测试报告.md`；P0/P1 立即报 lead+架构；P2 按 §5 派单复测。
