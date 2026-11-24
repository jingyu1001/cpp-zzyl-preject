#include "zzyl/data/SeedServices.h"

#include <algorithm>
#include <cctype>

namespace zzyl::data {
namespace {

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool ContainsNoCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
}

bool MatchStatus(bool enabled, int statusFilter) {
    if (statusFilter < 0) {
        return true;
    }
    return enabled == (statusFilter != 0);
}

}  // namespace

// =============================================================================
// 用户管理种子数据：Web 实测 7 条（sys_user）
// =============================================================================
SeedUserService::SeedUserService(bool emptyData) {
    if (emptyData) {
        return;  // --empty-data 空态测试钩子：不注入种子数据
    }
    m_users.push_back(UserRecord{1, "admin", "若依", "院长办公室", "15888888888", "ry@163.com",
                                 "男", true, "2024-08-14 02:48:23", "管理员", "董事长", "超级管理员"});
    m_users.push_back(UserRecord{2, "ry", "若依", "院长办公室", "15666666666", "ry@qq.com", "男",
                                 true, "2024-08-14 02:48:23", "", "项目经理", "普通角色"});
    m_users.push_back(UserRecord{101, "xiaoqing", "小青", "护理部", "", "", "女", true,
                                 "2024-10-18 10:12:45", "", "护理员", "护理员"});
    m_users.push_back(UserRecord{102, "xiaobai", "小白", "护理部", "", "", "女", true,
                                 "2024-10-18 10:13:12", "", "护理员", "护理员"});
    m_users.push_back(UserRecord{103, "laozhang", "老张", "后勤部", "", "", "男", true,
                                 "2024-10-18 10:13:41", "", "维修工", "维修工"});
    m_users.push_back(UserRecord{104, "laowang", "老王", "院长办公室", "", "", "男", true,
                                 "2024-10-18 10:13:59", "", "院长", "院长"});
    m_users.push_back(UserRecord{105, "xuxian", "许仙", "护理部", "", "", "男", true,
                                 "2024-10-19 01:27:15", "", "护理员", "护理员"});
}

std::vector<UserRecord> SeedUserService::List(const UserQuery& query) {
    std::vector<UserRecord> result;
    for (const UserRecord& user : m_users) {
        if (!ContainsNoCase(user.userName, query.keyword) &&
            !ContainsNoCase(user.nickName, query.keyword)) {
            continue;
        }
        if (!query.phone.empty() && user.phone.find(query.phone) == std::string::npos) {
            continue;
        }
        if (!query.deptName.empty() && user.deptName != query.deptName) {
            continue;
        }
        if (!MatchStatus(user.enabled, query.status)) {
            continue;
        }
        result.push_back(user);
    }
    return result;
}

bool SeedUserService::Add(const UserRecord& record, std::string* error) {
    if (record.userName.empty() || record.nickName.empty()) {
        if (error) *error = "用户名称/用户昵称不能为空";
        return false;
    }
    for (const UserRecord& user : m_users) {
        if (user.userName == record.userName) {
            if (error) *error = "用户名称已存在";
            return false;
        }
    }
    UserRecord added = record;
    added.userId = m_nextId++;
    m_users.push_back(added);
    return true;
}

bool SeedUserService::Update(const UserRecord& record, std::string* error) {
    for (UserRecord& user : m_users) {
        if (user.userId == record.userId) {
            user = record;
            return true;
        }
    }
    if (error) *error = "用户不存在";
    return false;
}

bool SeedUserService::Remove(const std::vector<int>& userIds, std::string* error) {
    if (userIds.empty()) {
        if (error) *error = "请选择要删除的用户";
        return false;
    }
    for (int id : userIds) {
        if (id == 1) {
            if (error) *error = "不允许删除超级管理员";
            return false;
        }
    }
    m_users.erase(std::remove_if(m_users.begin(), m_users.end(),
                                 [&userIds](const UserRecord& user) {
                                     return std::find(userIds.begin(), userIds.end(), user.userId) !=
                                            userIds.end();
                                 }),
                  m_users.end());
    return true;
}

