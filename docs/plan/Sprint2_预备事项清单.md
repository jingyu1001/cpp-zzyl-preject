# Sprint 2 预备事项清单（只做准备，不实现）

> 编制：cpp-arch（2026-11-23，lead 指示"先列清单，先不实现"）
> 适用：Sprint 2（业务页面 + 数据层落地 + 图片资源）
> 前置阅读：`docs/architecture/Cpp架构设计_v0.1.md`、《Web架构提取_v0.1.md》§9、Web 端源码

---

## 0. 需要 lead 决策 / 报安装的事项（汇总）

| # | 事项 | 建议方案 | 是否需要"安装需求" |
|---|---|---|---|
| 1 | 图片解码 | **Windows WIC**（系统 API，`wincodec.lib` + `windowscodecs.lib`，无需第三方库） | 否（系统自带） |
| 2 | JSON 解析（接后端接口用） | `nlohmann/json` 单头文件放入 `third_party/`（约 1MB，header-only） | 否（复制单头即可）；或 lead 统一装 vcpkg |
| 3 | HTTP 客户端（接 RuoYi 后端） | 方案 A：WinHTTP（系统 API，无需安装）；方案 B：libcurl（需安装） | 建议 A：**零依赖** |
| 4 | SQLite3（本地缓存/会话） | sqlite3 amalgamation 3.45+（`sqlite3.c/h` 复制进 `third_party/sqlite3/`） | 需 lead 确认来源；若用 vcpkg 则报安装 |
| 5 | MySQL 直连（若课程要求） | MySQL Connector/C++ 8.x（vcpkg 或 MSI） | **需要 lead 安装**（版本/路径待定） |
| 6 | 缺失业务表 SQL | 向老师/后端要完整建表+数据 SQL | 否（协作事项） |
| 7 | 图片资源复制 | 从 Web 仓库 `src/assets/images/` 复制到 `cpp-zzyl-preject/assets/images/` | 否（同项目资源，lead 知悉即可） |

> 纪律：以上 1 中的 WIC、3A WinHTTP 属于系统能力，只需 CMake 加系统库；2/4 的第三方代码必须 lead 同意后再放入 `third_party/`（不直接下载）；5 必须走安装需求流程。

---

## 1. 图片 / 纹理系统（Sprint 2 第一个技术能力）

### 1.1 架构落点（预计新增，不改已有接口）
```
include/zzyl/render/Texture.h      // Texture{unsigned int id; int width,height; bool valid();}
src/render/Texture.cpp             // WIC 解码 → GL 纹理
include/zzyl/render/TextureCache.h // 资源级纹理缓存（World::Resource）
```
建议接口：
```cpp
namespace zzyl::render {
struct Texture { unsigned int id = 0; int width = 0; int height = 0; bool Valid() const; };
Texture LoadTextureFromFile(const char* path);            // WIC: jpg/png/bmp/gif
Texture LoadTextureFromMemory(const void* data, size_t n); // 后续验证码图片用
void DestroyTexture(Texture&);
std::string ResolveAssetPath(const char* relative);       // cwd / exe 目录 / 源码根 三种候选
void DrawImage(ImDrawList*, const Texture&, const layout::Rect& target, ImU32 tint = IM_COL32_WHITE);
void DrawImageCover(ImDrawList*, const Texture&, const layout::Rect& target); // 等比 cover 裁剪
}
```
要点：
- WIC 用 `IWICImagingFactory` → decoder → `GUID_WICPixelFormat32bppRGBA` → `glTexImage2D(GL_RGBA)`；COM 只 `CoInitializeEx` 一次（App 启动时）。
- ImGui 1.92 `ImTextureRef` / `AddImage` 用法：`drawList->AddImage(ImTextureRef(static_cast<ImTextureID>(tex.id)), min, max)`；纹理 id 生命周期由 `TextureCache` 管理。
- 纹理缓存（`TextureCache` 资源）：文件路径 → Texture；启动时按需加载（登录背景/logo/头像），退出时统一销毁。

### 1.2 资源清单（Web 端已有，Sprint 2 复制到 `assets/images/`）
| 资源 | Web 路径 | 用途 |
|---|---|---|
| 登录背景 | `src/assets/images/login-background.jpg`（2842×1469，294KB） | 登录页 cover 背景 |
| Logo | `src/assets/logo/logo.png` | 侧栏 Logo / 登录标题 |
| 个人头像 | `src/assets/images/grzxhz.jpg` | 个人卡 / 顶栏头像 |
| 下属员工头像 | `img1.png ~ img6.png` | 个人卡（P1.5） |
| 常用功能图标 | `icon_cwbb/rutui2x/rutui/cwyl/xzyh/ddgl2/tkgl/bjsj.png` | 8 宫格（P1.5） |
| 大/小背景 | `bigbg.png / smallbg.png` | 备用 |
| 菜单图标 | Web 为 `src/assets/icons/svg/*.svg` | 需先栅格化为 PNG 或用字体图标方案（本期可继续几何占位） |

