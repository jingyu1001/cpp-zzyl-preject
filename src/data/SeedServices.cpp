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
SeedUserService::SeedUserService() {
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
SeedRoomTypeService::SeedRoomTypeService() {
    m_roomTypes.push_back(RoomTypeRecord{1, "单人套房", 0, 4000.0, "宽敞舒适的套房，配备独立卫浴与护理呼叫系统", "", "", true, "2023-09-26 15:57:50"});
    m_roomTypes.push_back(RoomTypeRecord{2, "双人套房", 0, 6000.0, "适合夫妻或朋友两人居住，空间宽敞", "", "", true, "2023-09-26 15:58:51"});
    m_roomTypes.push_back(RoomTypeRecord{3, "豪华单人间", 0, 3000.0, "豪华装修的单人房间，采光充足", "", "", true, "2023-09-26 15:59:33"});
    m_roomTypes.push_back(RoomTypeRecord{4, "豪华双人间", 0, 4500.0, "精心装修的双人房间，设施齐全", "", "", true, "2023-09-26 16:00:03"});
    m_roomTypes.push_back(RoomTypeRecord{5, "普通单人间", 0, 2000.0, "简洁实用的单人间，性价比高", "", "", true, "2023-09-26 16:00:27"});
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
SeedNursingProjectService::SeedNursingProjectService() {
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

}  // namespace zzyl::data
