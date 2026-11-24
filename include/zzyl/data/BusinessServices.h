#pragma once

// =============================================================================
// Sprint 2 业务数据层：用户 / 房型 / 护理项目
//
// 本迭代全部使用内存种子数据（SeedServices），不引入 SQLite。
// 页面系统只通过接口读数据；增删改由页面系统调用接口完成（后续换 SQLite 只替换实现）。
// 字段与 Web 端一致（sys_user / room_type / nursing_project），详见
// docs/architecture/Cpp架构设计_v0.2.md 与 Web功能清单_v0.2.md。
// =============================================================================

#include <string>
#include <vector>

namespace zzyl::data {

// ---- 系统管理-用户管理（sys_user + dept/post/role 冗余名） ----
struct UserRecord {
    int userId = 0;
    std::string userName;   // 登录名
    std::string nickName;   // 昵称
    std::string deptName;   // 部门名（冗余，展示用）
    std::string phone;
    std::string email;
    std::string sex = "未知";  // 男/女/未知
    bool enabled = true;       // 正常=true / 停用=false
    std::string createTime;    // yyyy-MM-dd HH:mm:ss
    std::string remark;
    std::string postNames;  // 岗位（多选，逗号分隔）
    std::string roleNames;  // 角色（多选，逗号分隔）
};

struct UserQuery {
    std::string keyword;  // 用户名称 / 昵称模糊
    std::string phone;    // 手机号模糊
    int status = -1;      // -1 全部 / 1 正常 / 0 停用
    std::string deptName; // 部门（空=全部）
    std::string beginTime;
    std::string endTime;
};

class IUserService {
public:
    virtual ~IUserService() = default;
    virtual std::vector<UserRecord> List(const UserQuery& query) = 0;
    virtual bool Add(const UserRecord& record, std::string* error) = 0;
    virtual bool Update(const UserRecord& record, std::string* error) = 0;
    virtual bool Remove(const std::vector<int>& userIds, std::string* error) = 0;
    virtual bool SetStatus(int userId, bool enabled, std::string* error) = 0;
    virtual bool ResetPassword(int userId, const std::string& newPassword, std::string* error) = 0;
    virtual std::vector<std::string> DeptNames() = 0;
};

// ---- 在住管理-房型设置（room_type） ----
struct RoomTypeRecord {
    int id = 0;
    std::string name;          // 房型名称
    int bedCount = 0;          // 床位数量
    double price = 0.0;        // 床位费用（元/月）
    std::string introduction;  // 介绍
    std::string photoPath;     // 图片相对路径（assets/images/...），空=占位
    std::string remark;
    bool enabled = true;
    std::string createTime;
};

struct RoomTypeQuery {
    std::string keyword;  // 房型名称模糊
    int status = -1;      // -1 全部 / 1 启用 / 0 禁用
};

class IRoomTypeService {
public:
    virtual ~IRoomTypeService() = default;
    virtual std::vector<RoomTypeRecord> List(const RoomTypeQuery& query) = 0;
    virtual bool Add(const RoomTypeRecord& record, std::string* error) = 0;
    virtual bool Update(const RoomTypeRecord& record, std::string* error) = 0;
    virtual bool Remove(const std::vector<int>& ids, std::string* error) = 0;
    virtual bool SetStatus(int id, bool enabled, std::string* error) = 0;
};

// ---- 服务管理-护理项目（nursing_project） ----
struct NursingProjectRecord {
    int id = 0;
    std::string name;      // 名称
    std::string unit;      // 单位：次/件/餐/小时
    int orderNo = 0;       // 排序号
    double price = 0.0;    // 价格（元）
    bool enabled = true;   // 启用/禁用
    std::string nursingRequirement;  // 护理要求
    std::string imagePath;           // 图片相对路径（assets/icons/...）
    std::string createTime;
};

struct NursingProjectQuery {
    std::string keyword;  // 名称模糊
    int status = -1;      // -1 全部 / 1 启用 / 0 禁用
};

class INursingProjectService {
public:
    virtual ~INursingProjectService() = default;
    virtual std::vector<NursingProjectRecord> List(const NursingProjectQuery& query) = 0;
    virtual bool Add(const NursingProjectRecord& record, std::string* error) = 0;
    virtual bool Update(const NursingProjectRecord& record, std::string* error) = 0;
    virtual bool Remove(const std::vector<int>& ids, std::string* error) = 0;
    virtual bool SetStatus(int id, bool enabled, std::string* error) = 0;
    // 素材库（12 张图相对路径，供新增/编辑弹窗选择图片）
    virtual std::vector<std::string> ImageLibrary() = 0;
};

}  // namespace zzyl::data
