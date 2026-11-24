#pragma once

// 内存种子数据实现（Sprint 2；SQLite 接入在 Sprint 3 替换本实现即可）

#include "zzyl/data/BusinessServices.h"

namespace zzyl::data {

class SeedUserService final : public IUserService {
public:
    explicit SeedUserService(bool emptyData = false);
    std::vector<UserRecord> List(const UserQuery& query) override;
    bool Add(const UserRecord& record, std::string* error) override;
    bool Update(const UserRecord& record, std::string* error) override;
    bool Remove(const std::vector<int>& userIds, std::string* error) override;
    bool SetStatus(int userId, bool enabled, std::string* error) override;
    bool ResetPassword(int userId, const std::string& newPassword, std::string* error) override;
    std::vector<std::string> DeptNames() override;

private:
    std::vector<UserRecord> m_users;
    int m_nextId = 200;
};

class SeedRoomTypeService final : public IRoomTypeService {
public:
    explicit SeedRoomTypeService(bool emptyData = false);
    std::vector<RoomTypeRecord> List(const RoomTypeQuery& query) override;
    bool Add(const RoomTypeRecord& record, std::string* error) override;
    bool Update(const RoomTypeRecord& record, std::string* error) override;
    bool Remove(const std::vector<int>& ids, std::string* error) override;
    bool SetStatus(int id, bool enabled, std::string* error) override;

private:
    std::vector<RoomTypeRecord> m_roomTypes;
    int m_nextId = 200;
};

class SeedNursingProjectService final : public INursingProjectService {
public:
    explicit SeedNursingProjectService(bool emptyData = false);
    std::vector<NursingProjectRecord> List(const NursingProjectQuery& query) override;
    bool Add(const NursingProjectRecord& record, std::string* error) override;
    bool Update(const NursingProjectRecord& record, std::string* error) override;
    bool Remove(const std::vector<int>& ids, std::string* error) override;
    bool SetStatus(int id, bool enabled, std::string* error) override;
    std::vector<std::string> ImageLibrary() override;

private:
    std::vector<NursingProjectRecord> m_projects;
    int m_nextId = 200;
};

// M4 入住办理内存种子（仅 --empty-data/DB 不可用时的回退；空态模式返回空集合）
// 字段与 SQLite 种子一致（依据 M4 规格 v0.1；脏数据已清洗、缺失配置/合同已补全）。
class SeedCheckInService final : public ICheckInService {
public:
    explicit SeedCheckInService(bool emptyData = false);
    std::vector<CheckInRecord> List(const CheckInQuery& query) override;
    bool Get(int id, CheckInRecord* outRecord, std::string* error) override;
    bool Add(const CheckInRecord& record, std::string* error) override;
    bool Update(const CheckInRecord& record, std::string* error) override;
    bool Remove(const std::vector<int>& ids, std::string* error) override;
    std::vector<NursingLevelRecord> NursingLevels() override;

private:
    std::vector<CheckInRecord> m_records;
    int m_nextId = 200;
};

}  // namespace zzyl::data
