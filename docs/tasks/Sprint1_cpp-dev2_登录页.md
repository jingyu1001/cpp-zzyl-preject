# Sprint 1 任务单 · cpp-dev2 · 登录页（表单 + 模拟登录 + 跳转）

> 负责人：cpp-dev2
> 唯一可写文件：`src/pages/LoginModule.cpp`（可新增 `include/zzyl/pages/LoginTypes.h` 头文件）
> 必读：`docs/architecture/Cpp架构设计_v0.1.md` 第 5/7 章；Web 端《UI复刻规格_Cpp_1280x720_v0.1.md》§7（登录页规格）+ `assets/01_登录页_1280x720.png`
> 骨架现状：`LoginPageRenderSystem` 已按 Web 规格画出静态登录页（全屏渐变背景 + 400×407 白卡 + 标题 + 3 行输入框占位 + 验证码占位 + 记住密码 + 主色登录按钮 + 页脚版权），但**没有交互、没有登录逻辑**。本任务把它做成可用的登录页。

## 1. 目标（验收画面）

- 全屏背景（Web 为 `login-background.jpg` cover；当前无图片解码能力，先用渐变占位，Sprint 2 换图——见 D-06）；
- 400×407 白卡居中、圆角 6、内边距 25；标题「中州养老」16.5px / #707070；输入框 350×40、行距 68；验证码 221 输入 + 119 图形；记住密码；登录按钮 350×40 #00B8A0；页脚 12px 白字；
- 真实输入框：账号、密码（密文）、验证码；验证码可点击“刷新”（本迭代用随机 4 位字符占位）；
- 「登 录」按钮 + Enter 提交；登录中按钮禁用/文案「登 录 中...」；
- Mock 登录：`admin / admin123 + 任意非空验证码` → 进入首页看板；错误账号/密码 → 红色提示；空值 → 字段校验提示；
- 缩放 1024×640 / F11 全屏下卡片保持居中且不溢出。

## 2. 实现要点（在骨架文件里改）

### 2.1 登录表单状态（每帧持久）
在 `LoginModule.cpp` 内定义并注册资源（`RegisterLoginModule` 里 `world.EmplaceResource<LoginFormState>()`）：
```cpp
struct LoginFormState {
    char username[64] = "admin";   // Web 演示环境默认预填
    char password[64] = "admin123";
    char code[16] = "";            // 验证码输入
    char captcha[8] = "";          // 当前“验证码图片”随机字符（占位）
    bool rememberMe = false;
    bool loading = false;
    bool submitted = false;
    float errorFlashTimer = 0.0f;
};
```

### 2.2 系统拆分（都在同一个 .cpp）
- `LoginPageRenderSystem`（Render，order 200）：只负责绘制 + 采集交互。
  - 骨架里输入框是 DrawList 占位；改成真正的 `ImGui::InputText`：`SetCursorScreenPos` 到每个输入框位置、`SetNextItemWidth(350)`，`PushStyleVar(FrameRounding,4)` + 自定义 FrameBg 白 / 边框 #DCDFE6 / focus 边框 #00B8A0（ImGui 可用 `ImGuiCol_FrameBg`、`ImGuiCol_Border`、`ImGuiCol_NavCursor` 近似；必要时自绘边框）。
  - 输入框位置全部由卡片矩形推导（card.x+25 / card.y+73 + 68*i），不要写死屏幕坐标。
  - 密码：`ImGuiInputTextFlags_Password`；Enter 提交（`ImGui::IsKeyPressed(ImGuiKey_Enter, false)`，避免在输入法组合时误触发可用 `io.WantTextInput` 判断）。
  - 验证码：输入宽 221；右侧 119×40 的“验证码图”用 `InvisibleButton` 检测点击 → 重新随机 4 位字符；显示字符用 18px 粗体 #00B8A0 倾斜/彩色即可（占位）。
  - 记住密码：14×14 复选框 + 文字；点击切换；本迭代只保存到资源，不落盘（Sprint 2 用本地配置/系统凭据，D-05）。
  - 错误提示：读 `AuthFeedback`，`hasError` 时在按钮下方 12px 红色显示 `message`；提交前做空值校验（账号/密码/验证码必填），提示写自己的 `LoginFormState`。
  - 按钮：骨架是自绘矩形；改成可点击区域（`InvisibleButtonAt` + hover/active 颜色 #33C6B3/#00977F），loading 时文字「登 录 中...」且禁用。
  - 所有 PushFont 作用域用 `ui::ScopedFontSize/ScopedBoldFont`，注意结束位置。
