# 中州养老 · C++ 转译端 架构设计 v0.2（Sprint 2 · 列表/表单组件 + 3 业务模块）

> 增量文档：承接 `Cpp架构设计_v0.1.md`（应用框架/登录/看板，Sprint 1 已完成并通过测试）；本文件只写 **Sprint 2 增量**，未提及的部分沿用 v0.1。
> 输入：`UI复刻规格_Cpp_v0.2.md`、`Web功能清单_v0.2.md`、`assets_v0.2/`（46 张截图）、`素材/服务项目图片/`（12 张 png）
> 工程：`C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject`（Release 可构建，Sprint 1 测试通过）
> 作者：cpp-arch｜日期：2026-11-24｜状态：骨架已落地 + Release 构建/自测通过（见 §9）

---

## 1. Sprint 2 范围与增量总览

### 1.1 总控拍板范围
| 编号 | 内容 | 说明 |
|---|---|---|
| M0 | **通用组件体系**（基础，先做） | 搜索栏 / 工具栏 / 数据表格 / 分页 / 表单弹窗 / 确认框 / Toast / 状态标签 / 图片列（WIC） |
| M1 | 系统管理-用户管理 `/system/user` | 7 条种子数据、搜索、表格 9 列、新增/编辑（600×512 双列）、删除/重置密码/状态开关 |
| M2 | 在住管理-房型设置 `/liveIn/houseSet` | 6 条种子数据、图片列、单列弹窗（500×667）、启用/禁用、增删改 |
| M3 | 服务管理-护理项目 `/nursing/project` | 后端缺 Controller → 本地 9 条种子；12 张素材图标用于列表/表单 |
| 外壳 | 菜单/路由从 2 页扩到 5 页 | 登录 / 首页 / 用户管理 / 房型设置 / 护理项目；目录展开、面包屑、标签栏延续 v0.1 |
| 数据层 | **内置种子数据（不引入 SQLite）** | `DataServices` 扩展 3 个业务服务；SQLite 接入列 Sprint 3 安装事项 |
| 图片 | **WIC 解码**（系统 API，禁止第三方库） | 12 张素材复制进 `assets/icons/`（约 458KB），随仓库提交 |

### 1.2 新增代码（骨架已建，dev 填充）
```
include/zzyl/app/Routes.h               路由表（path <-> Route）                            ★架构
src/app/Routes.cpp
include/zzyl/render/Texture.h           WIC 解码 + GL 纹理 + TextureCache + 绘制助手        ★架构
src/render/Texture.cpp
include/zzyl/ui/Widgets.h               通用组件契约（搜索/工具栏/表格/分页/弹窗/确认/Toast）★架构
src/ui/Widgets.cpp                      M0 实现（cpp-dev1）                                  dev1
include/zzyl/data/BusinessServices.h    用户/房型/护理项目接口 + 记录结构                    ★架构
include/zzyl/data/SeedServices.h        Sprint 2 内存种子实现（7/6/9 条）                    ★架构
src/data/SeedServices.cpp
src/pages/SystemUserModule.cpp          M1 用户管理（cpp-dev2）                              dev2
src/pages/RoomTypeModule.cpp            M2 房型设置（cpp-dev3）                              dev3
src/pages/NursingProjectModule.cpp      M3 护理项目（cpp-dev3）                              dev3
assets/icons/*.png                      12 张服务项目素材（从 Web 素材复制）
```
依赖方向不变：`data ← app/systems ← pages/shell/ui/render`；`pages` 不直接碰数据实现（只经 `DataServices` 接口）；`ui/Widgets` 只做绘制与交互采集，不读业务数据。