bool SeedUserService::SetStatus(int userId, bool enabled, std::string* error) {
    for (UserRecord& user : m_users) {
        if (user.userId == userId) {
            if (userId == 1 && !enabled) {
                if (error) *error = "不允许停用超级管理员";
                return false;
            }
            user.enabled = enabled;
            return true;
        }
    }
    if (error) *error = "用户不存在";
    return false;
}

bool SeedUserService::ResetPassword(int userId, const std::string& newPassword, std::string* error) {
    if (newPassword.size() < 5 || newPassword.size() > 20) {
        if (error) *error = "密码长度必须为 5~20 个字符";
        return false;
    }
    for (const UserRecord& user : m_users) {
        if (user.userId == userId) {
            return true;  // 演示环境：只校验不真正存储
        }
    }
    if (error) *error = "用户不存在";
    return false;
}

std::vector<std::string> SeedUserService::DeptNames() {
    std::vector<std::string> names;
    const char* candidates[] = {"院长办公室", "高层办公室", "财务部", "行政部", "护理部", "后勤部", "销售部"};
    for (const char* name : candidates) {
        names.emplace_back(name);
    }
    return names;
}

// =============================================================================
// 房型设置种子数据：Web 实测 6 条（room_type）
// =============================================================================
SeedRoomTypeService::SeedRoomTypeService(bool emptyData) {
    if (emptyData) {
        return;  // --empty-data 空态测试钩子：不注入种子数据
    }
    m_roomTypes.push_back(RoomTypeRecord{1, "单人套房", 0, 4000.0, "宽敞舒适的套房，配备独立卫浴与护理呼叫系统", "assets/roomtypes/单人套房.png", "", true, "2023-09-26 15:57:50"});
    m_roomTypes.push_back(RoomTypeRecord{2, "双人套房", 0, 6000.0, "适合夫妻或朋友两人居住，空间宽敞", "assets/roomtypes/双人套房.png", "", true, "2023-09-26 15:58:51"});
    m_roomTypes.push_back(RoomTypeRecord{3, "豪华单人间", 0, 3000.0, "豪华装修的单人房间，采光充足", "assets/roomtypes/豪华单人间.png", "", true, "2023-09-26 15:59:33"});
    m_roomTypes.push_back(RoomTypeRecord{4, "豪华双人间", 0, 4500.0, "精心装修的双人房间，设施齐全", "assets/roomtypes/豪华双人间.png", "", true, "2023-09-26 16:00:03"});
    m_roomTypes.push_back(RoomTypeRecord{5, "普通单人间", 0, 2000.0, "简洁实用的单人间，性价比高", "assets/roomtypes/普通单人间.png", "", true, "2023-09-26 16:00:27"});
    m_roomTypes.push_back(RoomTypeRecord{116, "标准双人间", 10, 1500.0, "适合好朋友一起居住", "", "", true, "2024-09-12 22:52:36"});
}

std::vector<RoomTypeRecord> SeedRoomTypeService::List(const RoomTypeQuery& query) {
    std::vector<RoomTypeRecord> result;
    for (const RoomTypeRecord& record : m_roomTypes) {
        if (!ContainsNoCase(record.name, query.keyword)) {
            continue;
        }
        if (!MatchStatus(record.enabled, query.status)) {
            continue;
        }
        result.push_back(record);
    }
    return result;
}

bool SeedRoomTypeService::Add(const RoomTypeRecord& record, std::string* error) {
    if (record.name.empty()) {
        if (error) *error = "房型名称不能为空";
        return false;
    }
    if (record.price <= 0.0) {
        if (error) *error = "床位费用必须大于 0";
        return false;
    }
    RoomTypeRecord added = record;
    added.id = m_nextId++;
    m_roomTypes.push_back(added);
    return true;
}

bool SeedRoomTypeService::Update(const RoomTypeRecord& record, std::string* error) {
    for (RoomTypeRecord& existing : m_roomTypes) {
        if (existing.id == record.id) {
            existing = record;
            return true;
        }
    }
    if (error) *error = "房型不存在";
    return false;
}

bool SeedRoomTypeService::Remove(const std::vector<int>& ids, std::string* error) {
    if (ids.empty()) {
        if (error) *error = "请选择要删除的房型";
        return false;
    }
    m_roomTypes.erase(std::remove_if(m_roomTypes.begin(), m_roomTypes.end(),
                                     [&ids](const RoomTypeRecord& record) {
                                         return std::find(ids.begin(), ids.end(), record.id) !=
                                                ids.end();
                                     }),
                      m_roomTypes.end());
    return true;
}

