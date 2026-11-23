#pragma once

// Mock 数据服务（本迭代不连后端；数据取自 Web 端 dashboard/constants.js）
// 演示账号：admin / admin123（与 RuoYi 默认一致）

#include "zzyl/data/Services.h"

namespace zzyl::data {

class MockAuthService final : public IAuthService {
public:
    LoginResult Login(const LoginRequest& request) override;
    void Logout() override;
};

class MockDashboardService final : public IDashboardService {
public:
    DashboardSnapshot LoadDashboard(const UserSession& session) override;
};

}  // namespace zzyl::data