### 1.3 硬约束（延续 v0.1，全部继续生效）
- 窗口 1280×720 / 最小 1024×640 / 可缩放 / F11 全屏 / 中文 / Per-Monitor V2 DPI；
- 外壳尺寸作为 `LayoutTokens` 设计约束（侧栏 200/54、顶栏 50、标签栏 34），页面内元素比例/公式布局，禁止绝对坐标；
- 渲染与数据分离：渲染系统只读资源、交互入 `CommandQueue` 或调用数据接口后刷新页面资源；
- 总体 ECS；通用组件按需抽象，不过度设计；
- 简体中文；不 commit；需装库先报 lead。

---

## 2. ECS / 资源扩展

### 2.1 路由扩展
```cpp
enum class Route { Login, Dashboard, SystemUser, RoomType, NursingProject };
struct RouteState { Route current = Route::Login; std::string path = "/login"; };
// app/Routes.h：TryRouteFromPath / RoutePath / RouteTitle / IsRoutablePath
```
- `RouteSystem`（Simulation）消费 `Navigate` 命令：`TryRouteFromPath(arg0)` 命中则切换 Route；每帧末尾把 `path` 归一化为 `RoutePath(current)`（菜单高亮/面包屑/标签栏唯一来源）。
- 未收录路径（如 `/system/role`）→ 忽略切换；外壳显示 Toast「该模块将在后续迭代接入」。
- 页面系统模式（3 个新页面统一）：一个 Render 阶段 `ISystem`，开头 `if (route->current != Route::Xxx) return;`；页面私有状态用 `World::EmplaceResource<页面State>()`；列表数据从 `DataServices` 读取后缓存在页面 State，搜索/翻页/增删改后重新拉取。

### 2.2 新增全局资源
| 资源 | 类型 | 写入者 | 读取者 | 说明 |
|---|---|---|---|---|
| `TextureCache` | `render::TextureCache` | 按需 Get（页面/组件） | 表格图片列、表单图片预览 | WIC 纹理缓存（key=相对路径） |
| `ToastState` | `ui::ToastState` | 页面系统（PushToast） | 外壳/页面每帧 DrawToasts | 顶部居中消息提示（成功/警告/错误） |

### 2.3 ECS 使用边界（不做过度设计）
- **实体/组件**继续用于：外壳菜单（NavItem）、页面锚定布局（AnchorLayout/ScreenRect）、后续可复用的可视化块。
- **列表页**（用户/房型/护理项目）数据量小（≤100 行），行数据放在页面 State 的 `std::vector`，**不逐行建 Entity**；表格刷新纯 immediate-mode 绘制（`ui::DrawDataTable`）。理由：避免"数据行=实体"带来的生命周期/同步复杂度，且无性能收益。
- **通用组件**（M0）是 immediate-mode 辅助函数（`ui::DrawSearchBar` 等），不是 ECS 系统；只有页面/外壳是系统。

---

## 3. 数据层扩展（种子数据，SQLite 留 Sprint 3）

### 3.1 接口（`include/zzyl/data/BusinessServices.h`）
```cpp
struct UserRecord { int userId; std::string userName, nickName, deptName, phone, email, sex;
                    bool enabled; std::string createTime, remark, postNames, roleNames; };
struct UserQuery { std::string keyword, phone; int status = -1; std::string deptName, beginTime, endTime; };
class IUserService {
    virtual std::vector<UserRecord> List(const UserQuery&) = 0;
    virtual bool Add(const UserRecord&, std::string* error) = 0;
    virtual bool Update(const UserRecord&, std::string* error) = 0;
    virtual bool Remove(const std::vector<int>& ids, std::string* error) = 0;
    virtual bool SetStatus(int id, bool enabled, std::string* error) = 0;
    virtual bool ResetPassword(int id, const std::string& newPassword, std::string* error) = 0;
    virtual std::vector<std::string> DeptNames() = 0;
};
struct RoomTypeRecord { int id; std::string name; int bedCount; double price; std::string introduction,
                        photoPath, remark; bool enabled; std::string createTime; };
class IRoomTypeService { List(RoomTypeQuery) / Add / Update / Remove / SetStatus };
struct NursingProjectRecord { int id; std::string name, unit; int orderNo; double price; bool enabled;
                              std::string nursingRequirement, imagePath, createTime; };
class INursingProjectService { List(NursingProjectQuery) / Add / Update / Remove / SetStatus / ImageLibrary() };
```
`DataServices` 增加 `user / roomType / nursingProject` 三个 `unique_ptr`；`ModuleRegistry` 创建 `Seed*Service`。

