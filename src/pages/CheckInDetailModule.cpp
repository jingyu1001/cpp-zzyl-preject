// =============================================================================
// M4 入住详情 · cpp-dev2
//
// 依据：
//  - Web《Sprint3_M4入住办理规格_v0.1.md》§3/§5.3（与办理表单同构的 4 分区；只读展示，
//    图片 100×100、入住床位显示文本、合同显示「{老人姓名}的入住合同.pdf [查看]」，只读隐藏提交）
//  - Sprint3_cpp-dev2_M4办理入住与详情.md；架构 v0.3 §3.4
//
// 数据流（渲染与数据分离）：
//  - 进入页面：读 CheckInSelection.id → DataServices.checkIn->Get(id, &record)；
//    未从列表选择（id==0）时回退展示第 1 条示例并给出提示；
//  - 只读态：分区展示；「编辑」→ 可编辑态（缓冲字段）→「保存」→ Update → Toast → 回只读；
//  - 返回列表 / 保存成功后导航统一入队 CommandQueue{Navigate /enterQuit/checkIn}。
//
// 布局（禁止写死坐标）：页面级纵向滚动；分区/两列/label 100/控件 32 全部由
// ShellLayout::pageViewport 逐层推导（与办理表单同一套公式）。
// =============================================================================

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
// 设计常量（与 CheckInFormModule.cpp 同一套基准）
// -----------------------------------------------------------------------------
constexpr float kLabelWidth = 100.0f;
constexpr float kControlHeight = 32.0f;
constexpr float kRowGap = 28.0f;
constexpr float kRowStep = kControlHeight + kRowGap;
constexpr float kSectionPaddingX = 20.0f;
constexpr float kSectionTitleHeight = 26.0f;
constexpr float kSectionTitleGap = 12.0f;
constexpr float kFirstRowOffset = kSectionTitleHeight + kSectionTitleGap;
constexpr float kSectionGap = 24.0f;
constexpr float kLabelControlGap = 8.0f;
constexpr float kColumnGap = 24.0f;
constexpr float kFamilyHeaderHeight = 50.0f;
constexpr float kFamilyRowHeight = 50.0f;
constexpr float kScrollbarReserve = 12.0f;
constexpr float kPageBottomPadding = 24.0f;
constexpr float kFooterHeight = 32.0f;
constexpr float kAddressBlockHeight = 94.0f;
constexpr float kImageSize = 100.0f;        // 详情图片 100×100（§5.3）
constexpr float kImageBlockHeight = 116.0f;  // 100 图 + 16 空隙
constexpr float kUploadButtonWidth = 88.0f;
constexpr float kUploadButtonHeight = 24.0f;

const char* KinshipText(int kinship) {
    switch (kinship) {
        case 0:
            return "子女";
        case 1:
            return "配偶";
        case 2:
            return "亲属";
        case 3:
            return "朋友";
        case 4:
            return "社工";
        default:
            return "其他";
    }
}
constexpr int kKinshipCount = 6;

// -----------------------------------------------------------------------------
// 页面私有状态
// -----------------------------------------------------------------------------
struct FamilyRowState {
    char name[32] = {};
    char phone[32] = {};
    int kinship = 0;
};

struct CheckInDetailState {
    bool loaded = false;
    int loadedId = -1;
    bool editable = false;

    data::CheckInRecord record;
    std::string hint;        // 顶部提示（如未从列表选择时展示示例）
    std::string loadError;   // 读取失败提示
    std::string serviceError;

    // 编辑缓冲
    char elderName[64] = {};
    char idCard[32] = {};
    char birthday[16] = {};
    char age[8] = {};
    int sex = 0;
    char phone[32] = {};
    char address[256] = {};
    std::vector<FamilyRowState> families;
    char startDate[16] = {};
    char endDate[16] = {};
    char feeStartDate[16] = {};
    char feeEndDate[16] = {};
    char bedNumber[32] = {};
    char deposit[16] = {};
    char otherFees[16] = {};
    char insurancePayment[16] = {};
    char governmentSubsidy[16] = {};
    char contractName[64] = {};
    char signDate[16] = {};
    char thirdPartyName[32] = {};
    char thirdPartyPhone[32] = {};
    char agreementPath[128] = {};

    // 校验错误
    std::string eElderName;
    std::string eIdCard;
    std::string eBirthday;
    std::string eAge;
    std::string ePhone;
    std::string eAddress;
    std::string eStartDate;
    std::string eEndDate;
    std::string eFeeStartDate;
    std::string eFeeEndDate;
    std::string eBedNumber;
    std::string eDeposit;
    std::string eOtherFees;
    std::string eInsurancePayment;
    std::string eGovernmentSubsidy;
    std::string eContractName;
    std::string eSignDate;
};

// -----------------------------------------------------------------------------
// 小工具
// -----------------------------------------------------------------------------
std::string TextOf(const char* chars) {
    return chars != nullptr ? std::string(chars) : std::string();
}

void CopyToBuffer(char* buffer, size_t size, const std::string& text) {
    if (buffer == nullptr || size == 0) {
        return;
    }
    std::snprintf(buffer, size, "%s", text.c_str());
}

std::string NumberText(double value) {
    char buffer[32];
    if (value == static_cast<double>(static_cast<long long>(value))) {
        std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    }
    return std::string(buffer);
}