bool SeedRoomTypeService::SetStatus(int id, bool enabled, std::string* error) {
    for (RoomTypeRecord& record : m_roomTypes) {
        if (record.id == id) {
            record.enabled = enabled;
            return true;
        }
    }
    if (error) *error = "房型不存在";
    return false;
}

// =============================================================================
// 护理项目种子数据：Web DB 9 条（nursing_project）；图片按 UI 规格 §6.2 映射
// =============================================================================
SeedNursingProjectService::SeedNursingProjectService(bool emptyData) {
    if (emptyData) {
        return;  // --empty-data 空态测试钩子：不注入种子数据
    }
    m_projects.push_back(NursingProjectRecord{1, "修剪指甲", "次", 1, 30.0, true, "保持指甲整洁，避免抓伤", "assets/icons/修剪指甲@2x.png", "2024-10-18 10:00:00"});
    m_projects.push_back(NursingProjectRecord{2, "衣物清洁", "件", 2, 5.0, true, "清洗、晾晒并整理衣物", "assets/icons/衣物清洁@2x.png", "2024-10-18 10:01:00"});
    m_projects.push_back(NursingProjectRecord{3, "整理床铺", "次", 3, 15.0, true, "整理床单、被褥，保持床铺整洁", "assets/icons/整体床铺@2x.png", "2024-10-18 10:02:00"});
    m_projects.push_back(NursingProjectRecord{4, "助餐", "餐", 4, 15.0, true, "协助老人进食、饮水", "assets/icons/助餐@2x.png", "2024-10-18 10:03:00"});
    m_projects.push_back(NursingProjectRecord{5, "助浴", "次", 5, 40.0, true, "协助老人沐浴，注意防滑与隐私", "assets/icons/助浴@2x.png", "2024-10-18 10:04:00"});
    m_projects.push_back(NursingProjectRecord{6, "洗头", "次", 6, 20.0, false, "协助老人清洗头发并吹干", "assets/icons/洗头@2x.png", "2024-10-18 10:05:00"});
    m_projects.push_back(NursingProjectRecord{7, "洗脸", "次", 7, 15.0, true, "协助老人清洁面部", "assets/icons/洗脸@2x.png", "2024-10-18 10:06:00"});
    m_projects.push_back(NursingProjectRecord{8, "洗脚", "次", 8, 20.0, true, "协助老人泡脚、清洁", "assets/icons/洗脚@2x.png", "2024-10-18 10:07:00"});
    m_projects.push_back(NursingProjectRecord{9, "心理咨询", "小时", 9, 80.0, true, "陪伴沟通，缓解老人情绪", "assets/icons/谈心@2x.png", "2024-10-18 10:08:00"});
}

std::vector<NursingProjectRecord> SeedNursingProjectService::List(const NursingProjectQuery& query) {
    std::vector<NursingProjectRecord> result;
    for (const NursingProjectRecord& record : m_projects) {
        if (!ContainsNoCase(record.name, query.keyword)) {
            continue;
        }
        if (!MatchStatus(record.enabled, query.status)) {
            continue;
        }
        result.push_back(record);
    }
    return result;
}

bool SeedNursingProjectService::Add(const NursingProjectRecord& record, std::string* error) {
    if (record.name.empty()) {
        if (error) *error = "护理项目名称不能为空";
        return false;
    }
    if (record.price <= 0.0) {
        if (error) *error = "价格必须大于 0";
        return false;
    }
    NursingProjectRecord added = record;
    added.id = m_nextId++;
    m_projects.push_back(added);
    return true;
}

bool SeedNursingProjectService::Update(const NursingProjectRecord& record, std::string* error) {
    for (NursingProjectRecord& existing : m_projects) {
        if (existing.id == record.id) {
            existing = record;
            return true;
        }
    }
    if (error) *error = "护理项目不存在";
    return false;
}

bool SeedNursingProjectService::Remove(const std::vector<int>& ids, std::string* error) {
    if (ids.empty()) {
        if (error) *error = "请选择要删除的护理项目";
        return false;
    }
    m_projects.erase(std::remove_if(m_projects.begin(), m_projects.end(),
                                    [&ids](const NursingProjectRecord& record) {
                                        return std::find(ids.begin(), ids.end(), record.id) !=
                                               ids.end();
                                    }),
                     m_projects.end());
    return true;
}