### 3.2 种子数据（值取 Web 实测）
- 用户 7 条：admin/若依/院长办公室/15888888888/…（Web 截图 14 实测，详见 `SeedServices.cpp`），其余 101~105；
- 房型 6 条：单人套房 4000、双人套房 6000、豪华单人间 3000、豪华双人间 4500、普通单人间 2000、标准双人间 1500（床位数沿用 Web 库：前 5 条 0，第 6 条 10）；
- 护理项目 9 条（UI 规格 §5.1）：修剪指甲 30/次、衣物清洁 5/件、整理床铺 15/次、助餐 15/餐、助浴 40/次、洗头 20/次（禁用）、洗脸 15/次、洗脚 20/次、心理咨询 80/小时；图片按 UI 规格 §6.2 映射（9 张，另 3 张留在素材库）。
- **Sprint 3 替换点**：只替换 `src/data/SeedServices.cpp` 为 SQLite 实现（或新增 `SqliteUserService` 后在 ModuleRegistry 换构造），页面/组件零改动。

---

## 4. WIC 纹理层（Sprint 2 新增基础能力）

### 4.1 接口（`include/zzyl/render/Texture.h`）
```cpp
struct Texture { unsigned int id; int width, height; bool Valid() const; };
Texture LoadTextureFromFile(const std::filesystem::path&);   // WIC 解码 png/jpg/bmp/gif -> GL RGBA
void DestroyTexture(Texture&);
std::filesystem::path ResolveAssetPath(const char* relativeUtf8);  // cwd / exe 目录 / 源码根
void DrawImage(...); void DrawImageContain(...); void DrawImageCover(...);  // 拉伸/留白/裁剪
class TextureCache { const Texture& Get(const char* relativeUtf8); void Clear(); };
```
- WIC 为 Windows 系统组件（链接 `windowscodecs.lib`），**无第三方依赖**；支持中文文件名（UTF-8 源 → `std::filesystem::path` 宽字符）。
- 纹理参数：RGBA8、线性过滤、mipmap、Clamp；`TextureCache` 以相对路径为 key 缓存，程序退出统一销毁。
- 用途：护理项目图片列（60×60 cover）、表单图片选择（148×148 contain）、房型图片列（无素材时显示占位块，D-23：本地文件/素材库，二期做上传）。

### 4.2 资源与自测
- 12 张素材已复制到 `assets/icons/`（458KB，随仓库提交）；`assets/` 会在构建后自动复制到 exe 旁（CMake POST_BUILD），`ResolveAssetPath` 兼容源码根运行（VS 调试工作目录=源码根）。
- `--selftest` 新增 `08_texture_wic_1280x720.bmp`：加载 `assets/icons/修剪指甲@2x.png`（340×300）并绘制 + 中文标签；`metrics.json` 增加 `"wicTextureOk": true`。

---

## 5. 通用组件契约（M0）

`include/zzyl/ui/Widgets.h`（契约，**实现归属 cpp-dev1 / src/ui/Widgets.cpp**）：

