#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

#include "imgui.h"

#include "zzyl/app/AppState.h"
#include "zzyl/app/ModuleHooks.h"
#include "zzyl/core/System.h"
#include "zzyl/data/Services.h"
#include "zzyl/layout/Layout.h"
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"

namespace zzyl {
namespace {

// =============================================================================
// 登录页（cpp-dev2 · Sprint 1）
//
// 依据：
//  - Web 源码 `src/views/login.vue`：账号 / 密码 / 验证码 + 记住密码 + 「登 录」+ 版权；
//  - UI 复刻规格 §7：登录卡 400×407（1280×720 基准）、内边距 25、输入 350×40、
//    验证码行 221 + 119、按钮 350×40、页脚 12px 白字、卡片圆角取自 LayoutTokens；
//  - Sprint1_cpp-dev2_登录页.md：表单状态 / 校验 / 命令入队 / 模拟登录。
//
// 布局原则（硬约束「禁止写死位置」）：
//  所有几何量 = 设计稿基准（400×407 那一套） × 由窗口尺寸推导的等比缩放 k；
//  屏幕坐标一律由 viewport → card → 卡片内比例 逐层推导，没有绝对屏幕坐标。
//
// 数据流（硬约束「渲染与数据分离」）：
//  LoginPageRenderSystem（Render）只读资源 + 只向 CommandQueue 入队 Login 命令；
//  AuthSystem（Simulation）消费命令 → DataServices.auth->Login() →
//  写 AuthSession / AuthFeedback / RouteState。渲染系统不碰数据服务、不改业务资源。
// =============================================================================

// -----------------------------------------------------------------------------
// 设计基准（1280×720 下的 UI 规格值，仅作为等比缩放的基准，不直接出现在屏幕坐标里）
// -----------------------------------------------------------------------------
constexpr float kDesignViewportWidth = 1280.0f;  // 设计基准窗口宽（推导比例用）
constexpr float kDesignCardWidth = 400.0f;
constexpr float kDesignCardHeight = 407.0f;
constexpr float kDesignPaddingX = 25.0f;    // 卡片左右内边距
constexpr float kDesignTitleTop = 25.0f;    // 标题距卡片顶
constexpr float kDesignTitleHeight = 18.0f;
constexpr float kDesignFieldTop = 73.0f;    // 账号输入框距卡片顶
constexpr float kDesignFieldStep = 68.0f;   // 输入行间距（40 高 + 28 间距）
constexpr float kDesignFieldHeight = 40.0f;
constexpr float kDesignCodeWidthRatio = 0.63f;  // 验证码输入宽 = 内宽 63%
constexpr float kDesignCodeGap = 13.0f;         // 验证码输入与图形区间隙
constexpr float kDesignRememberTop = 277.0f;    // 记住密码行距卡片顶
constexpr float kDesignRememberHeight = 32.0f;
constexpr float kDesignCheckboxSize = 14.0f;
constexpr float kDesignButtonTop = 334.0f;      // 登录按钮距卡片顶
constexpr float kDesignErrorGap = 8.0f;         // 错误提示与按钮的下间距
constexpr float kDesignErrorHeight = 16.0f;
constexpr float kDesignFooterHeight = 40.0f;
constexpr float kDesignIconLeft = 11.0f;        // 输入框内图标左边距
constexpr float kDesignIconSize = 14.0f;
constexpr float kDesignTextLeft = 33.0f;        // 输入文字起点（11 + 14 + 8）

// 字号（设计基准 px；实际字号 = 基准 × k）
constexpr float kDesignTitleFontSize = 16.5f;
constexpr float kDesignBodyFontSize = 14.0f;
constexpr float kDesignHintFontSize = 12.5f;
constexpr float kDesignFooterFontSize = 12.0f;
constexpr float kDesignCaptchaFontSize = 17.0f;

// 卡片宽度约束：设计稿 400/1280 = 31.25%，随窗口等比缩放并 clamp（布局约束，非绝对坐标）
constexpr float kCardWidthRatio = kDesignCardWidth / kDesignViewportWidth;
constexpr float kCardWidthMin = 360.0f;
constexpr float kCardWidthMax = 460.0f;
constexpr float kCardHeightSafeRatio = 0.90f;  // 卡片最高占视口 90%（上下留安全边距）

// Web 规格中主题没有对应语义色的两项，集中定义避免散落裸色值
constexpr ImU32 kTitleText = IM_COL32(0x70, 0x70, 0x70, 0xFF);     // 登录标题 #707070
constexpr ImU32 kFieldBorder = IM_COL32(0xDC, 0xDF, 0xE6, 0xFF);  // 输入框边框 #DCDFE6

// 背景占位色：Web 为 login-background.jpg 照片背景；当前无图片解码能力（未装 stb_image），
// 按 lead 要求用「深色渐变 + 两个青绿光斑」近似（Sprint 2 接入图片后替换）。
constexpr ImU32 kBackgroundTop = IM_COL32(0x0C, 0x3E, 0x37, 0xFF);
constexpr ImU32 kBackgroundBottom = IM_COL32(0x02, 0x10, 0x0E, 0xFF);

// 验证码占位字符序列：点击刷新时循环切换（不用真随机，保证自测/复现稳定）。
constexpr const char* kCaptchaPool[] = {"A3F7", "Q8K2", "M7T5", "H6P9",
                                        "K4W2", "R5Y8", "B9D3", "N2S6"};

// -----------------------------------------------------------------------------
// 登录表单状态（表现层状态，仅登录模块读写；业务结果由 AuthFeedback/AuthSession 承载）
// -----------------------------------------------------------------------------
struct LoginFormState {
    char username[64] = "admin";    // 与 Web 登录页预填一致（演示环境）
    char password[64] = "admin123";
    char code[16] = "";
    char captcha[8] = "A3F7";       // 验证码占位图形上的字符（点击刷新）
    int captchaIndex = 0;
    bool rememberMe = false;
    bool loading = false;
    bool submitted = false;  // 是否提交过（决定是否展示错误提示）
};

// -----------------------------------------------------------------------------
// 登录卡几何（全部由 viewport 推导）
// -----------------------------------------------------------------------------
struct LoginGeometry {
    layout::Rect card;
    layout::Rect title;
    layout::Rect username;
    layout::Rect password;
    layout::Rect code;
    layout::Rect captcha;
    layout::Rect rememberRow;
    layout::Rect checkbox;
    layout::Rect button;
    layout::Rect error;
    layout::Rect footer;
    float scale = 1.0f;
};

LoginGeometry ComputeLoginGeometry(const layout::Rect& viewport) {
    LoginGeometry geo;

    // 1) 卡片宽：设计基准比例 × 视口宽，clamp 上下限（小窗口不溢出 / 大窗口不过分膨胀）
    float cardWidth = layout::Clamp(viewport.w * kCardWidthRatio, kCardWidthMin, kCardWidthMax);
    float scale = cardWidth / kDesignCardWidth;

    // 2) 高度安全约束：卡片不超过视口 90% 高，不足时整体等比缩小（内部行距同步缩放）
    const float maxCardHeight = viewport.h * kCardHeightSafeRatio;
    if (maxCardHeight > 0.0f && kDesignCardHeight * scale > maxCardHeight) {
        scale = maxCardHeight / kDesignCardHeight;
        cardWidth = kDesignCardWidth * scale;
    }
    const float cardHeight = kDesignCardHeight * scale;
    geo.scale = scale;

    // 3) 卡片居中
    geo.card = layout::Rect{(viewport.w - cardWidth) * 0.5f, (viewport.h - cardHeight) * 0.5f,
                            cardWidth, cardHeight};

    // 4) 卡片内部：设计基准 × scale 推导（无绝对坐标）
    const float paddingX = kDesignPaddingX * scale;
    const float fieldHeight = kDesignFieldHeight * scale;
    const float innerWidth = std::max(0.0f, cardWidth - paddingX * 2.0f);
    const float left = geo.card.x + paddingX;

    const auto rowAt = [&geo, left, innerWidth, scale](float designTop, float designHeight) {
        return layout::Rect{left, geo.card.y + designTop * scale, innerWidth, designHeight * scale};
    };

    geo.title = rowAt(kDesignTitleTop, kDesignTitleHeight);
    geo.username = rowAt(kDesignFieldTop, kDesignFieldHeight);
    geo.password = rowAt(kDesignFieldTop + kDesignFieldStep, kDesignFieldHeight);

    const float codeWidth = innerWidth * kDesignCodeWidthRatio;
    const float codeGap = kDesignCodeGap * scale;
    const float codeTop = kDesignFieldTop + kDesignFieldStep * 2.0f;
    geo.code = layout::Rect{left, geo.card.y + codeTop * scale, codeWidth, fieldHeight};
    geo.captcha = layout::Rect{geo.code.Right() + codeGap, geo.code.y,
                               std::max(0.0f, innerWidth - codeWidth - codeGap), fieldHeight};

    geo.rememberRow = rowAt(kDesignRememberTop, kDesignRememberHeight);
    const float checkboxSize = kDesignCheckboxSize * scale;
    geo.checkbox = layout::Rect{left,
                                geo.rememberRow.y + (geo.rememberRow.h - checkboxSize) * 0.5f,
                                checkboxSize, checkboxSize};

    geo.button = rowAt(kDesignButtonTop, kDesignFieldHeight);
    geo.error = layout::Rect{left, geo.button.Bottom() + kDesignErrorGap * scale, innerWidth,
                             kDesignErrorHeight * scale};

    const float footerHeight = kDesignFooterHeight * scale;
    geo.footer = layout::Rect{viewport.x, viewport.Bottom() - footerHeight, viewport.w,
                             footerHeight};
    return geo;
}

// -----------------------------------------------------------------------------
// 背景：深色渐变 + 两个青绿光斑（Web 背景图的桌面端占位）
// -----------------------------------------------------------------------------
void DrawBackground(ImDrawList* drawList, const layout::Rect& viewport) {
    ui::VerticalGradient(drawList, viewport, kBackgroundTop, kBackgroundBottom);

    const theme::Palette& colors = theme::Colors();
    // 多层低透明度同心圆叠加成柔和光斑：层数越多、单层 alpha 越小，环带越不可见。
    const auto glow = [drawList, &colors](ImVec2 center, float radius, float strength) {
        constexpr int kLayers = 40;
        for (int layer = 0; layer < kLayers; ++layer) {
            const float ratio = 1.0f - static_cast<float>(layer) / static_cast<float>(kLayers);
            drawList->AddCircleFilled(center, radius * ratio,
                                     theme::WithAlpha(colors.primary, strength / kLayers), 64);
        }
    };
    glow(ImVec2(viewport.x + viewport.w * 0.16f, viewport.y + viewport.h * 0.22f),
         viewport.h * 0.42f, 0.62f);
    glow(ImVec2(viewport.x + viewport.w * 0.86f, viewport.y + viewport.h * 0.82f),
         viewport.h * 0.36f, 0.46f);
}

// 输入框左侧「浅色小图标方块」（Web 为 svg 图标；无图标资源，浅色方块占位）
void DrawFieldIcon(ImDrawList* drawList, const LoginGeometry& geo, const layout::Rect& field) {
    const float size = kDesignIconSize * geo.scale;
    const layout::Rect icon{field.x + kDesignIconLeft * geo.scale, field.CenterY() - size * 0.5f,
                            size, size};
    ui::FillRounded(drawList, icon, theme::WithAlpha(theme::Colors().textSecondary, 0.45f),
                    3.0f * geo.scale);
}

// 单个输入框：真实 ImGui InputText + 主题化边框 + 聚焦/错误态描边
// 返回 true 表示按回车提交。
bool DrawInputField(ImDrawList* drawList, const LoginGeometry& geo, const char* id,
                    const layout::Rect& rect, const char* hint, char* buffer, size_t bufferSize,
                    ImGuiInputTextFlags flags, bool invalid) {
    const theme::Palette& colors = theme::Colors();
    const float scale = geo.scale;
    const float fontSize = kDesignBodyFontSize * scale;
    const float rounding = 4.0f * scale;
    const float framePaddingY = std::max(0.0f, (rect.h - fontSize) * 0.5f);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(kDesignTextLeft * scale, framePaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(colors.panel));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(colors.panel));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::ColorConvertU32ToFloat4(colors.panel));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ImGui::ColorConvertU32ToFloat4(invalid ? colors.danger : kFieldBorder));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,
                          ImGui::ColorConvertU32ToFloat4(colors.textPlaceholder));

    ImGui::SetCursorScreenPos(ui::Min(rect));
    ImGui::SetNextItemWidth(rect.w);

    bool submittedByEnter = false;
    {
        ui::ScopedFontSize fieldFont(fontSize);
        submittedByEnter = ImGui::InputTextWithHint(id, hint, buffer, bufferSize, flags);

        // ImGui 没有「聚焦边框色」选项：这里自绘 1.5px 描边覆盖（聚焦青绿 / 错误红）。
        const bool focused = ImGui::IsItemActive();
        if (focused || invalid) {
            drawList->AddRect(ui::Min(rect), ui::Max(rect),
                              invalid ? colors.danger : colors.primary, rounding, 0, 1.5f);
        }
        DrawFieldIcon(drawList, geo, rect);
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
    return submittedByEnter;
}

