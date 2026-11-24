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

// ---- Sprint 4：护理计划 / 护理等级 / 楼层-房间-床位 ----
struct NursingPlanProjectRecord {
    int id = 0;
    int planId = 0;
    int projectId = 0;
    std::string projectName;   // 冗余显示（nursing_projects.name）
    std::string executeTime;   // HH:mm:ss
    int executeCycle = 0;      // 0 天 / 1 周 / 2 月
    int executeFrequency = 1;  // 频次
};

struct NursingPlanRecord {
    int id = 0;
    int sortNo = 1;
    std::string planName;
    int status = 1;            // 0 禁用 / 1 启用
    int levelRefCount = 0;     // 被护理等级引用次数（>0 时列表禁用「修改/删除」，Web 口径）
    std::string createTime;
    std::string updateTime;
    std::vector<NursingPlanProjectRecord> projects;
};

struct NursingPlanQuery {
    std::string keyword;
    int status = -1;
};

class INursingPlanService {
public:
    virtual ~INursingPlanService() = default;
    virtual std::vector<NursingPlanRecord> List(const NursingPlanQuery& query) = 0;
    virtual bool Get(int id, NursingPlanRecord* outRecord, std::string* error) = 0;
    virtual bool Add(const NursingPlanRecord& record, std::string* error) = 0;
    virtual bool Update(const NursingPlanRecord& record, std::string* error) = 0;
    virtual bool Remove(const std::vector<int>& ids, std::string* error) = 0;
    virtual bool SetStatus(int id, bool enabled, std::string* error) = 0;
};

struct NursingLevelRecord {
    int id = 0;
    std::string name;   // 1号/2号/3号护理等级
    double price = 0.0; // 元/月（Web 字段 fee）
    int planId = 0;
    std::string planName;    // 冗余显示
    int status = 1;          // 0 禁用 / 1 启用
    std::string description;
    std::string createTime;
    std::string updateTime;
};

struct NursingLevelQuery {
    std::string keyword;
    int status = -1;
};

class INursingLevelService {
public:
    virtual ~INursingLevelService() = default;
    virtual std::vector<NursingLevelRecord> List(const NursingLevelQuery& query) = 0;
    virtual bool Add(const NursingLevelRecord& record, std::string* error) = 0;
    virtual bool Update(const NursingLevelRecord& record, std::string* error) = 0;
    virtual bool Remove(const std::vector<int>& ids, std::string* error) = 0;
    virtual bool SetStatus(int id, bool enabled, std::string* error) = 0;
};

struct FloorRecord {
    int id = 0;
    std::string name;
    int code = 0;
};

struct RoomRecord {
    int id = 0;
    std::string code;
    int sort = 0;
    std::string typeName;
    int floorId = 0;
    double price = 0.0;  // 房型床位费（room_type.price 弱关联；type_name 未匹配时为 0）
};

struct BedRecord {
    int id = 0;
    std::string bedNumber;
    int bedStatus = 0;  // 0 空闲 / 1 已入住 / 2 请假
    int sort = 0;
    int roomId = 0;
    std::string elderName;  // 已入住老人（M7 预览显示）
};

// M7 床位预览 + M4 床位级联（只读；床状态变更走入住业务）
class IFloorLayoutService {
public:
    virtual ~IFloorLayoutService() = default;
    virtual std::vector<FloorRecord> Floors() = 0;
    virtual std::vector<RoomRecord> Rooms(int floorId) = 0;
    virtual std::vector<BedRecord> Beds(int roomId) = 0;
    virtual std::vector<BedRecord> BedsByFloor(int floorId) = 0;
    // M4 级联专用：仅空闲床位（bed_status=0），Web getRoomAndBedByBedStatus/0 口径
    virtual std::vector<BedRecord> AvailableBeds(int roomId) = 0;
};

// ---- M4 入退管理-入住办理（check_in + check_in_family + nursing_levels） ----
// 字段依据 Web《Sprint3_M4入住办理规格_v0.1》§2.2/§4.1/§4.2；本地 SQLite 组装，不依赖后端 detail 接口。
struct CheckInFamilyRecord {
    std::string name;   // 家属姓名
    std::string phone;  // 家属联系方式
    int kinship = 0;    // 0 子女 / 1 配偶 / 2 亲属 / 3 朋友 / 4 社工 / 5 其他
};

struct CheckInRecord {
    int id = 0;
    // A. 基本信息（10 项）
    std::string elderName;            // 老人姓名
    std::string idCard;               // 身份证号
    std::string birthday;             // 出生日期 yyyy-MM-dd
    int age = 0;                      // 年龄
    int sex = 0;                      // 1 男 / 0 女
    std::string phone;                // 联系方式
    std::string address;              // 家庭住址
    std::string imagePath;            // 一寸照片（本地相对路径，空=无图）
    std::string idCardPortraitImg;    // 身份证人像面
    std::string idCardNationalEmblemImg;  // 身份证国徽面
    // B. 家属信息（本地独立子表 check_in_family；Web 为 remark JSON）
    std::vector<CheckInFamilyRecord> families;
    // C. 入住配置（10 项）
    std::string startDate;      // 入住开始 yyyy-MM-dd
    std::string endDate;        // 入住结束
    std::string feeStartDate;   // 费用开始
    std::string feeEndDate;     // 费用结束
    int nursingLevelId = 0;
    std::string nursingLevelName;
    std::string bedNumber;      // 入住床位号（如 104-1）
    int bedId = 0;
    int floorId = 0;            // Sprint 4 级联：楼层
    int roomId = 0;             // Sprint 4 级联：房间
    std::string floorName;
    std::string roomCode;
    double deposit = 3000.0;    // 押金（默认 3000）
    double nursingFee = 0.0;    // 护理费（由护理等级带出）
    double bedFee = 0.0;        // 床位费（由房型价格带出）
    double otherFees = 0.0;
    double insurancePayment = 0.0;
    double governmentSubsidy = 0.0;
    // D. 签约办理（5 项）
    std::string contractName;
    std::string signDate;       // 签约日期
    std::string thirdPartyName;
    std::string thirdPartyPhone;
    std::string agreementPath;  // 入住合同 PDF（本地相对路径，空=未上传）
    // 状态 / 审计
    int status = 0;             // 0 已入住 / 1 已退住
    std::string createTime;
    std::string remark;
};

struct CheckInQuery {
    std::string elderName;  // 老人姓名模糊
    std::string idCard;     // 身份证号（Web 为精确；本地用等于）
};

class ICheckInService {
public:
    virtual ~ICheckInService() = default;
    virtual std::vector<CheckInRecord> List(const CheckInQuery& query) = 0;
    virtual bool Get(int id, CheckInRecord* outRecord, std::string* error) = 0;
    virtual bool Add(const CheckInRecord& record, std::string* error) = 0;
    virtual bool Update(const CheckInRecord& record, std::string* error) = 0;
    virtual bool Remove(const std::vector<int>& ids, std::string* error) = 0;
    // 护理等级字典（Web `/nursing/level/all` 缺失；C++ 内置 3 条：1500/2000/3000）
    virtual std::vector<NursingLevelRecord> NursingLevels() = 0;
};

// ---- 服务接口集合（Sprint 3：SQLite 实现） ----

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
