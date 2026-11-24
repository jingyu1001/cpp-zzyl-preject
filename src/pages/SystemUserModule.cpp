// =============================================================================
// 系统管理-用户管理（M1）· cpp-dev2 负责实现
//
// 依据：
//  - UI复刻规格_Cpp_v0.2.md §2（列表页骨架）/ §3（通用组件）/ §4.1（用户管理页）
//  - Sprint2_cpp-dev2_用户管理.md（页面结构 / 字段 / 校验 / 交互）
//  - Web 源码 views/system/user/index.vue（7 条 sys_user 种子由 SeedUserService 提供）
//
// 分工：
//  通用组件（搜索栏/工具栏/表格/分页/弹窗/确认框/Toast）由 cpp-dev1 实现，
//  本文件只按 include/zzyl/ui/Widgets.h 契约调用；页面只负责「布局 + 状态 + 数据编排」。
//
// 数据流（与 Sprint 2 架构 v0.2 §2.3 一致）：
//  - 页面私有状态放 World 资源 SystemUserPageState；
//  - 列表数据来自 DataServices.user（IUserService 接口，不 include SeedServices）；
//  - 搜索/翻页/增删改由页面系统同步调用服务后重新 List 刷新（内存种子实现）；
//  - 渲染只读页面状态；不写其它页面的业务资源；导航仍走 CommandQueue（本页不涉及）。
//
// 布局（禁止写死坐标）：
//  所有矩形由 ShellLayout::pageViewport 逐层推导：搜索栏 → 工具栏 → 表格 → 分页，
//  高/宽取自组件辅助函数（SearchBarHeight / ToolbarHeight / PaginationHeight）与
//  UI 规格常量（表头 40 / 行高 49 / 区块间距 16·12）；页面整体纵向滚动。
// =============================================================================

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"

#include "zzyl/app/AppState.h"
#include "zzyl/app/ModuleHooks.h"
#include "zzyl/app/Routes.h"
#include "zzyl/core/System.h"
#include "zzyl/data/BusinessServices.h"
#include "zzyl/render/Texture.h"
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"
#include "zzyl/ui/Widgets.h"

namespace zzyl {
namespace {

// -----------------------------------------------------------------------------
// 设计常量（取 UI复刻规格 v0.2 §2/§3/§4.1 的实测值；只作为布局推导参数，不是屏幕坐标）
// -----------------------------------------------------------------------------
constexpr float kTableHeaderHeight = 40.0f;   // 表头高
constexpr float kTableRowHeight = 49.0f;      // 数据行高
constexpr float kTableEmptyHeight = 120.0f;   // 空态占位高度
constexpr float kSearchToToolbarGap = 16.0f;  // 搜索栏 → 工具栏
constexpr float kSectionGap = 12.0f;          // 工具栏/表格/分页之间
constexpr float kScrollbarReserve = 10.0f;    // 为纵向滚动条预留的宽度
constexpr float kPageBottomPadding = 20.0f;   // 页面底部留白
constexpr int kDefaultPageSize = 10;          // 10 条/页

// 表单弹窗（§4.1：600 宽，双列，每列约 284）
constexpr float kFormLabelWidth = 86.0f;
constexpr float kFormColumnGap = 16.0f;
constexpr float kFormRowGap = 28.0f;
constexpr float kFormControlHeight = 32.0f;
constexpr float kFormErrorGap = 2.0f;

// -----------------------------------------------------------------------------
// 页面私有状态（World 资源）。渲染系统只改这里 + 调数据服务，不碰其它页面资源。
// -----------------------------------------------------------------------------
struct SystemUserPageState {
    bool needRefresh = true;   // 首帧 / 查询条件 / CRUD 后置位，下一帧统一 List
    bool initializedServices = false;

    std::vector<data::UserRecord> allRows;  // 当前查询结果全集
    std::vector<data::UserRecord> rows;     // 当前页切片（表格数据源）
    std::vector<int> checkedIds;            // 表格勾选（组件每帧回写）

    // 搜索栏字段：部门 / 用户名称 / 手机号码 / 状态 / 创建时间
    std::vector<ui::SearchField> searchFields;
    ui::PaginationState pagination;

    // 新增/修改弹窗
    ui::DialogState formDialog;
    bool isEdit = false;
    data::UserRecord editing;  // 编辑时的原始记录（保留 userId / createTime 等）
    char fNickName[64] = {};
    char fDept[64] = {};
    char fPhone[32] = {};
    char fEmail[80] = {};
    char fUserName[64] = {};
    char fPassword[32] = {};
    char fPost[64] = {};
    char fRole[64] = {};
    char fRemark[256] = {};
    int fSexIndex = 2;     // 0 男 / 1 女 / 2 未知
    int fStatusIndex = 0;  // 0 正常 / 1 停用
    bool fShowPassword = false;
    std::string eNickName;
    std::string eUserName;
    std::string ePassword;
    std::string ePhone;
    std::string eEmail;
    std::string formServiceError;  // 服务端返回错误（顶部红字）

    // 重置密码弹窗
    ui::DialogState resetDialog;
    int resetTargetId = 0;
    char resetPassword[32] = {};
    std::string resetError;