bool IsDigits(const std::string& text, std::size_t minLength, std::size_t maxLength) {
    if (text.size() < minLength || text.size() > maxLength) {
        return false;
    }
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

double ParseDouble(const std::string& text, bool* ok) {
    if (ok != nullptr) {
        *ok = false;
    }
    if (text.empty()) {
        return 0.0;
    }
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == nullptr || *end != '\0') {
        return 0.0;
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return value;
}

void FormatDouble(char* buffer, size_t size, double value) {
    std::snprintf(buffer, size, "%g", value);
}

bool IsValidIdCard(const std::string& text) {
    if (text.size() != 18) {
        return false;
    }
    for (std::size_t index = 0; index < 17; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    const char last = text[17];
    return (last >= '0' && last <= '9') || last == 'X' || last == 'x';
}

bool IsValidDate(const std::string& text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        return false;
    }
    for (std::size_t index = 0; index < 10; ++index) {
        if (index == 4 || index == 7) {
            continue;
        }
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    const int month = (text[5] - '0') * 10 + (text[6] - '0');
    const int day = (text[8] - '0') * 10 + (text[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

void PushPageToast(World& world, const std::string& text, ui::ToastType type) {
    if (ui::ToastState* toasts = world.TryResource<ui::ToastState>()) {
        ui::PushToast(*toasts, text, type);
    }
}

void NavigateTo(World& world, const char* path) {
    if (CommandQueue* queue = world.TryResource<CommandQueue>()) {
        CommandQueue::Command command;
        command.type = CommandQueue::Type::Navigate;
        command.arg0 = path != nullptr ? path : "";
        queue->commands.push_back(command);
    }
}

data::ICheckInService* CheckInService(World& world) {
    DataServices* services = world.TryResource<DataServices>();
    return (services != nullptr && services->checkIn) ? services->checkIn.get() : nullptr;
}

// -----------------------------------------------------------------------------
// 记录 → 编辑缓冲 / 校验 / 保存
// -----------------------------------------------------------------------------
void BuffersFromRecord(CheckInDetailState& state) {
    const data::CheckInRecord& record = state.record;
    CopyToBuffer(state.elderName, sizeof(state.elderName), record.elderName);
    CopyToBuffer(state.idCard, sizeof(state.idCard), record.idCard);
    CopyToBuffer(state.birthday, sizeof(state.birthday), record.birthday);
    std::snprintf(state.age, sizeof(state.age), "%d", record.age);
    state.sex = record.sex;
    CopyToBuffer(state.phone, sizeof(state.phone), record.phone);
    CopyToBuffer(state.address, sizeof(state.address), record.address);
    state.families.clear();
    for (const data::CheckInFamilyRecord& family : record.families) {
        FamilyRowState row;
        CopyToBuffer(row.name, sizeof(row.name), family.name);
        CopyToBuffer(row.phone, sizeof(row.phone), family.phone);
        row.kinship = family.kinship;
        state.families.push_back(row);
    }
    if (state.families.empty()) {
        state.families.push_back(FamilyRowState{});
    }
    CopyToBuffer(state.startDate, sizeof(state.startDate), record.startDate);
    CopyToBuffer(state.endDate, sizeof(state.endDate), record.endDate);
    CopyToBuffer(state.feeStartDate, sizeof(state.feeStartDate), record.feeStartDate);
    CopyToBuffer(state.feeEndDate, sizeof(state.feeEndDate), record.feeEndDate);
    CopyToBuffer(state.bedNumber, sizeof(state.bedNumber), record.bedNumber);
    FormatDouble(state.deposit, sizeof(state.deposit), record.deposit);
    FormatDouble(state.otherFees, sizeof(state.otherFees), record.otherFees);
    FormatDouble(state.insurancePayment, sizeof(state.insurancePayment), record.insurancePayment);
    FormatDouble(state.governmentSubsidy, sizeof(state.governmentSubsidy),
                 record.governmentSubsidy);
    CopyToBuffer(state.contractName, sizeof(state.contractName), record.contractName);
    CopyToBuffer(state.signDate, sizeof(state.signDate), record.signDate);
    CopyToBuffer(state.thirdPartyName, sizeof(state.thirdPartyName), record.thirdPartyName);
    CopyToBuffer(state.thirdPartyPhone, sizeof(state.thirdPartyPhone), record.thirdPartyPhone);
    CopyToBuffer(state.agreementPath, sizeof(state.agreementPath), record.agreementPath);
}

bool ValidateBuffers(CheckInDetailState& state) {
    state.eElderName.clear();
    state.eIdCard.clear();
    state.eBirthday.clear();
    state.eAge.clear();
    state.ePhone.clear();
    state.eAddress.clear();
    state.eStartDate.clear();
    state.eEndDate.clear();
    state.eFeeStartDate.clear();
    state.eFeeEndDate.clear();
    state.eBedNumber.clear();
    state.eDeposit.clear();
    state.eOtherFees.clear();
    state.eInsurancePayment.clear();
    state.eGovernmentSubsidy.clear();
    state.eContractName.clear();
    state.eSignDate.clear();
    state.serviceError.clear();

    if (TextOf(state.elderName).empty()) {
        state.eElderName = "请输入老人姓名";
    }
    const std::string idCard = TextOf(state.idCard);
    if (idCard.empty()) {
        state.eIdCard = "请输入身份证号";
    } else if (!IsValidIdCard(idCard)) {
        state.eIdCard = "身份证号必须为 18 位（前 17 位数字，末位数字或 X）";
    }
    const std::string birthday = TextOf(state.birthday);
    if (birthday.empty()) {
        state.eBirthday = "请选择出生日期";
    } else if (!IsValidDate(birthday)) {
        state.eBirthday = "出生日期格式应为 YYYY-MM-DD";
    }
    const std::string age = TextOf(state.age);
    if (age.empty()) {
        state.eAge = "请输入年龄";
    } else if (!IsDigits(age, 1, 3)) {
        state.eAge = "年龄必须为 1~3 位数字";
    }
    const std::string phone = TextOf(state.phone);
    if (phone.empty()) {
        state.ePhone = "请输入联系方式";
    } else if (!IsDigits(phone, 11, 11)) {
        state.ePhone = "联系方式必须为 11 位数字";
    }
    if (TextOf(state.address).empty()) {
        state.eAddress = "请输入家庭住址";
    }

    const std::string startDate = TextOf(state.startDate);
    const std::string endDate = TextOf(state.endDate);
    if (startDate.empty()) {
        state.eStartDate = "请选择入住开始日期";
    } else if (!IsValidDate(startDate)) {
        state.eStartDate = "日期格式应为 YYYY-MM-DD";
    }
    if (endDate.empty()) {
        state.eEndDate = "请选择入住结束日期";
    } else if (!IsValidDate(endDate)) {
        state.eEndDate = "日期格式应为 YYYY-MM-DD";
    }
    if (state.eStartDate.empty() && state.eEndDate.empty() && endDate < startDate) {
        state.eEndDate = "入住结束日期不能早于开始日期";
    }
    const std::string feeStartDate = TextOf(state.feeStartDate);
    const std::string feeEndDate = TextOf(state.feeEndDate);
    if (feeStartDate.empty()) {
        state.eFeeStartDate = "请选择费用开始日期";
    } else if (!IsValidDate(feeStartDate)) {
        state.eFeeStartDate = "日期格式应为 YYYY-MM-DD";
    }
    if (feeEndDate.empty()) {
        state.eFeeEndDate = "请选择费用结束日期";
    } else if (!IsValidDate(feeEndDate)) {
        state.eFeeEndDate = "日期格式应为 YYYY-MM-DD";
    }
    if (state.eFeeStartDate.empty() && state.eFeeEndDate.empty() && feeEndDate < feeStartDate) {
        state.eFeeEndDate = "费用结束日期不能早于开始日期";
    }
    if (TextOf(state.bedNumber).empty()) {
        state.eBedNumber = "请输入入住床位（如 104-1）";
    }

    bool ok = false;
    ParseDouble(TextOf(state.deposit), &ok);
    if (!ok) {
        state.eDeposit = "请输入押金（数字）";
    }
    ParseDouble(TextOf(state.otherFees), &ok);
    if (!ok) {
        state.eOtherFees = "请输入其他费用（数字）";
    }
    ParseDouble(TextOf(state.insurancePayment), &ok);
    if (!ok) {
        state.eInsurancePayment = "请输入医保支付（数字）";
    }
    ParseDouble(TextOf(state.governmentSubsidy), &ok);
    if (!ok) {
        state.eGovernmentSubsidy = "请输入政府补贴（数字）";
    }
    if (TextOf(state.contractName).empty()) {
        state.eContractName = "请输入合同名称";
    }
    const std::string signDate = TextOf(state.signDate);
    if (signDate.empty()) {
        state.eSignDate = "请选择签约日期";
    } else if (!IsValidDate(signDate)) {
        state.eSignDate = "签约日期格式应为 YYYY-MM-DD";
    }

    return state.eElderName.empty() && state.eIdCard.empty() && state.eBirthday.empty() &&
           state.eAge.empty() && state.ePhone.empty() && state.eAddress.empty() &&
           state.eStartDate.empty() && state.eEndDate.empty() && state.eFeeStartDate.empty() &&
           state.eFeeEndDate.empty() && state.eBedNumber.empty() && state.eDeposit.empty() &&
           state.eOtherFees.empty() && state.eInsurancePayment.empty() &&
           state.eGovernmentSubsidy.empty() && state.eContractName.empty() &&
           state.eSignDate.empty();
}

data::CheckInRecord BuildUpdatedRecord(const CheckInDetailState& state) {
    data::CheckInRecord record = state.record;  // 保留 id / status / createTime / 护理等级与费用 / 图片路径
    record.elderName = TextOf(state.elderName);
    record.idCard = TextOf(state.idCard);
    record.birthday = TextOf(state.birthday);
    record.age = std::atoi(state.age);
    record.sex = state.sex;
    record.phone = TextOf(state.phone);
    record.address = TextOf(state.address);
    record.families.clear();
    for (const FamilyRowState& row : state.families) {
        data::CheckInFamilyRecord family;
        family.name = TextOf(row.name);
        family.phone = TextOf(row.phone);
        family.kinship = row.kinship;
        if (!family.name.empty() || !family.phone.empty()) {
            record.families.push_back(family);
        }
    }
    record.startDate = TextOf(state.startDate);
    record.endDate = TextOf(state.endDate);
    record.feeStartDate = TextOf(state.feeStartDate);
    record.feeEndDate = TextOf(state.feeEndDate);
    record.bedNumber = TextOf(state.bedNumber);
    record.deposit = ParseDouble(TextOf(state.deposit), nullptr);
    record.otherFees = ParseDouble(TextOf(state.otherFees), nullptr);
    record.insurancePayment = ParseDouble(TextOf(state.insurancePayment), nullptr);
    record.governmentSubsidy = ParseDouble(TextOf(state.governmentSubsidy), nullptr);
    record.contractName = TextOf(state.contractName);
    record.signDate = TextOf(state.signDate);
    record.thirdPartyName = TextOf(state.thirdPartyName);
    record.thirdPartyPhone = TextOf(state.thirdPartyPhone);
    record.agreementPath = TextOf(state.agreementPath);
    return record;
}

// 载入记录：优先 CheckInSelection.id；未选择时回退展示第一条示例
void LoadRecord(World& world, CheckInDetailState& state, bool force) {
    const CheckInSelection* selection = world.TryResource<CheckInSelection>();
    const int selectionId = selection != nullptr ? selection->id : 0;
    if (!force && state.loaded && state.loadedId == selectionId) {
        return;
    }
    data::ICheckInService* service = CheckInService(world);
    state.loadError.clear();
    state.serviceError.clear();
    state.hint.clear();
    state.loaded = true;
    state.loadedId = selectionId;
    if (service == nullptr) {
        state.loadError = "数据服务未就绪，请稍后重试";
        return;
    }
    if (selectionId > 0) {
        data::CheckInRecord record;
        std::string error;
        if (!service->Get(selectionId, &record, &error)) {
            state.loadError = error.empty() ? "读取入住记录失败" : error;
            return;
        }
        state.record = record;
        BuffersFromRecord(state);
        return;
    }
    // 未从列表选择：展示第一条示例（M4 前端自测/直接进入详情时可用）
    const std::vector<data::CheckInRecord> records = service->List(data::CheckInQuery{});
    if (records.empty()) {
        state.loadError = "暂无入住记录，请先在列表页发起入住申请";
        return;
    }
    state.record = records.front();
    state.hint = "未从列表选择记录，当前展示第 1 条示例数据（从列表点「查看」可查看指定记录）";
    BuffersFromRecord(state);
}

// -----------------------------------------------------------------------------
// 绘制基础
// -----------------------------------------------------------------------------
struct SectionLayout {
    float x = 0.0f;
    float width = 0.0f;
    float columnWidth = 0.0f;
    float controlWidth = 0.0f;
};

struct PageMapper {
    float originX = 0.0f;
    float originY = 0.0f;
    float scrollY = 0.0f;
    ImVec2 Pos(float x, float y) const { return ImVec2(originX + x, originY + y - scrollY); }
    void SetCursor(float x, float y) const { ImGui::SetCursorScreenPos(Pos(x, y)); }
};

void PushInputStyle(bool invalid) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 9.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(theme::Colors().panel));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ImGui::ColorConvertU32ToFloat4(invalid ? theme::Colors().danger
                                                                 : theme::Colors().border));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,
                          ImGui::ColorConvertU32ToFloat4(theme::Colors().textPlaceholder));
}

void PopInputStyle() {
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

void DrawErrorText(const PageMapper& mapper, float x, float y, const std::string& error) {
    if (error.empty()) {
        return;
    }
    mapper.SetCursor(x, y);
    ui::ScopedFontSize font(12.0f);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s", error.c_str());
}

void DrawLabel(const PageMapper& mapper, const SectionLayout& layout, int column, float y,
               const char* label, bool required) {
    const float labelRight = layout.x + static_cast<float>(column) * layout.columnWidth + kLabelWidth;
    const char* star = required ? "*" : "";
    const float starWidth = ImGui::CalcTextSize(star).x;
    const float textWidth = ImGui::CalcTextSize(label).x;
    mapper.SetCursor(labelRight - starWidth - textWidth,
                     y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f);
    if (required) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "*");
        ImGui::SameLine(0.0f, 0.0f);
    }
    ImGui::TextUnformatted(label);
}