// 验证码图形占位：4 位字符 + 干扰线，点击刷新
void DrawCaptchaBox(ImDrawList* drawList, const LoginGeometry& geo, const layout::Rect& rect,
                    const char* text, bool* clicked) {
    const theme::Palette& colors = theme::Colors();
    const float scale = geo.scale;
    const float rounding = 4.0f * scale;

    *clicked = ui::InvisibleButtonAt("##login_captcha", rect);
    const bool hovered = ImGui::IsItemHovered();

    ui::FillRounded(drawList, rect, colors.lightBlock, rounding);
    ui::StrokeRounded(drawList, rect, hovered ? colors.primary : kFieldBorder, rounding, 1.0f);

    drawList->PushClipRect(ui::Min(rect), ui::Max(rect), true);
    // 干扰线：模拟图形验证码的噪点（纯装饰，不参与校验）
    const ImU32 noiseColor = theme::WithAlpha(colors.link, 0.14f);
    const float lineWidth = std::max(1.0f, scale);
    drawList->AddLine(ImVec2(rect.x + rect.w * 0.08f, rect.y + rect.h * 0.30f),
                      ImVec2(rect.x + rect.w * 0.44f, rect.y + rect.h * 0.74f), noiseColor,
                      lineWidth);
    drawList->AddLine(ImVec2(rect.x + rect.w * 0.34f, rect.y + rect.h * 0.18f),
                      ImVec2(rect.x + rect.w * 0.92f, rect.y + rect.h * 0.62f), noiseColor,
                      lineWidth);
    drawList->AddLine(ImVec2(rect.x + rect.w * 0.52f, rect.y + rect.h * 0.82f),
                      ImVec2(rect.x + rect.w * 0.95f, rect.y + rect.h * 0.34f), noiseColor,
                      lineWidth);
    {
        ui::ScopedBoldFont captchaFont(kDesignCaptchaFontSize * scale);
        ui::TextCentered(drawList, rect, colors.link, text);
    }
    drawList->PopClipRect();

    if (hovered) {
        ImGui::SetTooltip("点击刷新验证码");
    }
}