| 组件 | 结构/函数 | 规格（UI v0.2 §3） |
|---|---|---|
| 搜索栏 | `SearchField`（label 68 / 控件 240×32 / Text·Select·DateRange）、`DrawSearchBar(area, fields)` | 行距 28、搜索/重置 80×32、主色 #00B8A0 |
| 工具栏 | `ToolbarButton`（Add/Edit/Delete/Import/Export）、`DrawToolbar(area, buttons)` | 80×32，浅青/浅绿/浅红/灰/浅橙五色，右侧圆形图标按钮 |
| 数据表格 | `TableColumn`（width/align/CellKind: Text/Image/StatusTag/Switch/Actions）、`TableRow{id,cells,enabled}`、`DrawDataTable(area, columns, rows, checkedIds, textures, options)` | 表头 40 `#F8F8F9`/13px `#515A6E`、行高 49、空态「暂无数据」、操作列主色文字按钮 |
| 分页 | `PaginationState`、`DrawPagination(area, state)` | 高 32、右对齐、“共 N 条 / 10条/页 / 前往 N 页”、当前页主色 |
| 表单弹窗 | `DialogState{open,title,width}`、`BeginFormDialog/EndFormDialog` | 宽 500/600、圆角 8、距顶 6vh、标题 18、底部确定/取消 |
| 确认框 | `DrawConfirmDialog(pageArea, open, title, message)` | 420×128、圆角 4、取消/确定 |
| Toast | `ToastState`、`PushToast`、`DrawToasts(windowArea, state, dt)` | 顶部居中 top 16、高 38、错误 `#FEF0F0`、3 秒消失 |

**页面使用顺序（固定骨架）**：`搜索栏 → 工具栏 → 表格（剩余高度）→ 分页（贴底）`；弹窗/确认框/Toast 在页面系统末尾绘制。表格宽/列宽按比例自适应（列宽基准值按表头 `width` 归一化到可用宽度，超宽内部横向滚动）。

### 5.1 契约变更记录（2026-11-24，v0.2 定稿）

为满足 dev2/dev3 三个列表页的交互回传，`include/zzyl/ui/Widgets.h` 补充（**已定稿，不再变更**）：

1. **表格事件回传**（原缺口：操作列/开关不知道作用于哪一行）：
```cpp
struct TableResult {
    int clickedRow = -1;                 // 普通行点击 -> TableRow.id
    int clickedAction = -1;              // Actions 列点击 -> 该行动作按钮下标
    int actionRowId = -1;                // Actions 列点击所在行 id
    struct SwitchToggle { int rowId; bool enabled; };
    std::vector<SwitchToggle> switchToggles;  // 本帧被切换的开关
    std::vector<int> checkedIds;              // 当前勾选 id
};
```
- **Actions 列逐行标签**：`TableRow.cells[actionCol]` 用 `'|'` 分隔本行按钮文字（如 `"修改|删除|禁用"`，启用/禁用随行状态变化），组件按行绘制；为空时回退 `TableDrawOptions.actionLabels`。`clickedAction` 是**该行标签数组**的下标。
- `CellKind::Switch` 切换通过 `switchToggles` 回传（`rowId + 新状态`），组件同时回填 `checkedIds` 最新态。

2. **表单弹窗确定/取消回传**（原缺口：页面无法知道结果、footer 责任不清）：
```cpp
enum class DialogResult { None, Ok, Cancel };
DialogResult EndFormDialog();   // 由 BeginFormDialog 返回 true 时必须配对调用
```
- 组件负责：遮罩、弹窗定位/宽度（500/600）、标题栏（含右上 ×）、**底部确定/取消按钮**；
- 调用方负责：仅内容区表单 + 校验红字；
- `Ok`：组件**不自动关闭**（方便调用方校验失败时继续显示红字）；调用方校验成功后自行 `state.open=false` 并提交；
- `Cancel`（含右上 ×）：组件自动 `state.open=false` 并返回 `Cancel`。

> 该变更仅涉及契约头 + `Widgets.cpp` 桩（已同步更新并 Release 构建通过），不影响已有代码；dev1 按此实现，dev2/dev3 按此调用。

---

## 6. 外壳与路由（dev1 增量）