bool SeedNursingProjectService::SetStatus(int id, bool enabled, std::string* error) {
    for (NursingProjectRecord& record : m_projects) {
        if (record.id == id) {
            record.enabled = enabled;
            return true;
        }
    }
    if (error) *error = "护理项目不存在";
    return false;
}

std::vector<std::string> SeedNursingProjectService::ImageLibrary() {
    return {
        "assets/icons/修剪指甲@2x.png", "assets/icons/衣物清洁@2x.png",
        "assets/icons/整体床铺@2x.png", "assets/icons/助餐@2x.png",
        "assets/icons/助浴@2x.png",     "assets/icons/洗头@2x.png",
        "assets/icons/洗脸@2x.png",     "assets/icons/洗脚@2x.png",
        "assets/icons/谈心@2x.png",     "assets/icons/公共空间清洁@2x.png",
        "assets/icons/复建运动@2x.png", "assets/icons/翻身拍背@2x.png",
    };
}

// =============================================================================
// M4 入住办理内存种子（5 条；依据 M4 规格 v0.1 §1.7/§4.3，已清洗脏数据、补全配置/合同）
// 仅 --empty-data / DB 不可用回退；正常运行走 SQLite。
// =============================================================================
SeedCheckInService::SeedCheckInService(bool emptyData) {
    if (emptyData) {
        return;
    }
    const auto addRecord = [this](int id, const char* name, const char* idCard, const char* birthday,
                                  int age, int sex, const char* phone, const char* address,
                                  const char* bedNumber, int levelId, const char* levelName,
                                  double bedFee, const char* startDate, const char* endDate,
                                  const char* createTime, const char* contractName,
                                  const char* signDate) {
        CheckInRecord record;
        record.id = id;
        record.elderName = name;
        record.idCard = idCard;
        record.birthday = birthday;
        record.age = age;
        record.sex = sex;
        record.phone = phone;
        record.address = address;
        record.bedNumber = bedNumber;
        record.nursingLevelId = levelId;
        record.nursingLevelName = levelName;
        record.nursingFee = levelId == 76 ? 1500.0 : (levelId == 77 ? 2000.0 : 3000.0);
        record.bedFee = bedFee;
        record.deposit = 3000.0;
        record.startDate = startDate;
        record.endDate = endDate;
        record.feeStartDate = startDate;
        record.feeEndDate = endDate;
        record.contractName = contractName;
        record.signDate = signDate;
        record.createTime = createTime;
        record.status = 0;
        m_records.push_back(record);
    };

    addRecord(1, "张三", "132123196712131234", "1967-12-13", 57, 1, "13211223322",
              "河南省郑州市金水区花园路 88 号", "104-1", 77, "2号护理等级", 4500.0,
              "2024-08-27", "2024-09-30", "2024-08-27 16:43:20", "张三的入住合同", "2024-08-27");
    addRecord(2, "李天龙", "132123196712131239", "1967-12-13", 57, 1, "13211224433",
              "河南省郑州市中原区建设路 12 号", "104-2", 76, "1号护理等级", 4500.0,
              "2024-08-27", "2024-09-30", "2024-08-27 16:50:09", "李天龙的入住合同", "2024-08-27");
    addRecord(3, "老李", "132123195612132345", "1956-12-13", 68, 1, "13211225544",
              "河南省郑州市二七区大学路 66 号", "101-2", 77, "2号护理等级", 3000.0,
              "2024-09-12", "2024-10-31", "2024-09-12 18:51:36", "老李的入住合同", "2024-09-12");
    addRecord(4, "老李头儿", "410725196904056698", "1969-04-05", 55, 1, "13211226655",
              "河南省新乡市红旗区人民路 9 号", "101-1", 77, "2号护理等级", 3000.0,
              "2024-09-12", "2024-10-31", "2024-09-12 19:10:23", "老李头儿的入住合同", "2024-09-12");
    addRecord(5, "魏忠语", "210102196703292842", "1967-03-29", 58, 1, "13211227766",
              "辽宁省沈阳市和平区中山路 21 号", "403-1", 77, "2号护理等级", 2000.0,
              "2026-03-05", "2026-04-30", "2026-03-05 20:49:06", "魏忠语的入住合同", "2026-03-05");

    m_records[0].families.push_back(CheckInFamilyRecord{"张小三", "13900001111", 0});
    m_records[1].families.push_back(CheckInFamilyRecord{"李小龙", "13900002222", 0});
    m_records[2].families.push_back(CheckInFamilyRecord{"李芳", "13900003333", 0});
    m_records[3].families.push_back(CheckInFamilyRecord{"李强", "13900004444", 0});
    m_records[4].families.push_back(CheckInFamilyRecord{"魏小语", "13900005555", 0});
    m_records[4].families.push_back(CheckInFamilyRecord{"魏强", "13900006666", 2});
}