// 记住密码：自绘 Element 风格复选框，整行可点击
void DrawRememberRow(ImDrawList* drawList, const LoginGeometry& geo, bool checked,
                     bool* clicked) {
    const theme::Palette& colors = theme::Colors();
    const float scale = geo.scale;
    const float rounding = 2.0f * scale;

    *clicked = ui::InvisibleButtonAt("##login_remember", geo.rememberRow);
    const bool hovered = ImGui::IsItemHovered();

    if (checked) {
        ui::FillRounded(drawList, geo.checkbox, colors.primary, rounding);
        const ImVec2 p0(geo.checkbox.x + geo.checkbox.w * 0.24f,
                        geo.checkbox.y + geo.checkbox.h * 0.52f);
        const ImVec2 p1(geo.checkbox.x + geo.checkbox.w * 0.44f,
                        geo.checkbox.y + geo.checkbox.h * 0.72f);
        const ImVec2 p2(geo.checkbox.x + geo.checkbox.w * 0.78f,
                        geo.checkbox.y + geo.checkbox.h * 0.30f);
        const float thickness = std::max(1.5f, 1.8f * scale);
        drawList->AddLine(p0, p1, colors.textInverse, thickness);
        drawList->AddLine(p1, p2, colors.textInverse, thickness);
    } else {
        ui::FillRounded(drawList, geo.checkbox, colors.panel, rounding);
        ui::StrokeRounded(drawList, geo.checkbox, hovered ? colors.primary : kFieldBorder,
                          rounding, 1.0f);
    }

    {
        ui::ScopedFontSize rememberFont(kDesignBodyFontSize * scale);
        const float gap = 8.0f * scale;
        const float labelLeft = geo.checkbox.Right() + gap;
        const layout::Rect label{labelLeft, geo.rememberRow.y,
                                 std::max(0.0f, geo.rememberRow.Right() - labelLeft),
                                 geo.rememberRow.h};
        ui::TextLeftMiddle(drawList, label, hovered ? colors.textPrimary : colors.textRegular,
                           "记住密码");
    }
}