    // 删除确认框
    bool confirmDelete = false;
    std::string confirmText;
    std::vector<int> pendingDeleteIds;
};

// -----------------------------------------------------------------------------
// 小工具
// -----------------------------------------------------------------------------
std::string NowText() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
    return std::string(buffer);
}

std::string TextOf(const char* chars) { return chars != nullptr ? std::string(chars) : std::string(); }

void CopyToBuffer(char* buffer, size_t size, const std::string& text) {
    if (buffer == nullptr || size == 0) {
        return;
    }
    std::snprintf(buffer, size, "%s", text.c_str());
}

const char* SexText(int index) {
    switch (index) {
        case 0:
            return "男";
        case 1:
            return "女";
        default:
            return "未知";
    }
}

int SexIndexFromText(const std::string& text) {
    if (text == "男") {
        return 0;
    }
    if (text == "女") {
        return 1;
    }
    return 2;
}

bool IsDigits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

bool IsEmailLike(const std::string& text) {
    const std::size_t at = text.find('@');
    if (at == std::string::npos || at == 0) {
        return false;
    }
    const std::size_t dot = text.find('.', at + 1);
    return dot != std::string::npos && dot + 1 < text.size();
}

// 密码禁止字符（Web 用户管理新增/修改规则）
bool HasForbiddenPasswordChar(const std::string& text) {
    const char* forbidden = "<>\"'\\|";
    return text.find_first_of(forbidden) != std::string::npos;
}

void PushPageToast(World& world, const std::string& text, ui::ToastType type) {
    if (ui::ToastState* toasts = world.TryResource<ui::ToastState>()) {
        ui::PushToast(*toasts, text, type);
    }
}

// 表单控件的输入框样式（弹窗内容由页面绘制，需与 §3.5 规格一致：白底 + #DCDFE6 边框 + 4 圆角）
void PushFormFieldStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 9.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(theme::Colors().panel));
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(theme::Colors().border));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,
                          ImGui::ColorConvertU32ToFloat4(theme::Colors().textPlaceholder));
}