- `AuthSystem`（Simulation，order 20）：消费 `CommandQueue::Type::Login`：
  ```cpp
  auto* services = world.TryResource<DataServices>();        // 空则返回
  for (const auto& cmd : queue->commands) {
      if (cmd.type != CommandQueue::Type::Login) continue;
      data::LoginResult r = services->auth->Login({cmd.arg0, cmd.arg1, cmd.arg2});
      auto* feedback = world.TryResource<AuthFeedback>();
      feedback->hasError = !r.ok; feedback->message = r.ok ? "" : r.message;
      auto* form = world.TryResource<LoginFormState>();
      form->loading = false;
      if (r.ok) {
          auto* session = world.TryResource<AuthSession>();
          session->loggedIn = true; session->user = r.session;
          world.TryResource<RouteState>()->current = Route::Dashboard;   // 登录系统负责切页
          if (services->dashboard) world.SetResource(services->dashboard->LoadDashboard(r.session));
      }
  }
  ```
  不要清空命令队列（App 每帧末统一清空）；不要阻塞 IO。
- 渲染层提交时只入队命令：`CommandQueue::Command{Type::Login, username, password, code}`。

### 2.3 资源与接口
| 资源 | 读/写 | 用途 |
|---|---|---|
| `layout::ShellLayout` / `WindowState` | 只读 | viewport 尺寸（登录页全屏用 viewport） |
| `DataServices` | 只读 | `services->auth->Login(...)` |
| `AuthSession` / `AuthFeedback` / `RouteState` | 可写（仅 AuthSystem） | 登录结果 / 错误 / 切页 |
| `CommandQueue` | 入队 Login | 渲染层唯一通道 |
| `UiState` | 只读 | — |

## 3. 硬性约束

- 只改 `src/pages/LoginModule.cpp`（新增头文件仅限 `include/zzyl/pages/LoginTypes.h`）。
- 布局：卡片宽 `min(400, viewport.w−80)`、高 `min(407, viewport.h−80)` 居中；卡片内元素按 Web 规格固定偏移（25/73/141/209/434/491 等）由卡片矩形推导；**不写死屏幕坐标**。
- 渲染系统只读业务资源、只入队命令；数据获取一律在 `AuthSystem` 中通过 `DataServices`（渲染与数据分离硬约束）。
- 颜色用 `theme::Colors()`（主色 #00B8A0、边框 #DCDFE6、错误 #F56C6C 等）；不新增硬编码色值。
- 字体作用域用 RAII 类；不修改 `AppState.h`/`ModuleHooks.h`/CMakeLists。
- Web 规格 D-05：记住密码不照抄 Cookie/RSA，Sprint 2 改本地安全存储；本迭代仅内存。

## 4. 自测与验收

1. 编译 Release（命令见总览）。
2. `build\Release\zzyl_desktop.exe`：
   - 默认账号 admin / admin123 预填；随便填 4 位验证码 → 点登录或 Enter → 进入首页（侧栏出现）；
   - 密码改错 → 红色提示「账号或密码错误（演示账号：admin / admin123）」；
   - 账号清空 → 「请输入您的账号」；验证码清空 → 「请输入验证码」；
   - 点验证码图 → 字符变化；
   - 记住密码可勾选/取消（视觉态）；
   - 1024×640 / F11 → 卡片居中不溢出、页脚在底部。
3. 自测截图：`build\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev2`，`01_login_1280x720.png` 为登录页画面；与 Web `assets/01_登录页_1280x720.png` 对比（允许差异：背景图为渐变占位、图标为占位方块）。
4. 汇报 lead：完成内容、改动文件、编译/自测结果、截图、风险（含 IME 中文输入是否正常）。

## 5. 提示 / 常见坑

- 输入框风格：ImGui 默认 FrameBg 是白色、边框 0；需要 `PushStyleVar(ImGuiStyleVar_FrameBorderSize,1)` + `PushStyleColor(ImGuiCol_Border,#DCDFE6)` + `PushStyleColor(ImGuiCol_FrameBg,#FFFFFF)`，focus 用 `ImGui::IsItemActive()` 自绘 #00B8A0 边框。
- 登录成功切路由后本页面系统会在下一帧因 `RouteState != Login` 直接 return，无需手动隐藏。
- 验证码是本地占位（不调接口）；后端 `/captchaImage` 接入在 Sprint 2（需要 HTTP 客户端）。
- `AuthFeedback` 初始无错误；只在失败时显示，成功时清空。
- 自测模式下 `--selftest` 会在第 3 帧自动 Mock 登录，`01_*.bmp` 是登录页、`02` 及之后是看板。