// 登录按钮：自绘主色按钮（hover/active/loading 三态），整块热区可点击
void DrawLoginButton(ImDrawList* drawList, const LoginGeometry& geo, bool loading, bool* clicked) {
    const theme::Palette& colors = theme::Colors();
    const float scale = geo.scale;
    const float rounding = 4.0f * scale;

    *clicked = ui::InvisibleButtonAt("##login_submit", geo.button);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImU32 fill = colors.primary;
    if (loading) {
        fill = theme::WithAlpha(colors.primary, 0.65f);
    } else if (held && hovered) {
        fill = colors.primaryActive;
    } else if (hovered) {
        fill = colors.primaryHover;
    }
    ui::FillRounded(drawList, geo.button, fill, rounding);

    {
        ui::ScopedFontSize buttonFont(kDesignBodyFontSize * scale);
        ui::TextCentered(drawList, geo.button, colors.textInverse,
                         loading ? "登 录 中..." : "登 录");
    }
    if (loading) {
        *clicked = false;  // 提交中忽略重复点击
    }
}

// 错误提示：按钮下方居中红字（超宽时裁到卡片内，不溢出）
void DrawErrorText(ImDrawList* drawList, const LoginGeometry& geo, const char* message) {
    if (message == nullptr || message[0] == '\0') {
        return;
    }
    drawList->PushClipRect(ui::Min(geo.card), ui::Max(geo.card), true);
    {
        ui::ScopedFontSize errorFont(kDesignHintFontSize * geo.scale);
        ui::TextCentered(drawList, geo.error, theme::Colors().danger, message);
    }
    drawList->PopClipRect();
}