void PopFormFieldStyle() {
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

data::IUserService* UserService(World& world) {
    DataServices* services = world.TryResource<DataServices>();
    return (services != nullptr && services->user) ? services->user.get() : nullptr;
}

// -----------------------------------------------------------------------------
// 查询 / 分页
// -----------------------------------------------------------------------------
void EnsureSearchFields(SystemUserPageState& state, data::IUserService& service,
                        bool reloadOptions) {
    if (!state.searchFields.empty() && !reloadOptions) {
        return;
    }
    // 保持已有输入值（reloadOptions 时只刷新下拉选项，不清空用户输入）
    std::string deptValue;
    std::string statusValue = "全部";
    std::vector<std::string> deptOptions{"全部"};
    if (!state.searchFields.empty()) {
        deptValue = state.searchFields[0].value;
        statusValue = state.searchFields[3].value.empty() ? std::string("全部") : state.searchFields[3].value;
        deptOptions = state.searchFields[0].options;
    }
    for (const std::string& name : service.DeptNames()) {
        if (std::find(deptOptions.begin(), deptOptions.end(), name) == deptOptions.end()) {
            deptOptions.push_back(name);
        }
    }

    state.searchFields.clear();
    state.searchFields.push_back(ui::SearchField{"dept", "部门", deptValue, ui::SearchFieldType::Select,
                                                 deptOptions, 240.0f});
    state.searchFields.push_back(ui::SearchField{"userName", "用户名称", "", ui::SearchFieldType::Text,
                                                 {}, 240.0f});
    state.searchFields.push_back(ui::SearchField{"phone", "手机号码", "", ui::SearchFieldType::Text,
                                                 {}, 240.0f});
    state.searchFields.push_back(ui::SearchField{"status", "状态", statusValue,
                                                 ui::SearchFieldType::Select,
                                                 {"全部", "正常", "停用"}, 240.0f});
    state.searchFields.push_back(ui::SearchField{"createTime", "创建时间", "",
                                                 ui::SearchFieldType::DateRange, {}, 240.0f});
}

data::UserQuery BuildQuery(const SystemUserPageState& state) {
    data::UserQuery query;
    if (state.searchFields.size() >= 5) {
        query.keyword = state.searchFields[1].value;
        query.phone = state.searchFields[2].value;
        const std::string& status = state.searchFields[3].value;
        if (status == "正常") {
            query.status = 1;
        } else if (status == "停用") {
            query.status = 0;
        } else {
            query.status = -1;
        }
        const std::string& dept = state.searchFields[0].value;
        query.deptName = (dept == "全部") ? std::string() : dept;
        // 创建时间：本迭代为日期范围占位（种子服务不参与过滤），保留字符串以便 Sprint 3 接 SQLite
        query.beginTime = state.searchFields[4].value;
    }
    return query;
}

void SlicePage(SystemUserPageState& state) {
    const int pageSize = state.pagination.pageSize > 0 ? state.pagination.pageSize : kDefaultPageSize;
    const int total = static_cast<int>(state.allRows.size());
    const int maxPage = total <= 0 ? 1 : (total + pageSize - 1) / pageSize;
    state.pagination.page = std::min(std::max(state.pagination.page, 1), maxPage);
    const int begin = (state.pagination.page - 1) * pageSize;
    const int end = std::min(begin + pageSize, total);
    state.rows.clear();
    for (int index = begin; index < end; ++index) {
        state.rows.push_back(state.allRows[static_cast<std::size_t>(index)]);
    }
}

void RefreshList(SystemUserPageState& state, data::IUserService& service, bool keepChecked) {
    state.allRows = service.List(BuildQuery(state));
    state.pagination.total = static_cast<int>(state.allRows.size());
    state.pagination.pageSize = state.pagination.pageSize > 0 ? state.pagination.pageSize : kDefaultPageSize;
    SlicePage(state);
    if (!keepChecked) {
        state.checkedIds.clear();
    }
}

// -----------------------------------------------------------------------------
// 弹窗打开 / 表单校验 / 提交
// -----------------------------------------------------------------------------
void ClearFormErrors(SystemUserPageState& state) {
    state.eNickName.clear();
    state.eUserName.clear();
    state.ePassword.clear();
    state.ePhone.clear();
    state.eEmail.clear();
    state.formServiceError.clear();
}

void OpenAddDialog(SystemUserPageState& state) {
    state.isEdit = false;
    state.editing = data::UserRecord{};
    state.fNickName[0] = '\0';
    state.fDept[0] = '\0';
    state.fPhone[0] = '\0';
    state.fEmail[0] = '\0';
    state.fUserName[0] = '\0';
    state.fPassword[0] = '\0';
    state.fPost[0] = '\0';
    state.fRole[0] = '\0';
    state.fRemark[0] = '\0';
    state.fSexIndex = 2;
    state.fStatusIndex = 0;
    state.fShowPassword = false;
    ClearFormErrors(state);
    state.formDialog.title = "添加用户";
    state.formDialog.width = 600.0f;
    state.formDialog.open = true;
}

void OpenEditDialog(SystemUserPageState& state, int userId) {
    const auto it = std::find_if(state.allRows.begin(), state.allRows.end(),
                                 [userId](const data::UserRecord& record) {
                                     return record.userId == userId;
                                 });
    if (it == state.allRows.end()) {
        return;
    }
    state.isEdit = true;
    state.editing = *it;
    CopyToBuffer(state.fNickName, sizeof(state.fNickName), it->nickName);
    CopyToBuffer(state.fDept, sizeof(state.fDept), it->deptName);
    CopyToBuffer(state.fPhone, sizeof(state.fPhone), it->phone);
    CopyToBuffer(state.fEmail, sizeof(state.fEmail), it->email);
    CopyToBuffer(state.fUserName, sizeof(state.fUserName), it->userName);
    CopyToBuffer(state.fPassword, sizeof(state.fPassword), std::string());
    CopyToBuffer(state.fPost, sizeof(state.fPost), it->postNames);
    CopyToBuffer(state.fRole, sizeof(state.fRole), it->roleNames);
    CopyToBuffer(state.fRemark, sizeof(state.fRemark), it->remark);
    state.fSexIndex = SexIndexFromText(it->sex);
    state.fStatusIndex = it->enabled ? 0 : 1;
    state.fShowPassword = false;
    ClearFormErrors(state);
    state.formDialog.title = "修改用户";
    state.formDialog.width = 600.0f;
    state.formDialog.open = true;
}

bool ValidateForm(SystemUserPageState& state) {
    ClearFormErrors(state);
    const std::string nickName = TextOf(state.fNickName);
    const std::string userName = TextOf(state.fUserName);
    const std::string password = TextOf(state.fPassword);
    const std::string phone = TextOf(state.fPhone);
    const std::string email = TextOf(state.fEmail);

    if (nickName.empty()) {
        state.eNickName = "请输入用户昵称";
    } else if (nickName.size() > 30) {
        state.eNickName = "用户昵称不能超过 30 个字符";
    }

    if (!state.isEdit) {
        if (userName.empty()) {
            state.eUserName = "请输入用户名称";
        } else if (userName.size() < 2 || userName.size() > 20) {
            state.eUserName = "用户名称长度必须在 2~20 个字符";
        }
        if (password.empty()) {
            state.ePassword = "请输入用户密码";
        } else if (password.size() < 5 || password.size() > 20) {
            state.ePassword = "用户密码长度必须在 5~20 个字符";
        } else if (HasForbiddenPasswordChar(password)) {
            state.ePassword = "用户密码不能包含 < > \" ' \\ | 字符";
        }
    }

    if (!phone.empty() && (phone.size() != 11 || !IsDigits(phone))) {
        state.ePhone = "请输入正确的 11 位手机号码";
    }
    if (!email.empty() && (email.size() > 50 || !IsEmailLike(email))) {
        state.eEmail = "请输入正确的邮箱地址";
    }

    return state.eNickName.empty() && state.eUserName.empty() && state.ePassword.empty() &&
           state.ePhone.empty() && state.eEmail.empty();
}

data::UserRecord BuildFormRecord(const SystemUserPageState& state) {
    data::UserRecord record = state.isEdit ? state.editing : data::UserRecord{};
    record.nickName = TextOf(state.fNickName);
    record.deptName = TextOf(state.fDept);
    record.phone = TextOf(state.fPhone);
    record.email = TextOf(state.fEmail);
    record.sex = SexText(state.fSexIndex);
    record.enabled = state.fStatusIndex == 0;
    record.postNames = TextOf(state.fPost);
    record.roleNames = TextOf(state.fRole);
    record.remark = TextOf(state.fRemark);
    if (!state.isEdit) {
        record.userName = TextOf(state.fUserName);
        record.createTime = NowText();
        record.userId = 0;  // 由数据服务分配
    }
    return record;
}

void SubmitForm(World& world, SystemUserPageState& state) {
    data::IUserService* service = UserService(world);
    if (service == nullptr) {
        state.formServiceError = "数据服务未就绪，请稍后重试";
        return;
    }
    if (!ValidateForm(state)) {
        return;  // 保持弹窗打开，红字提示
    }
    const data::UserRecord record = BuildFormRecord(state);
    std::string error;
    const bool ok = state.isEdit ? service->Update(record, &error) : service->Add(record, &error);
    if (!ok) {
        state.formServiceError = error.empty() ? "保存失败，请稍后重试" : error;
        return;
    }
    PushPageToast(world, state.isEdit ? "修改成功" : "新增成功", ui::ToastType::Success);
    state.formDialog.open = false;
    state.needRefresh = true;
}

// -----------------------------------------------------------------------------
// 行操作
// -----------------------------------------------------------------------------
void OpenDeleteConfirm(SystemUserPageState& state, const std::vector<int>& ids,
                       const std::string& message) {
    state.pendingDeleteIds = ids;
    state.confirmText = message;
    state.confirmDelete = true;
}

void OpenResetPasswordDialog(SystemUserPageState& state, int userId) {
    state.resetTargetId = userId;
    state.resetPassword[0] = '\0';
    state.resetError.clear();
    state.resetDialog.title = "重置密码";
    state.resetDialog.width = 500.0f;
    state.resetDialog.open = true;
}

// -----------------------------------------------------------------------------
// 页面绘制
// -----------------------------------------------------------------------------
class SystemUserPageSystem final : public ISystem {
public:
    const char* Name() const override { return "SystemUserPageSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& frame) override {
        const RouteState* route = world.TryResource<RouteState>();
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        if (route == nullptr || shell == nullptr || route->current != Route::SystemUser) {
            m_visible = false;
            return;
        }
        SystemUserPageState* state = world.TryResource<SystemUserPageState>();
        data::IUserService* service = UserService(world);
        if (state == nullptr || service == nullptr) {
            return;
        }

        // 每次进入页面：重置选择、重新拉取数据（搜索条件保留）
        if (!m_visible) {
            m_visible = true;
            state->needRefresh = true;
            state->checkedIds.clear();
            EnsureSearchFields(*state, *service, true);
        }
        EnsureSearchFields(*state, *service, false);

        if (state->needRefresh) {
            RefreshList(*state, *service, false);
            state->needRefresh = false;
        }

        const layout::Rect pageViewport = shell->pageViewport;
        if (pageViewport.Empty()) {
            return;
        }

        DrawPage(world, *shell, *state, *service);

        // 弹窗 / 确认框 / Toast：页面系统末尾绘制
        DrawFormDialog(world, *state, pageViewport);
        DrawResetPasswordDialog(world, *state, pageViewport);
        DrawDeleteConfirm(world, *state, pageViewport);
        if (ui::ToastState* toasts = world.TryResource<ui::ToastState>()) {
            ui::DrawToasts(shell->viewport, *toasts, frame.deltaTime);
        }
    }

private:
    // ---- 页面主区域：搜索栏 → 工具栏 → 表格 → 分页（整体可纵向滚动）----
    void DrawPage(World& world, const layout::ShellLayout& shell, SystemUserPageState& state,
                  data::IUserService& service) {
        (void)service;
        const layout::Rect pageViewport = shell.pageViewport;
        ImGui::SetNextWindowPos(ui::Min(pageViewport));
        ImGui::SetNextWindowSize(ImVec2(pageViewport.w, pageViewport.h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;
        const bool open = ImGui::Begin("##system_user_page", nullptr, flags);
        if (open) {
            const float scrollY = ImGui::GetScrollY();
            const float contentWidth = std::max(1.0f, pageViewport.w - kScrollbarReserve);

            // 局部坐标（页面左上为原点）→ 屏幕坐标（含滚动偏移）
            const auto localToScreen = [&pageViewport, scrollY](const layout::Rect& local) {
                return layout::Rect{pageViewport.x + local.x, pageViewport.y + local.y - scrollY,
                                    local.w, local.h};
            };

            // ---- 布局推导（全部由 pageViewport + 组件高度函数得到）----
            const float searchHeight = std::max(0.0f, ui::SearchBarHeight(
                                                        static_cast<int>(state.searchFields.size())));
            const layout::Rect searchLocal{0.0f, 0.0f, contentWidth, searchHeight};
            float cursorY = searchLocal.Bottom() + kSearchToToolbarGap;

            const float toolbarHeight = std::max(0.0f, ui::ToolbarHeight());
            const layout::Rect toolbarLocal{0.0f, cursorY, contentWidth, toolbarHeight};
            cursorY = toolbarLocal.Bottom() + kSectionGap;

            const std::size_t rowCount = state.rows.size();
            const float tableHeight =
                kTableHeaderHeight +
                (rowCount > 0 ? static_cast<float>(rowCount) * kTableRowHeight : kTableEmptyHeight);
            const layout::Rect tableLocal{0.0f, cursorY, contentWidth, tableHeight};
            cursorY = tableLocal.Bottom() + kSectionGap;

            const float paginationHeight = std::max(0.0f, ui::PaginationHeight());
            // 分页优先贴内容区底部；内容不足时也跟随表格下方
            const float paginationBottomY = pageViewport.h - paginationHeight - kPageBottomPadding;
            const float paginationY = std::max(cursorY, paginationBottomY);
            const layout::Rect paginationLocal{0.0f, paginationY, contentWidth, paginationHeight};
            const float contentHeight =
                paginationLocal.Bottom() + kPageBottomPadding;

            // ---- 搜索栏 ----
            const ui::SearchBarResult search =
                ui::DrawSearchBar(localToScreen(searchLocal), state.searchFields);
            if (search.search) {
                state.pagination.page = 1;
                state.needRefresh = true;
            } else if (search.reset) {
                ResetSearchFields(state, service);
                state.pagination.page = 1;
                state.needRefresh = true;
            }

            // ---- 工具栏 ----
            std::vector<ui::ToolbarButton> buttons;
            buttons.push_back(ui::ToolbarButton{ui::ToolbarKind::Add, "新增", true});
            buttons.push_back(ui::ToolbarButton{ui::ToolbarKind::Edit, "修改",
                                                state.checkedIds.size() == 1});
            buttons.push_back(ui::ToolbarButton{ui::ToolbarKind::Delete, "删除",
                                                !state.checkedIds.empty()});
            // 导入/导出：二期功能（按钮存在，点击提示）
            buttons.push_back(ui::ToolbarButton{ui::ToolbarKind::Import, "导入", true});
            buttons.push_back(ui::ToolbarButton{ui::ToolbarKind::Export, "导出", true});
            const ui::ToolbarResult toolbar =
                ui::DrawToolbar(localToScreen(toolbarLocal), buttons);
            HandleToolbar(world, state, toolbar.clicked);

            // ---- 表格 ----
            const std::vector<ui::TableColumn> columns = BuildColumns();
            std::vector<ui::TableRow> tableRows = BuildTableRows(state.rows);
            ui::TableDrawOptions options;
            options.showCheckbox = true;
            options.showIndex = false;
            options.actionLabels = {"修改", "删除", "重置密码", "分配角色"};
            const render::TextureCache* textures = world.TryResource<render::TextureCache>();
            const ui::TableResult table =
                ui::DrawDataTable(localToScreen(tableLocal), columns, tableRows, state.checkedIds,
                                  textures, options);
            HandleTable(world, state, table);

            // ---- 分页 ----
            const ui::PaginationResult pagination =
                ui::DrawPagination(localToScreen(paginationLocal), state.pagination);
            if (pagination.changed) {
                SlicePage(state);
                state.checkedIds.clear();
            }

            // 撑起滚动范围（组件用绝对坐标绘制，不参与内容尺寸计算）
            ImGui::SetCursorPos(ImVec2(0.0f, contentHeight));
            ImGui::Dummy(ImVec2(1.0f, 1.0f));
            (void)contentHeight;
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ResetSearchFields(SystemUserPageState& state, data::IUserService& service) {
        state.searchFields.clear();
        EnsureSearchFields(state, service, true);
    }

    std::vector<ui::TableColumn> BuildColumns() const {
        std::vector<ui::TableColumn> columns;
        columns.push_back(ui::TableColumn{"userId", "用户编号", 80.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(ui::TableColumn{"userName", "用户名称", 110.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(ui::TableColumn{"nickName", "用户昵称", 100.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(ui::TableColumn{"deptName", "部门", 110.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(ui::TableColumn{"phone", "手机号码", 120.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(ui::TableColumn{"status", "状态", 80.0f, ui::Align::Center, ui::CellKind::Switch});
        columns.push_back(ui::TableColumn{"createTime", "创建时间", 160.0f, ui::Align::Center, ui::CellKind::Text});
        columns.push_back(ui::TableColumn{"actions", "操作", 200.0f, ui::Align::Center, ui::CellKind::Actions});
        return columns;
    }

    std::vector<ui::TableRow> BuildTableRows(const std::vector<data::UserRecord>& records) const {
        std::vector<ui::TableRow> rows;
        rows.reserve(records.size());
        for (const data::UserRecord& record : records) {
            ui::TableRow row;
            row.id = record.userId;
            row.enabled = record.enabled;
            row.cells.push_back(std::to_string(record.userId));
            row.cells.push_back(record.userName);
            row.cells.push_back(record.nickName);
            row.cells.push_back(record.deptName);
            row.cells.push_back(record.phone);
            row.cells.push_back(record.enabled ? "正常" : "停用");
            row.cells.push_back(record.createTime);
            row.cells.push_back("修改|删除|重置密码|分配角色");
            rows.push_back(std::move(row));
        }
        return rows;
    }

    void HandleToolbar(World& world, SystemUserPageState& state, int clicked) {
        switch (clicked) {
            case 0:  // 新增
                OpenAddDialog(state);
                break;
            case 1:  // 修改（需勾选 1 条）
                if (state.checkedIds.size() == 1) {
                    OpenEditDialog(state, state.checkedIds.front());
                }
                break;
            case 2:  // 批量删除
                if (!state.checkedIds.empty()) {
                    OpenDeleteConfirm(state, state.checkedIds,
                                      "确定删除选中的 " + std::to_string(state.checkedIds.size()) +
                                          " 个用户吗？");
                }
                break;
            case 3:  // 导入（二期）
                PushPageToast(world, "导入功能二期支持", ui::ToastType::Warning);
                break;
            case 4:  // 导出（二期）
                PushPageToast(world, "导出功能二期支持", ui::ToastType::Warning);
                break;
            default:
                break;
        }
    }

    void HandleTable(World& world, SystemUserPageState& state, const ui::TableResult& table) {
        data::IUserService* service = UserService(world);
        if (service == nullptr) {
            return;
        }

        // 状态开关：调用 SetStatus → Toast → 刷新
        for (const ui::TableResult::SwitchToggle& toggle : table.switchToggles) {
            std::string error;
            if (service->SetStatus(toggle.rowId, toggle.enabled, &error)) {
                PushPageToast(world, "修改成功", ui::ToastType::Success);
                state.needRefresh = true;
            } else {
                PushPageToast(world, error.empty() ? "修改状态失败" : error, ui::ToastType::Error);
            }
        }

        // 操作列（修改 / 删除 / 重置密码 / 分配角色）
        if (table.clickedAction >= 0 && table.actionRowId > 0) {
            switch (table.clickedAction) {
                case 0:
                    OpenEditDialog(state, table.actionRowId);
                    break;
                case 1:
                    OpenDeleteConfirm(state, {table.actionRowId},
                                      "确定删除该用户吗？");
                    break;
                case 2:
                    OpenResetPasswordDialog(state, table.actionRowId);
                    break;
                case 3:
                    PushPageToast(world, "跳转分配角色（二期）", ui::ToastType::Warning);
                    break;
                default:
                    break;
            }
        }
    }

    // ---- 新增/修改弹窗（只画内容区；标题栏与底部按钮由组件负责）----
    void DrawFormDialog(World& world, SystemUserPageState& state,
                        const layout::Rect& pageArea) {
        if (!state.formDialog.open) {
            return;
        }
        if (ui::BeginFormDialog(pageArea, state.formDialog)) {
            DrawFormContent(world, state);
        }
        const ui::DialogResult result = ui::EndFormDialog();
        if (result == ui::DialogResult::Ok) {
            SubmitForm(world, state);
        }
    }

    void DrawFormContent(World& world, SystemUserPageState& state) {
        data::IUserService* service = UserService(world);
        const float totalWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x - 8.0f);
        if (totalWidth <= 0.0f) {
            return;
        }
        const float columnWidth = (totalWidth - kFormColumnGap) * 0.5f;
        const float inputWidth = std::max(80.0f, columnWidth - kFormLabelWidth - 8.0f);
        const float originX = ImGui::GetCursorPosX();
        float cursorY = ImGui::GetCursorPosY();
        PushFormFieldStyle();

        const auto drawLabel = [&](float x, const char* label, bool required) {
            ImGui::SetCursorPos(ImVec2(x, cursorY));
            const float labelRight = x + kFormLabelWidth;
            const char* star = required ? "*" : "";
            const float starWidth = ImGui::CalcTextSize(star).x;
            const float textWidth = ImGui::CalcTextSize(label).x;
            ImGui::SetCursorPosX(labelRight - starWidth - textWidth);
            if (required) {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "*");
                ImGui::SameLine(0.0f, 0.0f);
            }
            ImGui::TextUnformatted(label);
        };

        // 单个输入控件（返回输入是否被编辑）
        const auto drawInput = [&](int column, const char* label, bool required, char* buffer,
                                   size_t bufferSize, ImGuiInputTextFlags flags,
                                   const std::string& error) {
            const float x = originX + static_cast<float>(column) * (columnWidth + kFormColumnGap);
            drawLabel(x, label, required);
            ImGui::SetCursorPos(ImVec2(x + kFormLabelWidth + 8.0f, cursorY));
            ImGui::SetNextItemWidth(inputWidth);
            {
                ui::ScopedFontSize font(14.0f);
                ImGui::InputText((std::string("##") + label).c_str(), buffer, bufferSize, flags);
            }
            if (!error.empty()) {
                ImGui::SetCursorPos(ImVec2(x + kFormLabelWidth + 8.0f, cursorY + kFormControlHeight));
                ui::ScopedFontSize errorFont(12.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                                   error.c_str());
            }
        };

        // 单行两个字段
        const auto drawRow = [&](const char* labelLeft, bool requiredLeft, char* bufferLeft,
                                 size_t sizeLeft, ImGuiInputTextFlags flagsLeft,
                                 const std::string& errorLeft, const char* labelRight,
                                 bool requiredRight, char* bufferRight, size_t sizeRight,
                                 ImGuiInputTextFlags flagsRight, const std::string& errorRight) {
            float rowHeight = kFormControlHeight;
            if (!errorLeft.empty() || !errorRight.empty()) {
                rowHeight += 16.0f;
            }
            drawInput(0, labelLeft, requiredLeft, bufferLeft, sizeLeft, flagsLeft, errorLeft);
            drawInput(1, labelRight, requiredRight, bufferRight, sizeRight, flagsRight, errorRight);
            cursorY += rowHeight + kFormRowGap;
        };

        // 服务端错误（顶部红字）
        if (!state.formServiceError.empty()) {
            ImGui::SetCursorPos(ImVec2(originX, cursorY));
            ui::ScopedFontSize errorFont(13.0f);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                               state.formServiceError.c_str());
            cursorY += 22.0f;
        }

        // 第一行：用户昵称* / 归属部门
        {
            drawInput(0, "用户昵称", true, state.fNickName, sizeof(state.fNickName), 0,
                      state.eNickName);
            const float x = originX + columnWidth + kFormColumnGap;
            float rowHeight = kFormControlHeight;
            if (!state.eNickName.empty()) {
                rowHeight += 16.0f;
            }
            drawLabel(x, "归属部门", false);
            ImGui::SetCursorPos(ImVec2(x + kFormLabelWidth + 8.0f, cursorY));
            ImGui::SetNextItemWidth(inputWidth);
            DrawDeptCombo(service, state);
            cursorY += rowHeight + kFormRowGap;
        }

        // 第二行：手机号码 / 邮箱
        drawRow("手机号码", false, state.fPhone, sizeof(state.fPhone),
                ImGuiInputTextFlags_CharsDecimal, state.ePhone, "邮箱", false, state.fEmail,
                sizeof(state.fEmail), 0, state.eEmail);

        // 第三行：用户名称* / 用户密码*（仅新增显示）
        if (!state.isEdit) {
            drawInput(0, "用户名称", true, state.fUserName, sizeof(state.fUserName),
                      ImGuiInputTextFlags_CharsNoBlank, state.eUserName);
            // 密码 + 显隐切换
            const float x = originX + columnWidth + kFormColumnGap;
            drawLabel(x, "用户密码", true);
            ImGui::SetCursorPos(ImVec2(x + kFormLabelWidth + 8.0f, cursorY));
            const float showButtonWidth = 40.0f;
            ImGui::SetNextItemWidth(std::max(60.0f, inputWidth - showButtonWidth - 6.0f));
            ImGuiInputTextFlags passwordFlags = state.fShowPassword ? 0 : ImGuiInputTextFlags_Password;
            {
                ui::ScopedFontSize font(14.0f);
                ImGui::InputText("##user_password", state.fPassword, sizeof(state.fPassword),
                                 passwordFlags);
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ImGui::Button(state.fShowPassword ? "隐藏" : "显示",
                              ImVec2(showButtonWidth, kFormControlHeight))) {
                state.fShowPassword = !state.fShowPassword;
            }
            if (!state.ePassword.empty()) {
                ImGui::SetCursorPos(ImVec2(x + kFormLabelWidth + 8.0f, cursorY + kFormControlHeight));
                ui::ScopedFontSize errorFont(12.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                                   state.ePassword.c_str());
            }
            float rowHeight = kFormControlHeight;
            if (!state.eUserName.empty() || !state.ePassword.empty()) {
                rowHeight += 16.0f;
            }
            cursorY += rowHeight + kFormRowGap;
        }

        // 第四行：用户性别 / 状态
        {
            const float x0 = originX;
            drawLabel(x0, "用户性别", false);
            ImGui::SetCursorPos(ImVec2(x0 + kFormLabelWidth + 8.0f, cursorY));
            ImGui::SetNextItemWidth(inputWidth);
            ImGui::Combo("##user_sex", &state.fSexIndex, "男\0女\0未知\0");
            const float x1 = originX + columnWidth + kFormColumnGap;
            drawLabel(x1, "状态", false);
            ImGui::SetCursorPos(ImVec2(x1 + kFormLabelWidth + 8.0f, cursorY));
            ui::ScopedFontSize font(14.0f);
            ImGui::RadioButton("正常", &state.fStatusIndex, 0);
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::RadioButton("停用", &state.fStatusIndex, 1);
            cursorY += kFormControlHeight + kFormRowGap;
        }

        // 第五行：岗位 / 角色
        drawRow("岗位", false, state.fPost, sizeof(state.fPost), 0, "", "角色", false,
                state.fRole, sizeof(state.fRole), 0, "");

        // 第六行：备注（整行）
        {
            drawLabel(originX, "备注", false);
            ImGui::SetCursorPos(ImVec2(originX + kFormLabelWidth + 8.0f, cursorY));
            ImGui::SetNextItemWidth(totalWidth - kFormLabelWidth - 8.0f);
            {
                ui::ScopedFontSize font(14.0f);
                ImGui::InputTextMultiline("##user_remark", state.fRemark, sizeof(state.fRemark),
                                          ImVec2(totalWidth - kFormLabelWidth - 8.0f, 64.0f));
            }
            cursorY += 64.0f + kFormRowGap;
        }

        // 收尾：把内联光标推到内容末尾（组件据此摆放 footer）
        ImGui::SetCursorPos(ImVec2(originX, cursorY));
        PopFormFieldStyle();
    }

    void DrawDeptCombo(data::IUserService* service, SystemUserPageState& state) {
        // 归属部门：数据源为服务部门列表；保持可自由输入（种子服务存字符串）
        std::vector<std::string> options;
        if (service != nullptr) {
            options = service->DeptNames();
        }
        if (ImGui::BeginCombo("##user_dept", state.fDept[0] == '\0' ? "请选择归属部门" : state.fDept)) {
            for (const std::string& option : options) {
                const bool selected = TextOf(state.fDept) == option;
                if (ImGui::Selectable(option.c_str(), selected)) {
                    CopyToBuffer(state.fDept, sizeof(state.fDept), option);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    // ---- 重置密码弹窗 ----
    void DrawResetPasswordDialog(World& world, SystemUserPageState& state,
                                 const layout::Rect& pageArea) {
        if (!state.resetDialog.open) {
            return;
        }
        if (ui::BeginFormDialog(pageArea, state.resetDialog)) {
            const float totalWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x - 8.0f);
            const float labelWidth = 86.0f;
            const float inputWidth = std::max(120.0f, totalWidth - labelWidth - 8.0f);
            const float originX = ImGui::GetCursorPosX();
            float cursorY = ImGui::GetCursorPosY();
            PushFormFieldStyle();

            ImGui::SetCursorPos(ImVec2(originX + labelWidth - ImGui::CalcTextSize("新密码").x,
                                       cursorY));
            ImGui::TextUnformatted("新密码");
            ImGui::SetCursorPos(ImVec2(originX + labelWidth + 8.0f, cursorY));
            ImGui::SetNextItemWidth(inputWidth);
            {
                ui::ScopedFontSize font(14.0f);
                ImGui::InputText("##reset_password", state.resetPassword,
                                 sizeof(state.resetPassword), ImGuiInputTextFlags_Password);
            }
            if (!state.resetError.empty()) {
                ImGui::SetCursorPos(ImVec2(originX + labelWidth + 8.0f,
                                           cursorY + kFormControlHeight + kFormErrorGap));
                ui::ScopedFontSize errorFont(12.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                                   state.resetError.c_str());
                cursorY += 18.0f;
            }
            ImGui::SetCursorPos(ImVec2(originX, cursorY + kFormControlHeight + kFormRowGap));
            PopFormFieldStyle();
        }
        const ui::DialogResult result = ui::EndFormDialog();
        if (result != ui::DialogResult::Ok) {
            return;
        }
        const std::string password = TextOf(state.resetPassword);
        if (password.size() < 5 || password.size() > 20) {
            state.resetError = "密码长度必须为 5~20 个字符";
            return;
        }
        data::IUserService* service = UserService(world);
        if (service == nullptr) {
            state.resetError = "数据服务未就绪，请稍后重试";
            return;
        }
        std::string error;
        if (!service->ResetPassword(state.resetTargetId, password, &error)) {
            state.resetError = error.empty() ? "重置密码失败" : error;
            return;
        }
        PushPageToast(world, "重置密码成功", ui::ToastType::Success);
        state.resetDialog.open = false;
    }

    // ---- 删除确认框 ----
    void DrawDeleteConfirm(World& world, SystemUserPageState& state,
                           const layout::Rect& pageArea) {
        if (!state.confirmDelete) {
            return;
        }
        const ui::ConfirmAction action =
            ui::DrawConfirmDialog(pageArea, state.confirmDelete, "系统提示", state.confirmText);
        if (action == ui::ConfirmAction::Cancel) {
            state.confirmDelete = false;
            state.pendingDeleteIds.clear();
            return;
        }
        if (action != ui::ConfirmAction::Ok) {
            return;
        }
        data::IUserService* service = UserService(world);
        std::string error;
        if (service == nullptr) {
            error = "数据服务未就绪，请稍后重试";
        } else if (!service->Remove(state.pendingDeleteIds, &error)) {
            if (error.empty()) {
                error = "删除失败，请稍后重试";
            }
        } else {
            PushPageToast(world, "删除成功", ui::ToastType::Success);
            state.needRefresh = true;
            state.confirmDelete = false;
            state.pendingDeleteIds.clear();
            return;
        }
        PushPageToast(world, error, ui::ToastType::Error);
        state.confirmDelete = false;
        state.pendingDeleteIds.clear();
    }

    bool m_visible = false;
};

}  // namespace

void RegisterSystemUserModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<SystemUserPageState>();
    scheduler.Add(std::make_unique<SystemUserPageSystem>(), 200);
}

}  // namespace zzyl