std::vector<CheckInRecord> SeedCheckInService::List(const CheckInQuery& query) {
    std::vector<CheckInRecord> result;
    for (const CheckInRecord& record : m_records) {
        if (!ContainsNoCase(record.elderName, query.elderName)) {
            continue;
        }
        if (!query.idCard.empty() && record.idCard != query.idCard) {
            continue;
        }
        result.push_back(record);
    }
    return result;
}

bool SeedCheckInService::Get(int id, CheckInRecord* outRecord, std::string* error) {
    for (const CheckInRecord& record : m_records) {
        if (record.id == id) {
            if (outRecord) {
                *outRecord = record;
            }
            return true;
        }
    }
    if (error) *error = "入住记录不存在";
    return false;
}

bool SeedCheckInService::Add(const CheckInRecord& record, std::string* error) {
    if (record.elderName.empty() || record.idCard.empty()) {
        if (error) *error = "老人姓名/身份证号不能为空";
        return false;
    }
    CheckInRecord added = record;
    added.id = m_nextId++;
    if (added.status != 1) {
        added.status = 0;
    }
    m_records.push_back(added);
    return true;
}

bool SeedCheckInService::Update(const CheckInRecord& record, std::string* error) {
    for (CheckInRecord& existing : m_records) {
        if (existing.id == record.id) {
            existing = record;
            return true;
        }
    }
    if (error) *error = "入住记录不存在";
    return false;
}

bool SeedCheckInService::Remove(const std::vector<int>& ids, std::string* error) {
    if (ids.empty()) {
        if (error) *error = "请选择要删除的入住记录";
        return false;
    }
    m_records.erase(std::remove_if(m_records.begin(), m_records.end(),
                                   [&ids](const CheckInRecord& record) {
                                       return std::find(ids.begin(), ids.end(), record.id) != ids.end();
                                   }),
                    m_records.end());
    return true;
}

std::vector<NursingLevelRecord> SeedCheckInService::NursingLevels() {
    return {
        NursingLevelRecord{76, "1号护理等级", 1500.0},
        NursingLevelRecord{77, "2号护理等级", 2000.0},
        NursingLevelRecord{78, "3号护理等级", 3000.0},
    };
}

// =============================================================================
// Sprint 4 内存回退（--empty-data / DB 不可用）
// =============================================================================
SeedNursingPlanService::SeedNursingPlanService(bool emptyData) {
    if (emptyData) {
        return;
    }
    m_plans.push_back(NursingPlanRecord{133, 1, "1号护理计划", 1, 0, "2024-08-19 19:17:31", ""});
    m_plans.push_back(NursingPlanRecord{134, 1, "2号护理计划", 0, 1, "2024-08-19 19:36:10", "2026-11-20 09:56:27"});
    m_plans.push_back(NursingPlanRecord{135, 1, "3号护理计划", 1, 0, "2024-08-29 16:56:39", ""});
}

std::vector<NursingPlanRecord> SeedNursingPlanService::List(const NursingPlanQuery& query) {
    std::vector<NursingPlanRecord> result;
    for (const NursingPlanRecord& plan : m_plans) {
        if (!ContainsNoCase(plan.planName, query.keyword)) {
            continue;
        }
        if (query.status >= 0 && plan.status != query.status) {
            continue;
        }
        result.push_back(plan);
    }
    return result;
}