// 页脚版权（12px 白字，窗口底部居中）
void DrawFooter(ImDrawList* drawList, const LoginGeometry& geo) {
    ui::ScopedFontSize footerFont(kDesignFooterFontSize * geo.scale);
    ui::TextCentered(drawList, geo.footer,
                     theme::WithAlpha(theme::Colors().textInverse, 0.88f),
                     "Copyright © 2018-2025 ruoyi.vip All Rights Reserved.");
}

// 点击刷新验证码：在预置字符序列中循环（保证每次点击字符必然变化）
void CycleCaptcha(LoginFormState& form) {
    form.captchaIndex =
        (form.captchaIndex + 1) % static_cast<int>(IM_ARRAYSIZE(kCaptchaPool));
    std::snprintf(form.captcha, sizeof(form.captcha), "%s", kCaptchaPool[form.captchaIndex]);
}

// -----------------------------------------------------------------------------
// 登录页渲染系统（Render 阶段 · order 200）：只读资源 + 绘制 + 交互采集
// -----------------------------------------------------------------------------
class LoginPageRenderSystem final : public ISystem {
public:
    const char* Name() const override { return "LoginPageRenderSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& /*frame*/) override {
        const RouteState* route = world.TryResource<RouteState>();
        if (route == nullptr || route->current != Route::Login) {
            m_visible = false;
            return;
        }
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        LoginFormState* form = world.TryResource<LoginFormState>();
        if (shell == nullptr || form == nullptr) {
            return;
        }
        if (shell->viewport.Empty()) {
            return;
        }
        if (!m_visible) {
            // 每次进入登录页：重置提交态（错误提示只在提交后显示）
            m_visible = true;
            form->loading = false;
            form->submitted = false;
        }

        const layout::Rect viewport = shell->viewport;
        const LoginGeometry geo = ComputeLoginGeometry(viewport);

        // ---- 错误提示：读 AuthFeedback（只读），按文案归因到具体输入框 ----
        const AuthFeedback* feedback = world.TryResource<AuthFeedback>();
        bool usernameInvalid = false;
        bool passwordInvalid = false;
        bool codeInvalid = false;
        const char* errorMessage = "";
        if (form->submitted && feedback != nullptr && feedback->hasError &&
            !feedback->message.empty()) {
            errorMessage = feedback->message.c_str();
            const std::string& message = feedback->message;
            usernameInvalid = message.find("账号") != std::string::npos;
            passwordInvalid = message.find("密码") != std::string::npos;
            codeInvalid = message.find("验证码") != std::string::npos;
            if (!usernameInvalid && !passwordInvalid && !codeInvalid) {
                usernameInvalid = true;
                passwordInvalid = true;
            }
        }

        // ---- 背景（背景绘制清单：始终位于所有窗口之下）----
        DrawBackground(ImGui::GetBackgroundDrawList(), viewport);

        // ---- 卡片 + 表单控件（固定窗口承载 InputText / 按钮交互）----
        bool submit = false;
        const bool open = ui::BeginFixedWindow("##login_page", viewport);
        if (open) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // 白卡（圆角取 LayoutTokens::loginCardRadius，与 Web 登录卡 6px 一致）
            ui::FillRounded(drawList, geo.card, theme::Colors().panel,
                            shell->tokens.loginCardRadius);

            {
                ui::ScopedFontSize titleFont(kDesignTitleFontSize * geo.scale);
                ui::TextCentered(drawList, geo.title, kTitleText, "中州养老");
            }

            submit |= DrawInputField(drawList, geo, "##login_username", geo.username, "账号",
                                     form->username, IM_ARRAYSIZE(form->username),
                                     ImGuiInputTextFlags_EnterReturnsTrue, usernameInvalid);
            submit |= DrawInputField(drawList, geo, "##login_password", geo.password, "密码",
                                     form->password, IM_ARRAYSIZE(form->password),
                                     ImGuiInputTextFlags_Password |
                                         ImGuiInputTextFlags_EnterReturnsTrue,
                                     passwordInvalid);
            submit |= DrawInputField(drawList, geo, "##login_code", geo.code, "验证码",
                                     form->code, IM_ARRAYSIZE(form->code),
                                     ImGuiInputTextFlags_EnterReturnsTrue, codeInvalid);

            bool captchaClicked = false;
            DrawCaptchaBox(drawList, geo, geo.captcha, form->captcha, &captchaClicked);
            if (captchaClicked) {
                CycleCaptcha(*form);
            }

            bool rememberClicked = false;
            DrawRememberRow(drawList, geo, form->rememberMe, &rememberClicked);
            if (rememberClicked) {
                form->rememberMe = !form->rememberMe;
            }

            bool loginClicked = false;
            DrawLoginButton(drawList, geo, form->loading, &loginClicked);
            submit |= loginClicked;

            DrawErrorText(drawList, geo, errorMessage);
            DrawFooter(drawList, geo);
        }
        ui::EndFixedWindow();