### 1.3 验收标准
- 登录页背景为真实照片 cover 无变形（与 `assets/01_登录页_1280x720.png` 对比）；
- 侧栏 Logo 为白色 logo 图 + “中州养老”；
- 顶栏/个人卡头像显示；窗口缩放/全屏时图片等比裁剪正确；
- 未找到资源时优雅回退（不崩溃，显示渐变/占位）。

---

## 2. 数据库接入

### 2.1 SQLite3（本地库，优先做）
- 依赖：sqlite3 amalgamation（`sqlite3.c` / `sqlite3.h`，约 9MB 源码），放 `third_party/sqlite3/`；CMake 加入静态库；无需外部安装。
- 用途：会话持久化（记住密码/token）、菜单缓存、看板数据缓存、离线只读数据。
- 表设计草案（本地库 `zzyl_local.db`）：
  ```sql
  CREATE TABLE app_session (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    user_name TEXT, nick_name TEXT, role_group TEXT, post_group TEXT,
    token TEXT, token_expire_at TEXT, remember_me INTEGER DEFAULT 0,
    updated_at TEXT DEFAULT (datetime('now','localtime'))
  );
  CREATE TABLE menu_cache (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    payload_json TEXT NOT NULL, fetched_at TEXT NOT NULL
  );
  CREATE TABLE dashboard_cache (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    payload_json TEXT NOT NULL, fetched_at TEXT NOT NULL
  );
  PRAGMA user_version = 1;  -- 迁移版本
  ```
- 接口（数据层）：`ISessionStore`、`ICacheRepository`；Mock/内存实现先行，SQLite 实现替换；`Database` 生命周期由 App 管理（启动打开、退出关闭）。
- 验收：登录后重启可见“记住的账号”；看板数据优先读缓存；DB 文件损坏时回退内存实现不崩溃。

### 2.2 MySQL / 后端接口（二选一，待 lead 定）
| 方案 | 做法 | 依赖 | 评价 |
|---|---|---|---|
| A（推荐） | WinHTTP 调 RuoYi REST：`POST /login`、`GET /getInfo`、`GET /getRouters`、`GET /system/user/profile`；token 存 `Authorization: Bearer` | 系统 WinHTTP + JSON 单头 | 与 Web 端同源语义，最小风险 |
| B | MySQL Connector/C++ 直连 `zzyl` 库 | 需 lead 安装 connector | 课程若要求直连才选；需处理连接池/线程安全 |

接口契约（与 Web 一致，来自 Web架构提取 §6.2）：
- `POST /login {username,password,code,uuid}` → `{code,msg,token}`
- `GET /getInfo`（Bearer）→ `{user:{...},roles,permissions}`
- `GET /getRouters`（Bearer）→ 菜单树（驱动 `MenuCatalog` 动态化）
- `GET /system/user/profile`（Bearer）→ 个人卡字段
- 验证码：`GET /captchaImage` → `{captchaEnabled,uuid,img(base64)}` → 用 `LoadTextureFromMemory` 显示（需 Base64 解码）

异步模型：数据服务接口在 Sprint 2 升级为异步（`Future`/回调 + 主线程消费结果），渲染层保持不变（读资源）。线程安全：IO 线程只写“结果队列”，主线程在 Simulation 系统合并。

---

## 3. 业务页面缺失表清单（Web架构提取 §9.3，重要风险）

| 缺失表 | 对应页面/功能 | 影响 | 需要谁补 |
|---|---|---|---|
| `device` | 智能监测-设备管理 | 页面/接口报错 | 老师/后端补 SQL |
| `device_data` | 设备详情 | 同上 | 同上 |
| `alert_rule` | 智能监测-报警规则 | 同上 | 同上 |
| `alert_data` | 智能监测-报警数据 | 同上 | 同上 |
| `reservation` | 来访管理-预约信息 | 同上 | 同上 |
| `family_member` | 小程序家属成员接口 | 同上 | 同上 |
| （无 Controller） | `views/nursing/healthAssessment/*` 健康评估 | 页面有、接口无 | 后端补 Controller |

现有可用表（Sprint 2 可先做）：`sys_user`(8)、`sys_role`(6)、`sys_menu`(128)、`sys_dept`(14)、`sys_post`(4)、`sys_config`(6)、`sys_dict_*`、`elder`(5)、`bed`(89)、`room`(60)、`room_type`(6)、`floor`(11)、`check_in`(5)、`contract`(1)、`nursing_elder`(7)、`nursing_level`(3)、`nursing_plan`(3)、`nursing_project`(9)、`nursing_project_plan`(6)、`nursing_task`(0)。
> 建议 Sprint 2 第一批业务页优先选“表齐全”的：老人管理（elder）、床位（bed/room/floor/room_type）、护理项目/计划/等级（nursing_*）、入住登记（check_in）、系统管理（sys_*）。