bool SeedNursingPlanService::Get(int id, NursingPlanRecord* outRecord, std::string* error) {
    for (const NursingPlanRecord& plan : m_plans) {
        if (plan.id == id) {
            if (outRecord) *outRecord = plan;
            return true;
        }
    }
    if (error) *error = "护理计划不存在";
    return false;
}

bool SeedNursingPlanService::Add(const NursingPlanRecord& record, std::string* error) {
    if (record.planName.empty()) {
        if (error) *error = "护理计划名称不能为空";
        return false;
    }
    NursingPlanRecord added = record;
    added.id = m_nextId++;
    m_plans.push_back(added);
    return true;
}

bool SeedNursingPlanService::Update(const NursingPlanRecord& record, std::string* error) {
    for (NursingPlanRecord& plan : m_plans) {
        if (plan.id == record.id) {
            plan = record;
            return true;
        }
    }
    if (error) *error = "护理计划不存在";
    return false;
}

bool SeedNursingPlanService::Remove(const std::vector<int>& ids, std::string* error) {
    if (ids.empty()) {
        if (error) *error = "请选择要删除的护理计划";
        return false;
    }
    m_plans.erase(std::remove_if(m_plans.begin(), m_plans.end(),
                                 [&ids](const NursingPlanRecord& plan) {
                                     return std::find(ids.begin(), ids.end(), plan.id) != ids.end();
                                 }),
                  m_plans.end());
    return true;
}

bool SeedNursingPlanService::SetStatus(int id, bool enabled, std::string* error) {
    for (NursingPlanRecord& plan : m_plans) {
        if (plan.id == id) {
            plan.status = enabled ? 1 : 0;
            return true;
        }
    }
    if (error) *error = "护理计划不存在";
    return false;
}

SeedNursingLevelService::SeedNursingLevelService(bool emptyData) {
    if (emptyData) {
        return;
    }
    m_levels.push_back(NursingLevelRecord{76, "1号护理等级", 1500.0, 133, "1号护理计划", 1, "我是1号哦", "2024-08-14 16:33:16", ""});
    m_levels.push_back(NursingLevelRecord{77, "2号护理等级", 2000.0, 134, "2号护理计划", 1, "我是2号哦", "2024-08-20 11:18:21", ""});
    m_levels.push_back(NursingLevelRecord{78, "3号护理等级", 3000.0, 135, "3号护理计划", 1, "我是3号哦", "2024-08-29 16:58:00", ""});
}

std::vector<NursingLevelRecord> SeedNursingLevelService::List(const NursingLevelQuery& query) {
    std::vector<NursingLevelRecord> result;
    for (const NursingLevelRecord& level : m_levels) {
        if (!ContainsNoCase(level.name, query.keyword)) {
            continue;
        }
        if (query.status >= 0 && level.status != query.status) {
            continue;
        }
        result.push_back(level);
    }
    return result;
}

bool SeedNursingLevelService::Add(const NursingLevelRecord& record, std::string* error) {
    if (record.name.empty()) {
        if (error) *error = "护理等级名称不能为空";
        return false;
    }
    NursingLevelRecord added = record;
    added.id = m_nextId++;
    m_levels.push_back(added);
    return true;
}

bool SeedNursingLevelService::Update(const NursingLevelRecord& record, std::string* error) {
    for (NursingLevelRecord& level : m_levels) {
        if (level.id == record.id) {
            level = record;
            return true;
        }
    }
    if (error) *error = "护理等级不存在";
    return false;
}

bool SeedNursingLevelService::Remove(const std::vector<int>& ids, std::string* error) {
    if (ids.empty()) {
        if (error) *error = "请选择要删除的护理等级";
        return false;
    }
    m_levels.erase(std::remove_if(m_levels.begin(), m_levels.end(),
                                  [&ids](const NursingLevelRecord& level) {
                                      return std::find(ids.begin(), ids.end(), level.id) != ids.end();
                                  }),
                   m_levels.end());
    return true;
}

bool SeedNursingLevelService::SetStatus(int id, bool enabled, std::string* error) {
    for (NursingLevelRecord& level : m_levels) {
        if (level.id == id) {
            level.status = enabled ? 1 : 0;
            return true;
        }
    }
    if (error) *error = "护理等级不存在";
    return false;
}

}  // namespace zzyl::data
