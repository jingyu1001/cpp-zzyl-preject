// =============================================================================
// M4 办理入住（表单）· cpp-dev2
//
// 依据：
//  - Web《Sprint3_M4入住办理规格_v0.1.md》§2/§5.2（四分区 30 字段、两列 50%、label 100、
//    控件高 32、总高约 1352 需纵向滚动；截图 assets_s3/02、03）
//  - Sprint3_cpp-dev2_M4办理入住与详情.md；架构 v0.3 §3.3
//
// 结构（单页长表单，非弹窗）：
//   基本信息(10) → 家属信息(可增删行) → 入住配置(10) → 签约办理(5) → 底部 提交/取消
//
// 数据流（渲染与数据分离）：
//  页面私有状态 CheckInFormState（World 资源）持有全部字段缓冲；
//  提交时组装 data::CheckInRecord → DataServices.checkIn->Add（SQLite 事务持久化）；
//  成功 Toast「提交成功」+ 入队 Navigate /enterQuit/checkIn；失败保持页面并红字提示。
//  导航只入队 CommandQueue；本页不直接改 RouteState。
//
// 布局（禁止写死坐标）：所有矩形由 ShellLayout::pageViewport 逐层推导（内容宽 → 分区内边距
//  → 两列 → label 100 → 控件宽），页面整体纵向滚动。
// =============================================================================

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
#include "zzyl/render/Theme.h"
#include "zzyl/render/UiDraw.h"
#include "zzyl/ui/Widgets.h"