        // ---- 提交：只入队 Login 命令，不直接改 AuthSession / RouteState ----
        if (submit && !form->loading) {
            form->loading = true;
            form->submitted = true;
            if (CommandQueue* queue = world.TryResource<CommandQueue>()) {
                CommandQueue::Command command;
                command.type = CommandQueue::Type::Login;
                command.arg0 = form->username;
                command.arg1 = form->password;
                command.arg2 = form->code;
                queue->commands.push_back(command);
            }
        }
    }

private:
    bool m_visible = false;
};

// -----------------------------------------------------------------------------
// 登录认证系统（Simulation 阶段 · order 20）：消费 Login 命令
//
// 职责（应用层，不渲染）：
//  1) 表单必填校验（与 Web login.vue 的 rules 一致，文案同 MockAuthService）；
//  2) 调数据服务 IAuthService::Login（Mock，后续换 MySQL / SQLite 实现不改这里）；
//  3) 写 AuthFeedback / AuthSession，成功后切 Route::Dashboard。
// -----------------------------------------------------------------------------
std::string ValidateLoginCommand(const CommandQueue::Command& command) {
    if (command.arg0.empty()) {
        return "请输入您的账号";
    }
    if (command.arg1.empty()) {
        return "请输入您的密码";
    }
    if (command.arg2.empty()) {
        return "请输入验证码";
    }
    if (command.arg1.size() < 6) {
        return "密码长度不能少于 6 位（演示密码 admin123）";
    }
    return std::string();
}

