#pragma once

// =============================================================================
// 数据服务接口（数据层）
//
// 渲染层 / 系统层只依赖这两个抽象接口，不直接碰 MySQL / SQLite3 / HTTP。
// 后续迭代扩展点：
//   - 远程 MySQL：实现 IAuthService / IDashboardService，内部走 HTTP 调 RuoYi 后端，
//     或直连 MySQL（视课程要求）；
//   - 本地 SQLite3：实现离线缓存/会话持久化（见架构文档"数据层设计"）。
// =============================================================================

#include <string>

#include "zzyl/data/Model.h"

namespace zzyl::data {

struct LoginRequest {
    std::string username;
    std::string password;
    std::string code;  // 验证码（本迭代仅占位校验）
};

struct LoginResult {
    bool ok = false;
    std::string message;  // 失败原因（展示给登录页）
    UserSession session;
};

class IAuthService {
public:
    virtual ~IAuthService() = default;

    // 同步登录（本迭代 Mock）。接真实后端时可在实现内做阻塞 IO 或改为异步接口。
    virtual LoginResult Login(const LoginRequest& request) = 0;
    virtual void Logout() {}
};

class IDashboardService {
public:
    virtual ~IDashboardService() = default;

    // 加载首页看板数据（本迭代为内存 Mock；后续可从 MySQL / 本地 SQLite 读）
    virtual DashboardSnapshot LoadDashboard(const UserSession& session) = 0;
};

}  // namespace zzyl::data