namespace zzyl {
namespace {

// -----------------------------------------------------------------------------
// 设计常量（Web 实测基准；仅作为布局推导参数，不是屏幕坐标）
// -----------------------------------------------------------------------------
constexpr float kLabelWidth = 100.0f;       // 标签宽（右对齐）
constexpr float kControlHeight = 32.0f;     // 控件高
constexpr float kRowGap = 28.0f;            // 行间距
constexpr float kRowStep = kControlHeight + kRowGap;
constexpr float kSectionPaddingX = 20.0f;   // 分区左右内边距
constexpr float kSectionTitleHeight = 26.0f;
constexpr float kSectionTitleGap = 12.0f;   // 标题 → 首行
constexpr float kFirstRowOffset = kSectionTitleHeight + kSectionTitleGap;
constexpr float kSectionGap = 24.0f;
constexpr float kLabelControlGap = 8.0f;
constexpr float kColumnGap = 24.0f;
constexpr float kRoleInputWidth = 192.0f;   // 家属姓名/电话（Web 192）
constexpr float kRoleComboWidth = 200.0f;   // 家属关系下拉（Web 200）
constexpr float kUploadButtonWidth = 88.0f;
constexpr float kUploadButtonHeight = 24.0f;
constexpr float kAddressHeight = 94.0f;     // 家庭住址 4 行文本域
constexpr float kFamilyHeaderHeight = 50.0f;
constexpr float kFamilyRowHeight = 50.0f;
constexpr float kScrollbarReserve = 12.0f;
constexpr float kPageBottomPadding = 24.0f;
constexpr float kFooterHeight = 32.0f;

// 床位号 → 房型床位费（规格 §4.3 实测映射；本期不做楼层级联，按床位号自动带出）
struct BedFeeSeed {
    const char* roomCode;
    double bedFee;
};
constexpr BedFeeSeed kBedFeeSeeds[] = {
    {"101", 3000.0}, {"104", 4500.0}, {"403", 2000.0}, {"501", 4000.0}, {"502", 6000.0},
};

double LookupBedFee(const std::string& bedNumber) {
    const std::size_t dash = bedNumber.find('-');
    const std::string roomCode =
        bedNumber.substr(0, dash == std::string::npos ? bedNumber.size() : dash);
    for (const BedFeeSeed& seed : kBedFeeSeeds) {
        if (roomCode == seed.roomCode) {
            return seed.bedFee;
        }
    }
    return 0.0;
}

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
// 页面私有状态（World 资源）
// -----------------------------------------------------------------------------
struct FamilyRowState {
    char name[32] = {};
    char phone[32] = {};
    int kinship = 0;
};

struct CheckInFormState {
    // A. 基本信息
    char elderName[64] = {};
    char idCard[32] = {};
    char birthday[16] = {};
    char age[8] = {};
    int sex = 0;  // 0 女 / 1 男（Web 默认 0）
    char phone[32] = {};
    char address[256] = {};
    char imagePath[128] = {};
    char idCardPortraitImg[128] = {};
    char idCardNationalEmblemImg[128] = {};
    // B. 家属信息
    std::vector<FamilyRowState> families;
    // C. 入住配置
    char startDate[16] = {};
    char endDate[16] = {};
    char feeStartDate[16] = {};
    char feeEndDate[16] = {};
    int nursingLevelIndex = -1;
    int nursingLevelId = 0;
    char nursingLevelName[64] = {};
    char bedNumber[32] = {};
    char deposit[16] = "3000";
    char nursingFee[16] = {};
    char bedFee[16] = {};
    char otherFees[16] = "0";
    char insurancePayment[16] = "0";
    char governmentSubsidy[16] = "0";
    // D. 签约办理
    char contractName[64] = {};
    char signDate[16] = {};
    char thirdPartyName[32] = {};
    char thirdPartyPhone[32] = {};
    char agreementPath[128] = {};
    // 字典
    std::vector<data::NursingLevelRecord> nursingLevels;
    // 校验结果
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
    std::string eNursingLevel;
    std::string eBedNumber;
    std::string eDeposit;
    std::string eNursingFee;
    std::string eBedFee;
    std::string eOtherFees;
    std::string eInsurancePayment;
    std::string eGovernmentSubsidy;
    std::string eContractName;
    std::string eSignDate;
    std::string serviceError;
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

std::string TextOf(const char* chars) {
    return chars != nullptr ? std::string(chars) : std::string();
}

void CopyToBuffer(char* buffer, size_t size, const std::string& text) {
    if (buffer == nullptr || size == 0) {
        return;
    }
    std::snprintf(buffer, size, "%s", text.c_str());
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
// 状态初始化 / 联动
// -----------------------------------------------------------------------------
void ResetForm(CheckInFormState& state) {
    state = CheckInFormState{};
    std::snprintf(state.deposit, sizeof(state.deposit), "3000");
    std::snprintf(state.otherFees, sizeof(state.otherFees), "0");
    std::snprintf(state.insurancePayment, sizeof(state.insurancePayment), "0");
    std::snprintf(state.governmentSubsidy, sizeof(state.governmentSubsidy), "0");
    state.families.clear();
    state.families.push_back(FamilyRowState{});
    state.nursingLevelIndex = -1;
    state.nursingLevelId = 0;
}

void EnsureNursingLevels(CheckInFormState& state, data::ICheckInService& service) {
    if (state.nursingLevels.empty()) {
        state.nursingLevels = service.NursingLevels();
    }
}

void ApplyNursingLevel(CheckInFormState& state) {
    if (state.nursingLevelIndex < 0 ||
        state.nursingLevelIndex >= static_cast<int>(state.nursingLevels.size())) {
        state.nursingLevelId = 0;
        state.nursingLevelName[0] = '\0';
        state.nursingFee[0] = '\0';
        return;
    }
    const data::NursingLevelRecord& level =
        state.nursingLevels[static_cast<std::size_t>(state.nursingLevelIndex)];
    state.nursingLevelId = level.id;
    CopyToBuffer(state.nursingLevelName, sizeof(state.nursingLevelName), level.name);
    FormatDouble(state.nursingFee, sizeof(state.nursingFee), level.price);
}

void ApplyBedNumber(CheckInFormState& state) {
    FormatDouble(state.bedFee, sizeof(state.bedFee), LookupBedFee(TextOf(state.bedNumber)));
}

bool ValidateForm(CheckInFormState& state) {
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
    state.eNursingLevel.clear();
    state.eBedNumber.clear();
    state.eDeposit.clear();
    state.eNursingFee.clear();
    state.eBedFee.clear();
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

    if (state.nursingLevelId <= 0) {
        state.eNursingLevel = "请选择护理等级";
    } else if (ParseDouble(TextOf(state.nursingFee), nullptr) <= 0.0) {
        state.eNursingFee = "护理费用未带出，请重新选择护理等级";
    }
    const std::string bedNumber = TextOf(state.bedNumber);
    if (bedNumber.empty()) {
        state.eBedNumber = "请输入入住床位（如 104-1）";
    } else if (ParseDouble(TextOf(state.bedFee), nullptr) <= 0.0) {
        state.eBedFee = "床位费用未带出，请检查床位号（如 104-1 / 101-1 / 403-1）";
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
           state.eFeeEndDate.empty() && state.eNursingLevel.empty() && state.eBedNumber.empty() &&
           state.eDeposit.empty() && state.eNursingFee.empty() && state.eBedFee.empty() &&
           state.eOtherFees.empty() && state.eInsurancePayment.empty() &&
           state.eGovernmentSubsidy.empty() && state.eContractName.empty() &&
           state.eSignDate.empty();
}

data::CheckInRecord BuildRecord(const CheckInFormState& state) {
    data::CheckInRecord record;
    record.elderName = TextOf(state.elderName);
    record.idCard = TextOf(state.idCard);
    record.birthday = TextOf(state.birthday);
    record.age = std::atoi(state.age);
    record.sex = state.sex;
    record.phone = TextOf(state.phone);
    record.address = TextOf(state.address);
    record.imagePath = TextOf(state.imagePath);
    record.idCardPortraitImg = TextOf(state.idCardPortraitImg);
    record.idCardNationalEmblemImg = TextOf(state.idCardNationalEmblemImg);
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
    record.nursingLevelId = state.nursingLevelId;
    record.nursingLevelName = TextOf(state.nursingLevelName);
    record.bedNumber = TextOf(state.bedNumber);
    record.deposit = ParseDouble(TextOf(state.deposit), nullptr);
    record.nursingFee = ParseDouble(TextOf(state.nursingFee), nullptr);
    record.bedFee = ParseDouble(TextOf(state.bedFee), nullptr);
    record.otherFees = ParseDouble(TextOf(state.otherFees), nullptr);
    record.insurancePayment = ParseDouble(TextOf(state.insurancePayment), nullptr);
    record.governmentSubsidy = ParseDouble(TextOf(state.governmentSubsidy), nullptr);
    record.contractName = TextOf(state.contractName);
    record.signDate = TextOf(state.signDate);
    record.thirdPartyName = TextOf(state.thirdPartyName);
    record.thirdPartyPhone = TextOf(state.thirdPartyPhone);
    record.agreementPath = TextOf(state.agreementPath);
    record.status = 0;
    record.createTime = NowText();
    return record;
}

// -----------------------------------------------------------------------------
// 绘制基础
// -----------------------------------------------------------------------------
struct SectionLayout {
    float x = 0.0f;  // 分区内容左缘（screen 的局部 x）
    float width = 0.0f;
    float columnWidth = 0.0f;
    float controlWidth = 0.0f;
};

// 页面局部坐标 → 窗口内屏幕坐标（窗口位于 pageViewport 左上，含滚动偏移）
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

bool DrawInput(const PageMapper& mapper, const SectionLayout& layout, int column, float y,
               const char* label, bool required, char* buffer, size_t bufferSize,
               const char* hint, const std::string& error, ImGuiInputTextFlags flags = 0) {
    DrawLabel(mapper, layout, column, y, label, required);
    PushInputStyle(!error.empty());
    mapper.SetCursor(ControlX(layout, column), y);
    ImGui::SetNextItemWidth(layout.controlWidth);
    const std::string id = std::string("##field_") + label;
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

void DrawReadOnly(const PageMapper& mapper, const SectionLayout& layout, int column, float y,
                  const char* label, const char* value,
                  const std::string& error = std::string()) {
    DrawLabel(mapper, layout, column, y, label, false);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = mapper.Pos(ControlX(layout, column), y);
    const ImVec2 max(min.x + layout.controlWidth, min.y + kControlHeight);
    drawList->AddRectFilled(min, max, theme::Colors().panelHover, 4.0f);
    drawList->AddRect(min, max,
                      error.empty() ? theme::Colors().border : theme::Colors().danger, 4.0f, 0,
                      1.0f);
    {
        ui::ScopedFontSize font(14.0f);
        const char* text = (value != nullptr && value[0] != '\0') ? value : "--";
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        drawList->AddText(ImVec2(min.x + 8.0f, min.y + (kControlHeight - textSize.y) * 0.5f),
                          (value != nullptr && value[0] != '\0') ? theme::Colors().textRegular
                                                                 : theme::Colors().textPlaceholder,
                          text);
    }
    DrawErrorText(mapper, ControlX(layout, column), y + kControlHeight + 2.0f, error);
}

// 上传按钮：占位实现（无本地文件对话框，点击由调用方 Toast「二期支持」）
bool DrawUploadButton(const PageMapper& mapper, const SectionLayout& layout, int column, float y,
                      const char* label) {
    DrawLabel(mapper, layout, column, y, label, false);
    const float x = ControlX(layout, column);
    mapper.SetCursor(x, y);
    const bool clicked = ImGui::InvisibleButton(
        (std::string("##upload_") + label).c_str(),
        ImVec2(kUploadButtonWidth, kUploadButtonHeight + 8.0f));
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = mapper.Pos(x, y);
    const ImVec2 max(min.x + kUploadButtonWidth, min.y + kUploadButtonHeight);
    drawList->AddRectFilled(min, max,
                            ImGui::IsItemHovered() ? theme::Colors().primaryHover
                                                   : theme::Colors().primary,
                            4.0f);
    {
        ui::ScopedFontSize font(13.0f);
        const ImVec2 textSize = ImGui::CalcTextSize("点击上传");
        drawList->AddText(ImVec2(min.x + (kUploadButtonWidth - textSize.x) * 0.5f,
                                 min.y + (kUploadButtonHeight - textSize.y) * 0.5f),
                          theme::Colors().textInverse, "点击上传");
    }
    return clicked;
}

void DrawSectionTitle(const PageMapper& mapper, float x, float y, const char* title) {
    mapper.SetCursor(x, y);
    ui::ScopedFontSize font(16.0f);
    ImGui::TextUnformatted(title);
}

// -----------------------------------------------------------------------------
// 页面系统
// -----------------------------------------------------------------------------
class CheckInFormPageSystem final : public ISystem {
public:
    const char* Name() const override { return "CheckInFormPageSystem"; }
    SystemPhase Phase() const override { return SystemPhase::Render; }

    void Update(World& world, const FrameContext& frame) override {
        const RouteState* route = world.TryResource<RouteState>();
        const layout::ShellLayout* shell = world.TryResource<layout::ShellLayout>();
        if (route == nullptr || shell == nullptr || route->current != Route::CheckInForm) {
            m_visible = false;
            return;
        }
        CheckInFormState* state = world.TryResource<CheckInFormState>();
        data::ICheckInService* service = CheckInService(world);
        if (state == nullptr || service == nullptr) {
            return;
        }
        if (!m_visible) {
            m_visible = true;
            ResetForm(*state);
            m_resetScroll = true;
        }
        EnsureNursingLevels(*state, *service);

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
    // 分区高度（与实际绘制用同一组公式，保证分区底色不裁内容）
    static float BasicHeight() {
        return kFirstRowOffset + 3.0f * kRowStep + kAddressHeight + kRowGap +
               kUploadButtonHeight + 8.0f + 16.0f;
    }
    static float FamilyHeight(const CheckInFormState& state) {
        const float rows = static_cast<float>(std::max<std::size_t>(state.families.size(), 1));
        return kFirstRowOffset + kFamilyHeaderHeight + rows * kFamilyRowHeight + 16.0f;
    }
    static float ConfigHeight() { return kFirstRowOffset + 5.0f * kRowStep + 16.0f; }
    static float ContractHeight() {
        return kFirstRowOffset + 2.0f * kRowStep + kUploadButtonHeight + 8.0f + 16.0f;
    }

    void DrawPage(World& world, const layout::ShellLayout& shell, CheckInFormState& state) {
        const layout::Rect pageViewport = shell.pageViewport;
        ImGui::SetNextWindowPos(ui::Min(pageViewport));
        ImGui::SetNextWindowSize(ImVec2(pageViewport.w, pageViewport.h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;
        const bool open = ImGui::Begin("##checkin_form_page", nullptr, flags);
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
            // 顶部错误提示（服务端）
            if (!state.serviceError.empty()) {
                mapper.SetCursor(layout.x, cursorY);
                ui::ScopedFontSize font(13.0f);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().danger), "%s",
                                   state.serviceError.c_str());
                cursorY += 28.0f;
            }

            const auto drawSectionCard = [&](float y, float height) {
                const ImVec2 min = mapper.Pos(0.0f, y);
                const ImVec2 max = mapper.Pos(cardWidth, y + height);
                drawList->AddRectFilled(min, max, theme::Colors().panel, 4.0f);
                drawList->AddRect(min, max, theme::Colors().border, 4.0f, 0, 1.0f);
            };

            // 先确定四个分区的 Y 与高度，并绘制卡底（卡底必须先于内容，避免遮住文字/控件）
            const float basicY = cursorY;
            const float familyY = basicY + BasicHeight() + kSectionGap;
            const float configY = familyY + FamilyHeight(state) + kSectionGap;
            const float contractY = configY + ConfigHeight() + kSectionGap;
            drawSectionCard(basicY, BasicHeight());
            drawSectionCard(familyY, FamilyHeight(state));
            drawSectionCard(configY, ConfigHeight());
            drawSectionCard(contractY, ContractHeight());

            // ---- 基本信息 ----
            DrawSectionTitle(mapper, layout.x, basicY + 12.0f, "基本信息");
            DrawBasicSection(world, mapper, layout, state, basicY + kFirstRowOffset);

            // ---- 家属信息 ----
            DrawSectionTitle(mapper, layout.x, familyY + 12.0f, "家属信息");
            DrawFamilySection(world, mapper, layout, state, familyY + kFirstRowOffset);

            // ---- 入住配置 ----
            DrawSectionTitle(mapper, layout.x, configY + 12.0f, "入住配置");
            DrawConfigSection(world, mapper, layout, state, configY + kFirstRowOffset);

            // ---- 签约办理 ----
            DrawSectionTitle(mapper, layout.x, contractY + 12.0f, "签约办理");
            DrawContractSection(world, mapper, layout, state, contractY + kFirstRowOffset);

            cursorY = contractY + ContractHeight() + kSectionGap;

            // ---- 底部按钮 ----
            DrawFooter(world, mapper, layout, state, cursorY);
            cursorY += kFooterHeight + kPageBottomPadding;

            ImGui::SetCursorPos(ImVec2(0.0f, cursorY));
            ImGui::Dummy(ImVec2(1.0f, 1.0f));
            (void)drawList;
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void DrawBasicSection(World& world, const PageMapper& mapper, const SectionLayout& layout,
                          CheckInFormState& state, float firstRow) {
        (void)world;
        float y = firstRow;
        DrawInput(mapper, layout, 0, y, "老人姓名", true, state.elderName,
                  sizeof(state.elderName), "请输入老人姓名", state.eElderName);
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

        // 家庭住址（4 行文本域）+ 一寸照片上传
        DrawLabel(mapper, layout, 0, y, "家庭住址", true);
        PushInputStyle(!state.eAddress.empty());
        mapper.SetCursor(ControlX(layout, 0), y);
        ImGui::SetNextItemWidth(layout.controlWidth);
        {
            ui::ScopedFontSize font(14.0f);
            ImGui::InputTextMultiline("##address", state.address, sizeof(state.address),
                                      ImVec2(layout.controlWidth, kAddressHeight));
        }
        PopInputStyle();
        DrawErrorText(mapper, ControlX(layout, 0), y + kAddressHeight + 2.0f, state.eAddress);
        if (DrawUploadButton(mapper, layout, 1, y, "一寸照片")) {
            PushPageToast(world, "图片上传二期支持（本地选图）", ui::ToastType::Warning);
        }
        y += kAddressHeight + kRowGap;

        if (DrawUploadButton(mapper, layout, 0, y, "身份证人像面")) {
            PushPageToast(world, "图片上传二期支持（本地选图）", ui::ToastType::Warning);
        }
        if (DrawUploadButton(mapper, layout, 1, y, "身份证国徽面")) {
            PushPageToast(world, "图片上传二期支持（本地选图）", ui::ToastType::Warning);
        }
    }

    void DrawFamilySection(World& world, const PageMapper& mapper, const SectionLayout& layout,
                           CheckInFormState& state, float headerY) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float contentX = 0.0f;
        const float contentW = layout.width + 2.0f * kSectionPaddingX;
        const ImVec2 headerMin = mapper.Pos(contentX, headerY);
        const ImVec2 headerMax = mapper.Pos(contentW, headerY + kFamilyHeaderHeight);
        drawList->AddRectFilled(headerMin, headerMax, theme::Colors().lightBlock, 4.0f);
        {
            ui::ScopedFontSize font(14.0f);
            const char* titles[] = {"家属姓名", "家属联系方式", "与老人关系", "操作"};
            const float columns[] = {0.04f, 0.40f, 0.68f, 0.88f};
            for (int index = 0; index < 4; ++index) {
                drawList->AddText(
                    mapper.Pos(columns[index] * contentW,
                               headerY + (kFamilyHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f),
                    theme::Colors().textPrimary, titles[index]);
            }
        }

        int addRowAt = -1;
        int removeRowAt = -1;
        float rowY = headerY + kFamilyHeaderHeight;
        for (std::size_t rowIndex = 0; rowIndex < state.families.size(); ++rowIndex) {
            FamilyRowState& row = state.families[rowIndex];
            const float fieldY = rowY + (kFamilyRowHeight - kControlHeight) * 0.5f;
            PushInputStyle(false);
            mapper.SetCursor(0.04f * contentW, fieldY);
            ImGui::SetNextItemWidth(kRoleInputWidth);
            {
                ui::ScopedFontSize font(14.0f);
                ImGui::InputTextWithHint(
                    (std::string("##family_name_") + std::to_string(rowIndex)).c_str(),
                    "请输入家属姓名", row.name, sizeof(row.name));
            }
            mapper.SetCursor(0.40f * contentW, fieldY);
            ImGui::SetNextItemWidth(kRoleInputWidth);
            {
                ui::ScopedFontSize font(14.0f);
                ImGui::InputTextWithHint(
                    (std::string("##family_phone_") + std::to_string(rowIndex)).c_str(),
                    "请输入家属联系方式", row.phone, sizeof(row.phone));
            }
            mapper.SetCursor(0.68f * contentW, fieldY);
            ImGui::SetNextItemWidth(kRoleComboWidth);
            {
                ui::ScopedFontSize font(14.0f);
                if (ImGui::BeginCombo(
                        (std::string("##family_kin_") + std::to_string(rowIndex)).c_str(),
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
                    (std::string("##family_add_") + std::to_string(rowIndex)).c_str(),
                    ImVec2(32.0f, 32.0f))) {
                addRowAt = static_cast<int>(rowIndex);
            }
            const bool addHovered = ImGui::IsItemHovered();
            const ImVec2 addMin = mapper.Pos(addX, buttonY);
            drawList->AddRectFilled(addMin, ImVec2(addMin.x + 32.0f, addMin.y + 32.0f),
                                    addHovered ? theme::Colors().panelHover : theme::Colors().panel,
                                    4.0f);
            drawList->AddRect(addMin, ImVec2(addMin.x + 32.0f, addMin.y + 32.0f),
                              theme::Colors().border, 4.0f, 0, 1.0f);
            {
                ui::ScopedFontSize font(16.0f);
                drawList->AddText(ImVec2(addMin.x + 10.0f, addMin.y + 5.0f),
                                  theme::Colors().primary, "+");
            }

            const bool removable = state.families.size() > 1;
            mapper.SetCursor(delX, buttonY);
            if (ImGui::InvisibleButton(
                    (std::string("##family_del_") + std::to_string(rowIndex)).c_str(),
                    ImVec2(32.0f, 32.0f))) {
                removeRowAt = static_cast<int>(rowIndex);
            }
            const bool delHovered = ImGui::IsItemHovered();
            const ImVec2 delMin = mapper.Pos(delX, buttonY);
            drawList->AddRectFilled(delMin, ImVec2(delMin.x + 32.0f, delMin.y + 32.0f),
                                    delHovered && removable ? theme::Colors().panelHover
                                                            : theme::Colors().panel,
                                    4.0f);
            drawList->AddRect(delMin, ImVec2(delMin.x + 32.0f, delMin.y + 32.0f),
                              theme::Colors().border, 4.0f, 0, 1.0f);
            {
                ui::ScopedFontSize font(16.0f);
                drawList->AddText(ImVec2(delMin.x + 11.0f, delMin.y + 5.0f),
                                  removable ? theme::Colors().primary : theme::Colors().textMuted,
                                  "-");
            }
            rowY += kFamilyRowHeight;
        }

        if (addRowAt >= 0) {
            state.families.push_back(FamilyRowState{});
        } else if (removeRowAt >= 0 && state.families.size() > 1) {
            state.families.erase(state.families.begin() + removeRowAt);
        }
        (void)world;
    }

    void DrawConfigSection(World& world, const PageMapper& mapper, const SectionLayout& layout,
                           CheckInFormState& state, float firstRow) {
        (void)world;
        float y = firstRow;
        DrawRangeRow(mapper, layout, 0, y, "入住期限", state.startDate, sizeof(state.startDate),
                     state.endDate, sizeof(state.endDate), state.eStartDate, state.eEndDate);
        DrawRangeRow(mapper, layout, 1, y, "费用期限", state.feeStartDate,
                     sizeof(state.feeStartDate), state.feeEndDate, sizeof(state.feeEndDate),
                     state.eFeeStartDate, state.eFeeEndDate);
        y += kRowStep;

        // 护理等级
        DrawLabel(mapper, layout, 0, y, "护理等级", true);
        PushInputStyle(!state.eNursingLevel.empty());
        mapper.SetCursor(ControlX(layout, 0), y);
        ImGui::SetNextItemWidth(layout.controlWidth);
        {
            ui::ScopedFontSize font(14.0f);
            const bool hasSelection = state.nursingLevelIndex >= 0 &&
                                      state.nursingLevelIndex <
                                          static_cast<int>(state.nursingLevels.size());
            const std::string preview =
                hasSelection
                    ? state.nursingLevels[static_cast<std::size_t>(state.nursingLevelIndex)].name
                    : "请选择护理等级";
            if (ImGui::BeginCombo("##nursing_level", preview.c_str())) {
                for (int index = 0; index < static_cast<int>(state.nursingLevels.size()); ++index) {
                    const data::NursingLevelRecord& level =
                        state.nursingLevels[static_cast<std::size_t>(index)];
                    char label[96];
                    std::snprintf(label, sizeof(label), "%s（%.0f 元/月）", level.name.c_str(),
                                  level.price);
                    if (ImGui::Selectable(label, state.nursingLevelIndex == index)) {
                        state.nursingLevelIndex = index;
                        ApplyNursingLevel(state);
                    }
                }
                ImGui::EndCombo();
            }
        }
        PopInputStyle();
        DrawErrorText(mapper, ControlX(layout, 0), y + kControlHeight + 2.0f,
                      state.eNursingLevel);

        const bool bedEdited = DrawInput(mapper, layout, 1, y, "入住床位", true, state.bedNumber,
                                         sizeof(state.bedNumber), "请输入床位号，如 104-1",
                                         state.eBedNumber);
        if (bedEdited) {
            ApplyBedNumber(state);
        }
        y += kRowStep;

        DrawInput(mapper, layout, 0, y, "押金", true, state.deposit, sizeof(state.deposit), "3000",
                  state.eDeposit, ImGuiInputTextFlags_CharsDecimal);
        DrawReadOnly(mapper, layout, 1, y, "护理费用", state.nursingFee, state.eNursingFee);
        y += kRowStep;

        DrawReadOnly(mapper, layout, 0, y, "床位费用", state.bedFee, state.eBedFee);
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
        const float toWidth = 16.0f;
        const float halfWidth = std::max(60.0f, (layout.controlWidth - toWidth - 12.0f) * 0.5f);
        PushInputStyle(!startError.empty() || !endError.empty());
        mapper.SetCursor(x, y);
        ImGui::SetNextItemWidth(halfWidth);
        {
            ui::ScopedFontSize font(14.0f);
            ImGui::InputTextWithHint((std::string("##range_start_") + label).c_str(), "YYYY-MM-DD",
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
            ImGui::InputTextWithHint((std::string("##range_end_") + label).c_str(), "YYYY-MM-DD",
                                     endBuffer, endSize);
        }
        PopInputStyle();
        DrawErrorText(mapper, x, y + kControlHeight + 2.0f,
                      !startError.empty() ? startError : endError);
    }

    void DrawContractSection(World& world, const PageMapper& mapper, const SectionLayout& layout,
                             CheckInFormState& state, float firstRow) {
        float y = firstRow;
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

        if (DrawUploadButton(mapper, layout, 0, y, "入住合同")) {
            PushPageToast(world, "合同 PDF 上传二期支持（本地选文件）", ui::ToastType::Warning);
        }
        mapper.SetCursor(ControlX(layout, 0) + kUploadButtonWidth + 8.0f,
                         y + (kUploadButtonHeight - ImGui::GetTextLineHeight()) * 0.5f);
        ui::ScopedFontSize hintFont(12.0f);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Colors().textMuted),
                           "只能上传不超过 2MB 的 PDF 文件");
    }

    void DrawFooter(World& world, const PageMapper& mapper, const SectionLayout& layout,
                    CheckInFormState& state, float y) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float buttonWidth = 88.0f;
        const float centerX = layout.x + layout.width * 0.5f;
        const float submitX = centerX - buttonWidth - 8.0f;
        const float cancelX = centerX + 8.0f;

        mapper.SetCursor(submitX, y);
        const bool submitClicked =
            ImGui::InvisibleButton("##submit_form", ImVec2(buttonWidth, kFooterHeight));
        const bool submitHovered = ImGui::IsItemHovered();
        const ImVec2 submitMin = mapper.Pos(submitX, y);
        drawList->AddRectFilled(submitMin,
                                ImVec2(submitMin.x + buttonWidth, submitMin.y + kFooterHeight),
                                submitHovered ? theme::Colors().primaryHover
                                              : theme::Colors().primary,
                                4.0f);
        {
            ui::ScopedFontSize font(14.0f);
            const ImVec2 textSize = ImGui::CalcTextSize("提交");
            drawList->AddText(ImVec2(submitMin.x + (buttonWidth - textSize.x) * 0.5f,
                                     submitMin.y + (kFooterHeight - textSize.y) * 0.5f),
                              theme::Colors().textInverse, "提交");
        }

        mapper.SetCursor(cancelX, y);
        const bool cancelClicked =
            ImGui::InvisibleButton("##cancel_form", ImVec2(buttonWidth, kFooterHeight));
        const bool cancelHovered = ImGui::IsItemHovered();
        const ImVec2 cancelMin = mapper.Pos(cancelX, y);
        drawList->AddRectFilled(cancelMin,
                                ImVec2(cancelMin.x + buttonWidth, cancelMin.y + kFooterHeight),
                                cancelHovered ? theme::Colors().panelHover
                                              : theme::Colors().panel,
                                4.0f);
        drawList->AddRect(cancelMin, ImVec2(cancelMin.x + buttonWidth, cancelMin.y + kFooterHeight),
                          theme::Colors().border, 4.0f, 0, 1.0f);
        {
            ui::ScopedFontSize font(14.0f);
            const ImVec2 textSize = ImGui::CalcTextSize("取消");
            drawList->AddText(ImVec2(cancelMin.x + (buttonWidth - textSize.x) * 0.5f,
                                     cancelMin.y + (kFooterHeight - textSize.y) * 0.5f),
                              theme::Colors().textRegular, "取消");
        }

        if (cancelClicked) {
            NavigateTo(world, "/enterQuit/checkIn");
            return;
        }
        if (submitClicked) {
            Submit(world, state);
        }
    }

    void Submit(World& world, CheckInFormState& state) {
        data::ICheckInService* service = CheckInService(world);
        if (service == nullptr) {
            state.serviceError = "数据服务未就绪，请稍后重试";
            return;
        }
        if (!ValidateForm(state)) {
            PushPageToast(world, "请检查表单必填项", ui::ToastType::Warning);
            return;
        }
        const data::CheckInRecord record = BuildRecord(state);
        std::string error;
        if (!service->Add(record, &error)) {
            state.serviceError = error.empty() ? "提交失败，请稍后重试" : error;
            PushPageToast(world, state.serviceError, ui::ToastType::Error);
            return;
        }
        PushPageToast(world, "提交成功", ui::ToastType::Success);
        NavigateTo(world, "/enterQuit/checkIn");
        ResetForm(state);
    }

    bool m_visible = false;
    bool m_resetScroll = false;
};

}  // namespace

void RegisterCheckInFormModule(World& world, SystemScheduler& scheduler) {
    world.EmplaceResource<CheckInFormState>();
    scheduler.Add(std::make_unique<CheckInFormPageSystem>(), 200);
}

}  // namespace zzyl