float ControlX(const SectionLayout& layout, int column) {
    return layout.x + static_cast<float>(column) * layout.columnWidth + kLabelWidth +
           kLabelControlGap;
}

// 只读值展示（灰底文本；只读态/详情页主用）
void DrawReadOnly(const PageMapper& mapper, const SectionLayout& layout, int column, float y,
                  const char* label, const std::string& value,
                  const std::string& error = std::string(), float height = kControlHeight) {
    DrawLabel(mapper, layout, column, y, label, false);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = mapper.Pos(ControlX(layout, column), y);
    const ImVec2 max(min.x + layout.controlWidth, min.y + height);
    drawList->AddRectFilled(min, max, theme::Colors().panelHover, 4.0f);
    drawList->AddRect(min, max,
                      error.empty() ? theme::Colors().border : theme::Colors().danger, 4.0f, 0,
                      1.0f);
    {
        ui::ScopedFontSize font(14.0f);
        if (height <= kControlHeight + 1.0f) {
            const std::string shown = value.empty() ? std::string("--") : value;
            const ImVec2 textSize = ImGui::CalcTextSize(shown.c_str());
            drawList->AddText(ImVec2(min.x + 8.0f, min.y + (height - textSize.y) * 0.5f),
                              value.empty() ? theme::Colors().textPlaceholder
                                            : theme::Colors().textRegular,
                              shown.c_str());
        } else {
            drawList->AddText(ImVec2(min.x + 8.0f, min.y + 8.0f),
                              value.empty() ? theme::Colors().textPlaceholder
                                            : theme::Colors().textRegular,
                              value.empty() ? "--" : value.c_str());
        }
    }
    DrawErrorText(mapper, ControlX(layout, column), y + height + 2.0f, error);
}