---

## 4. 通用 UI 组件（Sprint 2 复用，按需做，不过度设计）

| 组件 | 归属建议 | 说明 |
|---|---|---|
| 表格（`Table`）：列定义/排序/行 hover/斑马纹/分页 | dev1（外壳/通用组件） | 数据用 `TableModel` 资源；行高 40/48；表头 sticky |
| 搜索栏（输入 + 查询/重置按钮） | dev1 | 与表格配套 |
| 弹窗（Modal）/确认框 | dev1 | 圆角 8、遮罩 rgba(0,0,0,.45) |
| 表单控件（输入/下拉/日期/开关） | dev2（可复用登录页封装） | 统一 40 高、边框 #DCDFE6 |
| 消息提示（成功/失败 toast） | dev1 | 顶部居中，2s 自动消失 |
| 标签页 / 面包屑多级 | dev1 | 复用现有 TagsView/面包屑 |
| 虚拟滚动（大列表） | 架构师评估 | 先用分页，不做虚拟滚动（避免过度设计） |

ECS 组件草案（页面内数据驱动）：
```cpp
struct TableColumn { std::string title; float widthRatio; };        // 比例列宽
struct TableRow { std::vector<std::string> cells; };                // 或按实体+组件
struct TableView { int rowCount; int page; int pageSize; };
struct FormField { std::string label; std::string value; int type; };
```
> 具体等到第一个业务页需求明确后再冻结接口，避免提前设计。

---

## 5. Sprint 2 任务拆分草案（待 lead 确认后细化成任务单）

| 任务 | 负责人 | 依赖 |
|---|---|---|
| 图片/纹理系统（WIC + TextureCache）+ 登录背景/Logo/头像接入 | cpp-arch 出接口 → dev2/dev1 用 | WIC（系统） |
| SQLite3 会话/缓存层 + 记住账号 | cpp-arch 接口 + dev2 登录 | sqlite3 amalgamation（lead 同意） |
| 后端接口客户端（WinHTTP + JSON）+ 登录真实认证（可选） | cpp-arch 出 `HttpClient` 接口 → dev2 | lead 确认后端/Redis 可用（Web 架构 R3） |
| 业务页第一批：老人管理（表格+搜索+新增/编辑弹窗） | dev2 或 dev3（按页面分） | 表齐全 + 通用组件 |
| 业务页第一批：床位/房型/楼层（树/表格） | dev3 | 同上 |
| 业务页第一批：护理项目/等级/计划（表格 CRUD） | dev1 | 同上 |
| 看板 P1.5：个人卡 / 8 宫格 / 等级·年龄·预约三卡 + 内容滚动打磨 | dev3 | 图片资源、滚动能力 |
| 菜单动态化（getRouters → MenuCatalog） | cpp-arch + dev1 | HTTP 客户端 |
| 缺失表补齐 | lead/老师 | — |

---

## 6. Sprint 2 验收基线（建议）

1. 图片：登录背景/Logo/头像真实资源，缩放/全屏正确，资源缺失可回退。
2. 数据：SQLite 记住账号可用；后端可用时登录走真实 `/login`，不可用时回退 Mock（配置开关）。
3. 业务：至少 2 个业务页面（优先表齐全的）完成列表 + 搜索 + 分页 + 新增/编辑弹窗，数据来自后端或本地库。
4. 看板：P1.5 补齐个人卡/常用功能/三卡（时间允许），滚动顺畅。
5. 稳定性：连续 10 分钟操作无崩溃；ImGui 无断言；F11/缩放/切换页面布局正常。

---

## 7. 风险与依赖（Sprint 2 开始前确认）

| 风险 | 决策点 |
|---|---|
| 后端强依赖 Redis（Web 架构 R3） | 若后端跑不起来，登录真实认证推迟，用 Mock + SQLite 会话 |
| 数据库缺 6+ 张业务表 | 优先实现表齐全页面；缺失表页面先做只读占位 |
| 图片素材版权/来源 | 同项目 Web 资源，lead 知悉后复制 |
| JSON/HTTP/SQLite 依赖引入方式 | 统一规则：系统 API 优先（WIC/WinHTTP）；header-only 单头需 lead 同意后放 `third_party/`；二进制库走安装需求 |
| ImGui 1.92 纹理 API（ImTextureRef） | 先做小 demo（登录背景）验证，再推广 |

---

*本清单只做准备，Sprint 2 正式开工前由 lead 确认优先级与依赖安装；确认后由架构师拆成 dev1/2/3 文件级任务单。*