class AuthSystem final : public ISystem {
public:
    const char* Name() const override { return "AuthSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Simulation; }

    void Update(World& world, const FrameContext& /*frame*/) override {
        CommandQueue* queue = world.TryResource<CommandQueue>();
        if (queue == nullptr) {
            return;
        }
        LoginFormState* form = world.TryResource<LoginFormState>();
        AuthFeedback* feedback = world.TryResource<AuthFeedback>();
        DataServices* services = world.TryResource<DataServices>();

        for (const CommandQueue::Command& command : queue->commands) {
            if (command.type != CommandQueue::Type::Login) {
                continue;
            }
            // Mock 登录是同步的：收到命令即结束 loading 态
            if (form != nullptr) {
                form->loading = false;
            }

            const std::string invalid = ValidateLoginCommand(command);
            if (!invalid.empty()) {
                if (feedback != nullptr) {
                    feedback->hasError = true;
                    feedback->message = invalid;
                }
                continue;
            }

            if (services == nullptr || !services->auth) {
                if (feedback != nullptr) {
                    feedback->hasError = true;
                    feedback->message = "数据服务未就绪，请稍后重试";
                }
                continue;
            }

            data::LoginRequest request;
            request.username = command.arg0;
            request.password = command.arg1;
            request.code = command.arg2;
            const data::LoginResult result = services->auth->Login(request);

            if (feedback != nullptr) {
                feedback->hasError = !result.ok;
                feedback->message = result.ok ? std::string() : result.message;
            }
            if (!result.ok) {
                continue;
            }

            // 登录系统属应用层：允许直接写会话与路由（架构文档 §3.3 约定）
            if (AuthSession* session = world.TryResource<AuthSession>()) {
                session->loggedIn = true;
                session->user = result.session;
            }
            if (RouteState* routeState = world.TryResource<RouteState>()) {
                routeState->current = Route::Dashboard;
            }
        }
    }
};

}  // namespace

// 模块注册：登录模块专属资源 + 两个系统（Simulation 认证 / Render 绘制）
void RegisterLoginModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<LoginFormState>();
    scheduler.Add(std::make_unique<AuthSystem>(), 20);
    scheduler.Add(std::make_unique<LoginPageRenderSystem>(), 200);
}

}  // namespace zzyl