// 可编辑输入框
bool DrawInput(const PageMapper& mapper, const SectionLayout& layout, int column, float y,
               const char* label, bool required, char* buffer, size_t bufferSize,
               const char* hint, const std::string& error, ImGuiInputTextFlags flags = 0) {
    DrawLabel(mapper, layout, column, y, label, required);
    PushInputStyle(!error.empty());
    mapper.SetCursor(ControlX(layout, column), y);
    ImGui::SetNextItemWidth(layout.controlWidth);
    const std::string id = std::string("##detail_") + label;
    bool edited = false;
    {
        ui::ScopedFontSize font(14.0f);
        if (hint != nullptr) {
            edited = ImGui::InputTextWithHint(id.c_str(), hint, buffer, bufferSize, flags);
        } else {
            edited = ImGui::InputText(id.c_str(), buffer, bufferSize, flags);
        }
    }
    PopInputStyle();
    DrawErrorText(mapper, ControlX(layout, column), y + kControlHeight + 2.0f, error);
    return edited;
}

// 详情图片：有图（WIC 纹理）按 contain 绘制，无图灰底占位（100×100）
void DrawImageBox(World& world, const PageMapper& mapper, const layout::Rect& localRect,
                  const std::string& path, const char* emptyText) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = mapper.Pos(localRect.x, localRect.y);
    const ImVec2 max(min.x + localRect.w, min.y + localRect.h);
    drawList->AddRectFilled(min, max, theme::Colors().panelHover, 4.0f);
    drawList->AddRect(min, max, theme::Colors().border, 4.0f, 0, 1.0f);
    render::TextureCache* textures = world.TryResource<render::TextureCache>();
    if (textures != nullptr && !path.empty()) {
        const render::Texture& texture = textures->Get(path.c_str());
        if (texture.Valid()) {
            layout::Rect target{localRect.x + 4.0f, localRect.y + 4.0f, localRect.w - 8.0f,
                                localRect.h - 8.0f};
            const layout::Rect screenRect{mapper.originX + target.x,
                                          mapper.originY + target.y - mapper.scrollY, target.w,
                                          target.h};
            render::DrawImageContain(drawList, texture, screenRect);
            return;
        }
    }
    {
        ui::ScopedFontSize font(12.0f);
        const char* text = emptyText != nullptr ? emptyText : "暂无图片";
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        drawList->AddText(ImVec2(min.x + (localRect.w - textSize.x) * 0.5f,
                                 min.y + (localRect.h - textSize.y) * 0.5f),
                          theme::Colors().textPlaceholder, text);
    }
}

// -----------------------------------------------------------------------------
// 页面系统
// -----------------------------------------------------------------------------
class CheckInDetailPageSystem final : public ISystem {
public:
    const char* Name() const override { return "CheckInDetailPageSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& frame) override {
        const RouteState* route = world.TryResource<RouteState>();
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        if (route == nullptr || shell == nullptr || route->current != Route::CheckInDetail) {
            m_visible = false;
            return;
        }
        CheckInDetailState* state = world.TryResource<CheckInDetailState>();
        if (state == nullptr) {
            return;
        }
        if (!m_visible) {
            m_visible = true;
            LoadRecord(world, *state, true);
            state->editable = false;
            m_resetScroll = true;
        }
        const layout::Rect pageViewport = shell->pageViewport;
        if (pageViewport.Empty()) {
            return;
        }
        DrawPage(world, *shell, *state);

        if (ui::ToastState* toasts = world.TryResource<ui::ToastState>()) {
            ui::DrawToasts(shell->viewport, *toasts, frame.deltaTime);
        }
    }

private:
    static float BasicHeight() {
        // 只读：两行 60 + 各行 60 + 地址块(94) / 一寸照片(100) → 取 100；身份证两面行 100
        return kFirstRowOffset + 3.0f * kRowStep + kImageBlockHeight + kRowGap +
               kImageBlockHeight + 16.0f;
    }
    static float FamilyHeight(const CheckInDetailState& state) {
        const float rows = static_cast<float>(std::max<std::size_t>(state.record.families.size(), 1));
        return kFirstRowOffset + kFamilyHeaderHeight + rows * kFamilyRowHeight + 16.0f;
    }
    static float ConfigHeight() { return kFirstRowOffset + 5.0f * kRowStep + 16.0f; }
    static float ContractHeight() { return kFirstRowOffset + 3.0f * kRowStep + 16.0f; }