| 项 | 要求 |
|---|---|
| 菜单可达性 | 用 `IsRoutablePath()` 判定：5 页可达；其余显示但点击 Toast「该模块将在后续迭代接入」 |
| 菜单高亮 | 用 `RouteState.path` 与 `MenuEntry.route` 前缀匹配（如 `/system/user` → 系统管理组展开 + 用户管理高亮 `#00B8A0`） |
| 目录展开 | 进入子页面时自动展开父目录（`ShellUiState.expandedRoot`），同组唯一展开 |
| 面包屑 | 首页 / 系统管理 / 用户管理（`RouteTitle` + 菜单上下文），当前项 `#97A8BE` |
| 标签栏 | 首页固定标签 + 当前页标签（12px、选中 `#00B8A0` 白字 + 8px 白点）；关闭/切换能力 Sprint 3 再做（本期至少展示当前页） |
| 顶栏右侧 | 沿用 v0.1（头像/昵称/F11 提示），不改 |

---

## 7. 文件边界（Sprint 2）

| 负责人 | 可写文件 | 只读依赖 |
|---|---|---|
| cpp-dev1 | `src/ui/Widgets.cpp`（M0 实现）、`src/shell/ShellModule.cpp`（外壳路由/面包屑/标签） | `ui/Widgets.h`、`app/Routes.h`、`AppState.h`、`MenuCatalog.h`、Theme/UiDraw |
| cpp-dev2 | `src/pages/SystemUserModule.cpp` | `ui/Widgets.h`、`app/Routes.h`、`AppState.h`、`data/BusinessServices.h`、Theme/UiDraw |
| cpp-dev3 | `src/pages/RoomTypeModule.cpp`、`src/pages/NursingProjectModule.cpp` | 同上 + `render/Texture.h`（图片列/预览）、`data/BusinessServices.h` |
| cpp-arch | CMakeLists、docs、include/zzyl/**、src/{main,core,platform,layout,render,data,app}/** | — |

规则：不修改他人文件；不改 `AppState.h`/`Widgets.h`/`BusinessServices.h` 契约（确需扩展先找架构师）；不改 CMakeLists（源文件已登记）；字体作用域必须在 `ImGui::End()` 前结束；不 commit。

---

## 8. Bug Triage 流程（Sprint 2 新规）

```
cpp-tester 测试发现问题
   → 测试报告只交 cpp-arch（架构）
   → cpp-arch 按“文件归属映射表”判定责任人（dev1/dev2/dev3）
   → cpp-arch 派单给责任程序员（附复现步骤/日志/截图）
   → 程序员**先向 cpp-control 申请调试资源**，再复现/修复（见 §8.1）
   → 程序员修复后直接与 cpp-tester 对接复测（复测同样先申请 cpp-control）
   → 全部通过后测试报告移交 lead
```
**文件归属映射表（测试/架构共用）**：
| 文件/范围 | 负责人 |
|---|---|
| `src/ui/Widgets.cpp`（搜索/工具栏/表格/分页/弹窗/确认/Toast 组件） | cpp-dev1 |
| `src/shell/ShellModule.cpp`（外壳、菜单、路由切换、面包屑、标签栏） | cpp-dev1 |
| `src/pages/SystemUserModule.cpp`（用户管理页） | cpp-dev2 |
| `src/pages/RoomTypeModule.cpp`（房型设置页） | cpp-dev3 |
| `src/pages/NursingProjectModule.cpp`（护理项目页） | cpp-dev3 |
| `src/pages/LoginModule.cpp`（登录页，Sprint 1 遗留问题） | cpp-dev2 |
| `src/pages/DashboardModule.cpp`（首页看板，Sprint 1 遗留问题） | cpp-dev3 |
| `src/data/SeedServices.cpp`、`src/data/BusinessServices.h`、`src/render/Texture.cpp`、`src/app/**`、`include/zzyl/**`、`CMakeLists.txt`、`assets/**` | cpp-arch（架构，必要时找 lead） |

### 8.1 调试资源纪律：cpp-control（2026-11-24 新规，立即生效）

**目的**：串行化「可见窗口交互 / 模拟鼠标键盘 / 前台抓屏」稀缺资源，避免多 agent 抢控制台导致调试失真。

| 必须申请（独占 8~10 分钟） | 无需申请 |
|---|---|
| 启动 `zzyl_desktop.exe` **可见窗口** | 各成员自己的 `build-devN` 构建 |
| 模拟鼠标/键盘（含 F11 全屏点验、点击/输入脚本） | `--selftest` 隐藏窗口自测（不占前台；同一时刻也别开多个实例） |
| 前台抓屏（CopyFromScreen / PrintWindow / 可见窗口截图） | 代码阅读、单元审查、独立临时程序（不启动本工程窗口） |

**流程**：`send_message` 给 **cpp-control**，格式 `申请者 | 目的 | 涉及页面/功能 | 预计时长(≤15分钟) | 需要资源(交互窗口/模拟输入/截图/构建)` → FIFO 批准，同一时刻仅 1 人 → 批准后独占前台（默认 10 分钟，有人排队则 8 分钟）→ 结束立即发「释放」；到期前 2 分钟提醒，超时 2 分钟未释放标记超时并上报 lead。**禁止同时开多个 `zzyl_desktop.exe` 实例。**
**适用对象**：cpp-dev1/2/3、cpp-tester、cpp-arch 一视同仁；调度日志由 cpp-control 维护。
> 本迭代 Sprint 2 收尾（BUG-S2-01 复测、三页 1920 证据）一律先申请 cpp-control。后续 Sprint 任务单必须写入本规则（模板见 `docs/tasks/_任务单通用规则.md`）。

---

## 9. 构建与自测（Sprint 2 骨架，已实测）

```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
build\Release\zzyl_desktop.exe --selftest artifacts\selftest_s2
```
实测（2026-11-24）：
| 项 | 结果 |
|---|---|
| Release 构建 | ✅ 通过（新增 `Texture.cpp`、`Routes.cpp`、`SeedServices.cpp`、`Widgets.cpp`、3 个页面模块） |
| 8 张自测截图 | ✅ 登录 / 看板 / 看板 1024×640 / 全屏 / 用户管理 / 房型设置 / 护理项目 / WIC 纹理 |
| 中文 | ✅ `glyphsOk: true` |
| 命令队列时序回归 | ✅ `commandQueueTimingOk: true` |
| WIC 纹理 | ✅ `wicTextureOk: true`（340×300 png 显示正常） |
| 路由 | ✅ 通过 CommandQueue 依次切换 3 个新页面（截图 05/06/07） |
| ECS 统计 | 17 实体 / 4 组件类型 / 15 资源 / 11 系统 |
| 证据目录 | `artifacts/selftest_s2/` |

---

## 10. 验收清单（Sprint 2）

- [ ] 通用组件：搜索栏/工具栏/表格/分页/弹窗/确认框/Toast 规格与 UI v0.2 §3 一致（逐项见 UI 规格 §8）；
- [ ] M1 用户管理：7 条数据、搜索（名称/手机号/状态/部门）、9 列表格、新增/修改（双列校验）、删除确认、重置密码、状态开关；
- [ ] M2 房型设置：6 条数据、图片列、单列弹窗（名称/床位数/费用/介绍/图片/备注）、增删改、启用禁用；
- [ ] M3 护理项目：9 条种子（或 12 条）、搜索（名称/状态）、10 列表格、图片列使用 12 张素材、弹窗校验；
- [ ] 外壳：5 页可达、目录自动展开、面包屑/标签栏/首页高亮正确；不可达菜单提示；
- [ ] 缩放 1024×640 / 1280×720 / 1920×1080：表格列宽比例自适应、分页贴底、弹窗居中不超屏；
- [ ] 渲染与数据分离：页面不直接 include SeedServices 实现；交互后数据从 `DataServices` 接口刷新；
- [ ] `--selftest` 不回归（8 图 + 3 个标记位为 true）。

---

## 11. 风险与决策

| 风险 | 影响 | 对策 |
|---|---|---|
| 无 SQLite（本机 `C:\Users\ASUS\Desktop\C++` 未找到 sqlite3） | 数据不持久，重启丢失 | 本迭代明确用种子数据；Sprint 3 向 lead 报安装（amalgamation 放 `third_party/`） |
| 房型图片素材缺失（Web 为 OSS URL） | 房型图片列只能占位 | 图片列显示占位块 + 名称；Sprint 3 支持本地文件/素材库 |
| 通用组件先于页面完成 | dev2/3 并行时页面无组件可用 | 契约已冻结；dev2/3 可先接契约写页面骨架，dev1 先完成 M0 基础组件；必要时用 `ui::PlaceholderPanel` 占位 |
| WIC 中文文件名/路径 | 资源加载失败风险 | 已实测 12 张中文名 png 加载成功；`ResolveAssetPath` 多候选路径；失败显示占位不崩溃 |
| 后端缺失 Controller/表（Web 清单 §5） | 无法真实联调 | 本迭代全用种子数据；Sprint 3 视老师补 SQL/接口再决定是否接后端 |
| 导出/导入、列设置、图片上传拖拽 | 次要功能 | 本期按钮置灰或 Toast「二期支持」（D-24/D-25） |

### 11.1 本期已知差异 / 裁剪记录（架构已确认）

| 编号 | 差异 | 处理 | 原因 |
|---|---|---|---|
| D-26 | 用户管理「操作」列基准宽 **200**（UI 规格 §4.1 为 150） | **采纳 200**，已记录；若 lead 要求严格 150，需 dev1 将操作列改为图标按钮（Web 为图标按钮）后再调回 | C++ 用文字按钮（修改/删除/重置密码/分配角色），14px 下 4 个文字需约 200px；截图 `artifacts/selftest_dev2/05_system_user_1280x720.png` 验证可读 |
| D-27 | 列表加载态未做遮罩（loading） | 本期接受（数据为内存种子，同步返回、无等待）；若 tester 认为影响体验，再让 dev1 在表格组件加可选 loading 遮罩 | 内存服务即时返回，遮罩会闪烁 |
| **D-28** | **用户管理页下滚后搜索栏/工具栏滚出视口**（P2-2） | **设计一致，不修**：与 Web 端一致（RuoYi `fixedHeader=false`，内容区整体滚动）；任务书也标注“分页常在视口下方，需滚动”。Sprint 3 若 lead 要求工具区常驻再评估 sticky | Web 现状 + UI v0.2 §2 明确长页面整体纵向滚动 |
| **D-29** | **展开底部「系统管理」时侧栏菜单自动滚动，同级菜单滚出视口**（P2-3） | **接受现状**（展开块滚入可视区的体验取舍）；后续有人反馈再优化（可改“仅在子项超出可视区时最小滚动”） | 桌面侧栏可视高度有限；当前行为可预测 |
| D-30 | 用户管理行内「分配角色」为 Toast「二期支持」 | 采纳（分配角色为独立跳转页，Sprint 2 不实现） | 范围裁剪，与 M1 验收项一致 |

### 11.2 Sprint 2 Bug 登记表（架构维护，tester/lead 共用）

> 口径：tester 报告 → 架构 triage → 责任 dev 修复 → dev 直接与 tester 复测 → 架构更新状态 → 全部通过后报告 lead。
> 文件归属映射见 §8。

| Bug 编号 | 来源 | 级别 | 现象 | 责任文件 / 负责人 | 状态 | 复测 |
|---|---|---|---|---|---|---|
| **BUG-S2-01** | tester O-01（P2-1） | P2 | 护理项目表格横向滚动条压在最后一行内容上（行被遮） | `src/ui/Widgets.cpp` / **cpp-dev1** | ✅ **已关闭**：dev1 修复（滚动条独立 6px+6px 留白、横滚增强）→ **tester 双档复测 PASS**（1280/1024：①滚动条不遮末行 ②分页可见可点 ③横滚至最右可见操作列 ④末行完整；回归用户/房型无溢出）；架构隐藏 selftest 复核通过 | tester 证据：`artifacts/test_tester_s2/fix_retest/f01~f07`、`selftest_fix/07/08` |
| —（设计确认） | tester O-02（P2-2） | P2 | 用户管理下滚工具区滚出视口 | `src/shell/ShellModule.cpp` / dev1 | ⚪ 不修（设计一致，D-28） | — |
| —（体验取舍） | tester O-03（P2-3） | P2 | 展开系统管理时菜单自动滚动 | `src/shell/ShellModule.cpp` / dev1 | ⚪ 接受现状（D-29） | 有人反馈再优化 |
| —（环境） | tester O-04 | 非缺陷 | 多 agent 抢前台导致模拟键盘偶发无效 | — | ⚪ 环境因素，不计产品问题 | 全屏已由确定性帧验证 |

**待补覆盖项（tester 自行补齐，dev 提供证据给 tester → tester 报告标 PASS\*）**
| 项 | 内容 | 证据提供方 |
|---|---|---|
| COV-01 | M1 校验组合逐项 + 修改回填隐藏用户名/密码 | cpp-dev2 ✅ 已提供：`artifacts/verify_dev2/s2cov/c01_multi_validation.png`、`c02_password_illegal.png`、`c03_edit_dialog.png` |
| COV-02 | 超管删除/停用保护 | cpp-dev2 ✅ 已提供：`s2cov/c04_delete_confirm_admin.png`、`c05_delete_admin_rejected.png`、`c06_disable_admin_rejected.png`（数据层保护 `SeedServices.cpp:107/124`，预期行为） |
| COV-03 | M2/M3 保存/删除后刷新 | cpp-dev3（实测证据/说明发 tester） |
| COV-04 | 1024/1920 三页列宽/弹窗不超屏 | cpp-dev2 ✅ 用户页：`verify_dev2/s2cov04/g00~g05`（1280/1024/1920 页面+弹窗，**隐藏 selftest 帧缓冲，非前台抓屏**；旧 `real_m0/v03_user_1920` 已确认无效）；cpp-dev3 房型/护理页 1920 证据待补（tester 报告未覆盖项） |

### 11.3 Sprint 2 交付结论（2026-11-24）

| 项 | 内容 |
|---|---|
| 测试报告 | `docs/test/Sprint2_测试报告.md`（**v3 终版**，2026-11-24 13:46） |
| 用例统计 | **61 项：PASS 60 / FAIL 0 / 未覆盖 1**；P0/P1 = 0 |
| 唯一真缺陷 | BUG-S2-01（P2）→ 已修复 + tester 双档复测 PASS → ✅ 关闭 |
| **结论** | ✅ **Sprint 2 可交付**（M0 通用组件 + M1 用户管理 + M2 房型设置 + M3 护理项目 + 外壳 5 页路由/菜单/标签/面包屑均达验收主项；构建可复现、无崩溃/ImGui 断言、中文无方块） |
| 非阻塞遗留 | ① M0 表格空态未覆盖（种子非空、无 UI 入口触发；建议 Sprint 3 加空态注入开关补测）② 房型/护理 1920 逐像素建议 Sprint 3 补（当前以确定性全屏帧+自适应实现+dev3 1024 证据标 PASS*）③ 差异项 D-23/D-26/D-27/D-28/D-29/D-30 已列 |
| 调试资源 | 全程遵守 cpp-control 调度：tester 终测后已释放；dev3 补 1920 证据需重新排队申请 |


---

*维护：契约变更（Widgets.h / BusinessServices.h / Routes.h / Texture.h）必须由架构师同步更新本文档与任务单，并通知 dev/tester。*