    void DrawPage(World& world, const layout::ShellLayout& shell, CheckInDetailState& state) {
        const layout::Rect pageViewport = shell.pageViewport;
        ImGui::SetNextWindowPos(ui::Min(pageViewport));
        ImGui::SetNextWindowSize(ImVec2(pageViewport.w, pageViewport.h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;
        const bool open = ImGui::Begin("##checkin_detail_page", nullptr, flags);
        if (open) {
            if (m_resetScroll) {
                m_resetScroll = false;
                ImGui::SetScrollY(0.0f);
            }
            PageMapper mapper;
            mapper.originX = pageViewport.x;
            mapper.originY = pageViewport.y;
            mapper.scrollY = ImGui::GetScrollY();
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const float contentWidth = std::max(1.0f, pageViewport.w - kScrollbarReserve);
            SectionLayout layout;
            layout.x = kSectionPaddingX;
            layout.width = std::max(240.0f, contentWidth - 2.0f * kSectionPaddingX);
            layout.columnWidth = (layout.width - kColumnGap) * 0.5f;
            layout.controlWidth =
                std::max(120.0f, layout.columnWidth - kLabelWidth - kLabelControlGap);
            const float cardWidth = layout.width + 2.0f * kSectionPaddingX;

            float cursorY = 0.0f;
            // 顶部提示 / 错误
            if (!state.hint.empty()) {
                mapper.SetCursor(layout.x, cursorY);
                ui::ScopedFontSize font(13.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().warning), "%s",
                                   state.hint.c_str());
                cursorY += 28.0f;
            }
            if (!state.loadError.empty()) {
                mapper.SetCursor(layout.x, cursorY);
                ui::ScopedFontSize font(13.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                                   state.loadError.c_str());
                cursorY += 28.0f;
            }
            if (!state.serviceError.empty()) {
                mapper.SetCursor(layout.x, cursorY);
                ui::ScopedFontSize font(13.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                                   state.serviceError.c_str());
                cursorY += 28.0f;
            }

            if (state.loadError.empty() || state.loaded) {
                const auto drawSectionCard = [&](float y, float height) {
                    const ImVec2 min = mapper.Pos(0.0f, y);
                    const ImVec2 max = mapper.Pos(cardWidth, y + height);
                    drawList->AddRectFilled(min, max, theme::Colors().panel, 4.0f);
                    drawList->AddRect(min, max, theme::Colors().border, 4.0f, 0, 1.0f);
                };

                // 先确定四个分区的 Y 与高度，并绘制卡底（卡底先于内容，避免遮盖文字/控件）
                const float basicY = cursorY;
                const float familyY = basicY + BasicHeight() + kSectionGap;
                const float configY = familyY + FamilyHeight(state) + kSectionGap;
                const float contractY = configY + ConfigHeight() + kSectionGap;
                drawSectionCard(basicY, BasicHeight());
                drawSectionCard(familyY, FamilyHeight(state));
                drawSectionCard(configY, ConfigHeight());
                drawSectionCard(contractY, ContractHeight());

                DrawSectionTitle(mapper, layout.x, basicY + 12.0f, "基本信息");
                DrawBasicSection(world, mapper, layout, state, basicY + kFirstRowOffset);

                DrawSectionTitle(mapper, layout.x, familyY + 12.0f, "家属信息");
                DrawFamilySection(mapper, layout, state, familyY + kFirstRowOffset);

                DrawSectionTitle(mapper, layout.x, configY + 12.0f, "入住配置");
                DrawConfigSection(mapper, layout, state, configY + kFirstRowOffset);

                DrawSectionTitle(mapper, layout.x, contractY + 12.0f, "签约办理");
                DrawContractSection(world, mapper, layout, state, contractY + kFirstRowOffset);

                cursorY = contractY + ContractHeight() + kSectionGap;
            }

            DrawFooter(world, mapper, layout, state, cursorY);
            cursorY += kFooterHeight + kPageBottomPadding;
            ImGui::SetCursorPos(ImVec2(0.0f, cursorY));
            ImGui::Dummy(ImVec2(1.0f, 1.0f));
            (void)drawList;
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void DrawSectionTitle(const PageMapper& mapper, float x, float y, const char* title) {
        mapper.SetCursor(x, y);
        ui::ScopedFontSize font(16.0f);
        ImGui::TextUnformatted(title);
    }

    void DrawBasicSection(World& world, const PageMapper& mapper, const SectionLayout& layout,
                          CheckInDetailState& state, float firstRow) {
        const data::CheckInRecord& record = state.record;
        if (!state.editable) {
            float y = firstRow;
            DrawReadOnly(mapper, layout, 0, y, "老人姓名", record.elderName);
            DrawReadOnly(mapper, layout, 1, y, "身份证号", record.idCard);
            y += kRowStep;
            DrawReadOnly(mapper, layout, 0, y, "出生日期", record.birthday);
            DrawReadOnly(mapper, layout, 1, y, "年龄", record.age > 0 ? std::to_string(record.age) : "");
            y += kRowStep;
            DrawReadOnly(mapper, layout, 0, y, "性别", record.sex == 1 ? "男" : "女");
            DrawReadOnly(mapper, layout, 1, y, "联系方式", record.phone);
            y += kRowStep;

            // 家庭住址（4 行文本块）+ 一寸照片 100×100
            DrawReadOnly(mapper, layout, 0, y, "家庭住址", record.address, std::string(),
                         kAddressBlockHeight);
            DrawLabel(mapper, layout, 1, y, "一寸照片", false);
            DrawImageBox(world, mapper,
                         layout::Rect{ControlX(layout, 1), y, kImageSize, kImageSize},
                         record.imagePath, "暂无图片");
            y += kImageBlockHeight + kRowGap;

            DrawLabel(mapper, layout, 0, y, "身份证人像面", false);
            DrawImageBox(world, mapper,
                         layout::Rect{ControlX(layout, 0), y, kImageSize, kImageSize},
                         record.idCardPortraitImg, "暂无图片");
            DrawLabel(mapper, layout, 1, y, "身份证国徽面", false);
            DrawImageBox(world, mapper,
                         layout::Rect{ControlX(layout, 1), y, kImageSize, kImageSize},
                         record.idCardNationalEmblemImg, "暂无图片");
            return;
        }

        // 可编辑态
        float y = firstRow;
        DrawInput(mapper, layout, 0, y, "老人姓名", true, state.elderName, sizeof(state.elderName),
                  "请输入老人姓名", state.eElderName);
        DrawInput(mapper, layout, 1, y, "身份证号", true, state.idCard, sizeof(state.idCard),
                  "请输入身份证号", state.eIdCard);
        y += kRowStep;
        DrawInput(mapper, layout, 0, y, "出生日期", true, state.birthday, sizeof(state.birthday),
                  "YYYY-MM-DD", state.eBirthday);
        DrawInput(mapper, layout, 1, y, "年龄", true, state.age, sizeof(state.age), "请输入年龄",
                  state.eAge, ImGuiInputTextFlags_CharsDecimal);
        y += kRowStep;
        DrawLabel(mapper, layout, 0, y, "性别", true);
        {
            ui::ScopedFontSize font(14.0f);
            const float radioY = y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f;
            mapper.SetCursor(ControlX(layout, 0), radioY);
            ImGui::RadioButton("男", &state.sex, 1);
            ImGui::SameLine(0.0f, 24.0f);
            ImGui::RadioButton("女", &state.sex, 0);
        }
        DrawInput(mapper, layout, 1, y, "联系方式", true, state.phone, sizeof(state.phone),
                  "请输入联系方式", state.ePhone, ImGuiInputTextFlags_CharsDecimal);
        y += kRowStep;

        DrawLabel(mapper, layout, 0, y, "家庭住址", true);
        PushInputStyle(!state.eAddress.empty());
        mapper.SetCursor(ControlX(layout, 0), y);
        ImGui::SetNextItemWidth(layout.controlWidth);
        {
            ui::ScopedFontSize font(14.0f);
            ImGui::InputTextMultiline("##detail_address", state.address, sizeof(state.address),
                                      ImVec2(layout.controlWidth, kAddressBlockHeight));
        }
        PopInputStyle();
        DrawErrorText(mapper, ControlX(layout, 0), y + kAddressBlockHeight + 2.0f,
                      state.eAddress);
        y += kAddressBlockHeight + kRowGap;

        DrawLabel(mapper, layout, 0, y, "身份证人像面", false);
        DrawImageBox(world, mapper,
                     layout::Rect{ControlX(layout, 0), y, kImageSize, kImageSize},
                     state.record.idCardPortraitImg, "暂无图片");
        DrawLabel(mapper, layout, 1, y, "身份证国徽面", false);
        DrawImageBox(world, mapper,
                     layout::Rect{ControlX(layout, 1), y, kImageSize, kImageSize},
                     state.record.idCardNationalEmblemImg, "暂无图片");
    }

    void DrawFamilySection(const PageMapper& mapper, const SectionLayout& layout,
                           CheckInDetailState& state, float headerY) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float contentW = layout.width + 2.0f * kSectionPaddingX;
        const ImVec2 headerMin = mapper.Pos(0.0f, headerY);
        const ImVec2 headerMax = mapper.Pos(contentW, headerY + kFamilyHeaderHeight);
        drawList->AddRectFilled(headerMin, headerMax, theme::Colors().lightBlock, 4.0f);

        const char* titles[] = {"家属姓名", "家属联系方式", "与老人关系", "操作"};
        const float columns[] = {0.04f, 0.40f, 0.68f, 0.88f};
        {
            ui::ScopedFontSize font(14.0f);
            for (int index = 0; index < (state.editable ? 3 : 3); ++index) {
                drawList->AddText(mapper.Pos(columns[index] * contentW,
                                             headerY + (kFamilyHeaderHeight -
                                                        ImGui::GetTextLineHeight()) * 0.5f),
                                  theme::Colors().textPrimary, titles[index]);
            }

        }

        int addRowAt = -1;
        int removeRowAt = -1;
        float rowY = headerY + kFamilyHeaderHeight;
        const std::size_t rowCount =
            state.editable ? state.families.size() : std::max<std::size_t>(state.record.families.size(), 1);
        for (std::size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
            const float fieldY = rowY + (kFamilyRowHeight - kControlHeight) * 0.5f;
            const data::CheckInFamilyRecord* source =
                (!state.editable && rowIndex < state.record.families.size())
                    ? &state.record.families[rowIndex]
                    : nullptr;
            if (state.editable && rowIndex < state.families.size()) {
                FamilyRowState& row = state.families[rowIndex];
                PushInputStyle(false);
                mapper.SetCursor(0.04f * contentW, fieldY);
                ImGui::SetNextItemWidth(192.0f);
                {
                    ui::ScopedFontSize font(14.0f);
                    ImGui::InputTextWithHint(
                        (std::string("##df_name_") + std::to_string(rowIndex)).c_str(),
                        "请输入家属姓名", row.name, sizeof(row.name));
                }
                mapper.SetCursor(0.40f * contentW, fieldY);
                ImGui::SetNextItemWidth(192.0f);
                {
                    ui::ScopedFontSize font(14.0f);
                    ImGui::InputTextWithHint(
                        (std::string("##df_phone_") + std::to_string(rowIndex)).c_str(),
                        "请输入家属联系方式", row.phone, sizeof(row.phone));
                }
                mapper.SetCursor(0.68f * contentW, fieldY);
                ImGui::SetNextItemWidth(200.0f);
                {
                    ui::ScopedFontSize font(14.0f);
                    if (ImGui::BeginCombo(
                            (std::string("##df_kin_") + std::to_string(rowIndex)).c_str(),
                            KinshipText(row.kinship))) {
                        for (int kinship = 0; kinship < kKinshipCount; ++kinship) {
                            if (ImGui::Selectable(KinshipText(kinship), row.kinship == kinship)) {
                                row.kinship = kinship;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                PopInputStyle();

                const float buttonY = rowY + (kFamilyRowHeight - 32.0f) * 0.5f;
                const float addX = 0.88f * contentW;
                const float delX = addX + 40.0f;
                mapper.SetCursor(addX, buttonY);
                if (ImGui::InvisibleButton(
                        (std::string("##df_add_") + std::to_string(rowIndex)).c_str(),
                        ImVec2(32.0f, 32.0f))) {
                    addRowAt = static_cast<int>(rowIndex);
                }
                bool hovered = ImGui::IsItemHovered();
                ImVec2 min = mapper.Pos(addX, buttonY);
                drawList->AddRectFilled(min, ImVec2(min.x + 32.0f, min.y + 32.0f),
                                        hovered ? theme::Colors().panelHover
                                                : theme::Colors().panel,
                                        4.0f);
                drawList->AddRect(min, ImVec2(min.x + 32.0f, min.y + 32.0f),
                                  theme::Colors().border, 4.0f, 0, 1.0f);
                {
                    ui::ScopedFontSize font(16.0f);
                    drawList->AddText(ImVec2(min.x + 10.0f, min.y + 5.0f),
                                      theme::Colors().primary, "+");
                }
                const bool removable = state.families.size() > 1;
                mapper.SetCursor(delX, buttonY);
                if (ImGui::InvisibleButton(
                        (std::string("##df_del_") + std::to_string(rowIndex)).c_str(),
                        ImVec2(32.0f, 32.0f))) {
                    removeRowAt = static_cast<int>(rowIndex);
                }
                hovered = ImGui::IsItemHovered();
                min = mapper.Pos(delX, buttonY);
                drawList->AddRectFilled(min, ImVec2(min.x + 32.0f, min.y + 32.0f),
                                        hovered && removable ? theme::Colors().panelHover
                                                             : theme::Colors().panel,
                                        4.0f);
                drawList->AddRect(min, ImVec2(min.x + 32.0f, min.y + 32.0f),
                                  theme::Colors().border, 4.0f, 0, 1.0f);
                {
                    ui::ScopedFontSize font(16.0f);
                    drawList->AddText(ImVec2(min.x + 11.0f, min.y + 5.0f),
                                      removable ? theme::Colors().primary
                                                : theme::Colors().textMuted,
                                      "-");
                }
            } else {
                {
                    ui::ScopedFontSize font(14.0f);
                    const std::string name = source != nullptr ? source->name : std::string();
                    const std::string phone = source != nullptr ? source->phone : std::string();
                    const std::string kinship =
                        source != nullptr ? std::string(KinshipText(source->kinship))
                                          : std::string();
                    const float textY = rowY + (kFamilyRowHeight - ImGui::GetTextLineHeight()) * 0.5f;
                    drawList->AddText(
                        mapper.Pos(0.04f * contentW, textY),
                        name.empty() ? theme::Colors().textPlaceholder : theme::Colors().textPrimary,
                        name.empty() ? "--" : name.c_str());
                    drawList->AddText(mapper.Pos(0.40f * contentW, textY),
                                      phone.empty() ? theme::Colors().textPlaceholder
                                                    : theme::Colors().textPrimary,
                                      phone.empty() ? "--" : phone.c_str());
                    drawList->AddText(mapper.Pos(0.68f * contentW, textY),
                                      theme::Colors().textPrimary,
                                      kinship.empty() ? "--" : kinship.c_str());
                }
            }
            rowY += kFamilyRowHeight;
        }
        if (addRowAt >= 0) {
            state.families.push_back(FamilyRowState{});
        } else if (removeRowAt >= 0 && state.families.size() > 1) {
            state.families.erase(state.families.begin() + removeRowAt);
        }
    }

    void DrawConfigSection(const PageMapper& mapper, const SectionLayout& layout,
                           CheckInDetailState& state, float firstRow) {
        const data::CheckInRecord& record = state.record;
        float y = firstRow;
        if (!state.editable) {
            DrawReadOnly(mapper, layout, 0, y, "入住期限", record.startDate + " 至 " + record.endDate);
            DrawReadOnly(mapper, layout, 1, y, "费用期限",
                         record.feeStartDate + " 至 " + record.feeEndDate);
            y += kRowStep;
            DrawReadOnly(mapper, layout, 0, y, "护理等级", record.nursingLevelName);
            // 入住床位只读显示床号文本（不用级联）
            DrawReadOnly(mapper, layout, 1, y, "入住床位", record.bedNumber);
            y += kRowStep;
            DrawReadOnly(mapper, layout, 0, y, "押金", NumberText(record.deposit));
            DrawReadOnly(mapper, layout, 1, y, "护理费用", NumberText(record.nursingFee));
            y += kRowStep;
            DrawReadOnly(mapper, layout, 0, y, "床位费用", NumberText(record.bedFee));
            DrawReadOnly(mapper, layout, 1, y, "其他费用", NumberText(record.otherFees));
            y += kRowStep;
            DrawReadOnly(mapper, layout, 0, y, "医保支付", NumberText(record.insurancePayment));
            DrawReadOnly(mapper, layout, 1, y, "政府补贴", NumberText(record.governmentSubsidy));
            return;
        }

        // 可编辑态：入住期限/费用期限、床位号、押金与费用（护理费/床位费只读带出，不支持改）
        DrawRangeRow(mapper, layout, 0, y, "入住期限", state.startDate, sizeof(state.startDate),
                     state.endDate, sizeof(state.endDate), state.eStartDate, state.eEndDate);
        DrawRangeRow(mapper, layout, 1, y, "费用期限", state.feeStartDate,
                     sizeof(state.feeStartDate), state.feeEndDate, sizeof(state.feeEndDate),
                     state.eFeeStartDate, state.eFeeEndDate);
        y += kRowStep;
        DrawReadOnly(mapper, layout, 0, y, "护理等级", record.nursingLevelName);
        DrawInput(mapper, layout, 1, y, "入住床位", true, state.bedNumber,
                  sizeof(state.bedNumber), "请输入床位号，如 104-1", state.eBedNumber);
        y += kRowStep;
        DrawInput(mapper, layout, 0, y, "押金", true, state.deposit, sizeof(state.deposit), "3000",
                  state.eDeposit, ImGuiInputTextFlags_CharsDecimal);
        DrawReadOnly(mapper, layout, 1, y, "护理费用", NumberText(record.nursingFee));
        y += kRowStep;
        DrawReadOnly(mapper, layout, 0, y, "床位费用", NumberText(record.bedFee));
        DrawInput(mapper, layout, 1, y, "其他费用", true, state.otherFees,
                  sizeof(state.otherFees), "0", state.eOtherFees,
                  ImGuiInputTextFlags_CharsDecimal);
        y += kRowStep;
        DrawInput(mapper, layout, 0, y, "医保支付", true, state.insurancePayment,
                  sizeof(state.insurancePayment), "0", state.eInsurancePayment,
                  ImGuiInputTextFlags_CharsDecimal);
        DrawInput(mapper, layout, 1, y, "政府补贴", true, state.governmentSubsidy,
                  sizeof(state.governmentSubsidy), "0", state.eGovernmentSubsidy,
                  ImGuiInputTextFlags_CharsDecimal);
    }

    void DrawRangeRow(const PageMapper& mapper, const SectionLayout& layout, int column, float y,
                      const char* label, char* startBuffer, size_t startSize, char* endBuffer,
                      size_t endSize, const std::string& startError, const std::string& endError) {
        DrawLabel(mapper, layout, column, y, label, true);
        const float x = ControlX(layout, column);
        const float halfWidth = std::max(60.0f, (layout.controlWidth - 16.0f - 12.0f) * 0.5f);
        PushInputStyle(!startError.empty() || !endError.empty());
        mapper.SetCursor(x, y);
        ImGui::SetNextItemWidth(halfWidth);
        {
            ui::ScopedFontSize font(14.0f);
            ImGui::InputTextWithHint((std::string("##dr_start_") + label).c_str(), "YYYY-MM-DD",
                                     startBuffer, startSize);
        }
        ImGui::SameLine(0.0f, 6.0f);
        {
            ui::ScopedFontSize font(14.0f);
            ImGui::TextUnformatted("至");
        }
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::SetNextItemWidth(halfWidth);
        {
            ui::ScopedFontSize font(14.0f);
            ImGui::InputTextWithHint((std::string("##dr_end_") + label).c_str(), "YYYY-MM-DD",
                                     endBuffer, endSize);
        }
        PopInputStyle();
        DrawErrorText(mapper, x, y + kControlHeight + 2.0f,
                      !startError.empty() ? startError : endError);
    }

    void DrawContractSection(World& world, const PageMapper& mapper, const SectionLayout& layout,
                             CheckInDetailState& state, float firstRow) {
        (void)world;
        const data::CheckInRecord& record = state.record;
        float y = firstRow;
        if (!state.editable) {
            DrawReadOnly(mapper, layout, 0, y, "合同名称", record.contractName);
            DrawReadOnly(mapper, layout, 1, y, "签约日期", record.signDate);
            y += kRowStep;
            DrawReadOnly(mapper, layout, 0, y, "丙方姓名", record.thirdPartyName);
            DrawReadOnly(mapper, layout, 1, y, "丙方联系方式", record.thirdPartyPhone);
            y += kRowStep;

            // 入住合同：合同名称 + [查看] 链接（PDF 预览二期）
            DrawLabel(mapper, layout, 0, y, "入住合同", false);
            const std::string contractText =
                record.contractName.empty() ? std::string("--") : record.contractName + ".pdf";
            {
                ui::ScopedFontSize font(14.0f);
                mapper.SetCursor(ControlX(layout, 0),
                                 y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f);
                ImGui::TextUnformatted(contractText.c_str());
                ImGui::SameLine(0.0f, 12.0f);
                const ImVec2 linkPos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##contract_link", ImVec2(36.0f, 18.0f));
                const bool linkHovered = ImGui::IsItemHovered();
                ImGui::GetWindowDrawList()->AddText(
                    linkPos, theme::Colors().link, "[查看]");
                if (linkHovered) {
                    ImGui::SetTooltip("PDF 预览二期支持");
                }
            }
            return;
        }

        DrawInput(mapper, layout, 0, y, "合同名称", true, state.contractName,
                  sizeof(state.contractName), "请输入合同名称", state.eContractName);
        DrawInput(mapper, layout, 1, y, "签约日期", true, state.signDate, sizeof(state.signDate),
                  "YYYY-MM-DD", state.eSignDate);
        y += kRowStep;
        DrawInput(mapper, layout, 0, y, "丙方姓名", false, state.thirdPartyName,
                  sizeof(state.thirdPartyName), "请输入丙方姓名", std::string());
        DrawInput(mapper, layout, 1, y, "丙方联系方式", false, state.thirdPartyPhone,
                  sizeof(state.thirdPartyPhone), "请输入丙方联系方式", std::string(),
                  ImGuiInputTextFlags_CharsDecimal);
        y += kRowStep;
        DrawLabel(mapper, layout, 0, y, "入住合同", false);
        {
            ui::ScopedFontSize font(14.0f);
            mapper.SetCursor(ControlX(layout, 0), y + (kControlHeight - ImGui::GetTextLineHeight()) * 0.5f);
            const std::string text = TextOf(state.agreementPath).empty() ? std::string("--")
                                                                        : TextOf(state.agreementPath);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().textMuted), "%s",
                               text.c_str());
        }
    }

    void DrawFooter(World& world, const PageMapper& mapper, const SectionLayout& layout,
                    CheckInDetailState& state, float y) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float buttonWidth = 88.0f;
        const float gap = 8.0f;
        // 只读：编辑 / 返回列表；可编辑：保存 / 取消编辑 / 返回列表
        int buttonCount = state.editable ? 3 : 2;
        const float totalWidth = static_cast<float>(buttonCount) * buttonWidth +
                                 static_cast<float>(buttonCount - 1) * gap;
        float x = layout.x + (layout.width - totalWidth) * 0.5f;

        const auto drawButton = [&](const char* label, bool primary, bool* clicked) {
            mapper.SetCursor(x, y);
            *clicked = ImGui::InvisibleButton((std::string("##detail_btn_") + label).c_str(),
                                               ImVec2(buttonWidth, kFooterHeight));
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 min = mapper.Pos(x, y);
            const ImVec2 max(min.x + buttonWidth, min.y + kFooterHeight);
            if (primary) {
                drawList->AddRectFilled(min, max,
                                        hovered ? theme::Colors().primaryHover
                                                : theme::Colors().primary,
                                        4.0f);
            } else {
                drawList->AddRectFilled(min, max,
                                        hovered ? theme::Colors().panelHover
                                                : theme::Colors().panel,
                                        4.0f);
                drawList->AddRect(min, max, theme::Colors().border, 4.0f, 0, 1.0f);
            }
            {
                ui::ScopedFontSize font(14.0f);
                const ImVec2 textSize = ImGui::CalcTextSize(label);
                drawList->AddText(
                    ImVec2(min.x + (buttonWidth - textSize.x) * 0.5f,
                           min.y + (kFooterHeight - textSize.y) * 0.5f),
                    primary ? theme::Colors().textInverse : theme::Colors().textRegular, label);
            }
            x += buttonWidth + gap;
        };

        bool saveClicked = false;
        bool cancelEditClicked = false;
        bool editClicked = false;
        bool backClicked = false;
        if (state.editable) {
            drawButton("保存", true, &saveClicked);
            drawButton("取消编辑", false, &cancelEditClicked);
            drawButton("返回列表", false, &backClicked);
        } else {
            drawButton("编辑", true, &editClicked);
            drawButton("返回列表", false, &backClicked);
        }

        if (backClicked) {
            NavigateTo(world, "/enterQuit/checkIn");
            return;
        }
        if (editClicked) {
            BuffersFromRecord(state);
            state.editable = true;
            state.serviceError.clear();
            m_resetScroll = true;  // 切换编辑态回到顶部，避免用户停留在底部看不到字段
            return;
        }
        if (cancelEditClicked) {
            BuffersFromRecord(state);
            state.editable = false;
            state.serviceError.clear();
            m_resetScroll = true;
            return;
        }
        if (saveClicked) {
            Save(world, state);
        }
    }

    void Save(World& world, CheckInDetailState& state) {
        data::ICheckInService* service = CheckInService(world);
        if (service == nullptr) {
            state.serviceError = "数据服务未就绪，请稍后重试";
            return;
        }
        if (!ValidateBuffers(state)) {
            PushPageToast(world, "请检查表单必填项", ui::ToastType::Warning);
            return;
        }
        const data::CheckInRecord updated = BuildUpdatedRecord(state);
        std::string error;
        if (!service->Update(updated, &error)) {
            state.serviceError = error.empty() ? "保存失败，请稍后重试" : error;
            PushPageToast(world, state.serviceError, ui::ToastType::Error);
            return;
        }
        PushPageToast(world, "保存成功", ui::ToastType::Success);
        state.editable = false;
        state.serviceError.clear();
        LoadRecord(world, state, true);
        m_resetScroll = true;
    }

    bool m_visible = false;
    bool m_resetScroll = false;
};

}  // namespace

void RegisterCheckInDetailModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<CheckInDetailState>();
    scheduler.Add(std::make_unique<CheckInDetailPageSystem>(), 200);
}

}  // namespace zzyl
