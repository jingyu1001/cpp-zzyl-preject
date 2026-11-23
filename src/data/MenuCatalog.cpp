#include "zzyl/data/MenuCatalog.h"

namespace zzyl::data {
namespace {

MenuEntry Item(std::string title, std::string icon, std::string route) {
    MenuEntry entry;
    entry.title = std::move(title);
    entry.icon = std::move(icon);
    entry.route = std::move(route);
    return entry;
}

}  // namespace

MenuCatalog BuildDefaultMenuCatalog() {
    // 依据：Web 端 admin 账号 /getRouters 实际菜单（见 Web架构提取 §4.4、UI复刻规格 §4.2）
    MenuCatalog catalog;

    // 0. 首页（静态路由 /index，affix 标签，不可关闭）
    catalog.roots.push_back(Item("首页", "dashboard", "/index"));

    // 1. 中州智询
    {
        MenuEntry group = Item("中州智询", "ai", "/aiconsult");
        group.children.push_back(Item("小智", "agent", "/aiconsult/aiconsult"));
        group.children.push_back(Item("知识库", "knowledge", "/aiconsult/knowledge"));
        catalog.roots.push_back(std::move(group));
    }

    // 2. 来访管理
    {
        MenuEntry group = Item("来访管理", "visit", "/appointment");
        group.children.push_back(Item("预约信息", "calendar", "/appointment/comeVisit"));
        catalog.roots.push_back(std::move(group));
    }

    // 3. 入退管理
    {
        MenuEntry group = Item("入退管理", "checkinout", "/enterQuit");
        group.children.push_back(Item("入住办理", "checkin", "/enterQuit/enterDetails"));
        catalog.roots.push_back(std::move(group));
    }

    // 4. 在住管理
    {
        MenuEntry group = Item("在住管理", "livein", "/liveIn");
        group.children.push_back(Item("房型设置", "room", "/liveIn/roomType"));
        group.children.push_back(Item("床位预览", "bed", "/liveIn/houseType"));
        catalog.roots.push_back(std::move(group));
    }

    // 5. 服务管理
    {
        MenuEntry group = Item("服务管理", "nursing", "/nursing");
        group.children.push_back(Item("护理项目", "service", "/nursing/project"));
        group.children.push_back(Item("护理计划", "plan", "/nursing/plan"));
        group.children.push_back(Item("护理等级", "level", "/nursing/level"));
        group.children.push_back(Item("负责老人", "elder", "/nursing/oldPeople"));
        catalog.roots.push_back(std::move(group));
    }

    // 6. 智能监测
    {
        MenuEntry group = Item("智能监测", "monitor-device", "/intelligence");
        group.children.push_back(Item("设备管理", "device", "/intelligence/device"));
        group.children.push_back(Item("报警规则", "rule", "/intelligence/alertRule"));
        group.children.push_back(Item("报警数据", "alert", "/intelligence/alertData"));
        catalog.roots.push_back(std::move(group));
    }

    // 7. 系统管理
    {
        MenuEntry group = Item("系统管理", "system", "/system");
        group.children.push_back(Item("用户管理", "user", "/system/user"));
        group.children.push_back(Item("角色管理", "role", "/system/role"));
        group.children.push_back(Item("菜单管理", "menu", "/system/menu"));
        group.children.push_back(Item("部门管理", "dept", "/system/dept"));
        group.children.push_back(Item("岗位管理", "post", "/system/post"));
        group.children.push_back(Item("字典管理", "dict", "/system/dict"));
        group.children.push_back(Item("参数设置", "config", "/system/config"));
        group.children.push_back(Item("通知公告", "notice", "/system/notice"));
        group.children.push_back(Item("操作日志", "operlog", "/monitor/operlog"));
        group.children.push_back(Item("登录日志", "loginlog", "/monitor/logininfor"));
        catalog.roots.push_back(std::move(group));
    }

    // 8. 系统监控
    {
        MenuEntry group = Item("系统监控", "monitor", "/monitor");
        group.children.push_back(Item("在线用户", "online", "/monitor/online"));
        group.children.push_back(Item("定时任务", "job", "/monitor/job"));
        group.children.push_back(Item("数据监控", "druid", "/monitor/druid"));
        group.children.push_back(Item("服务监控", "server", "/monitor/server"));
        group.children.push_back(Item("缓存监控", "cache", "/monitor/cache"));
        group.children.push_back(Item("缓存列表", "cache-list", "/monitor/cacheList"));
        catalog.roots.push_back(std::move(group));
    }

    // 9. 系统工具
    {
        MenuEntry group = Item("系统工具", "tool", "/tool");
        group.children.push_back(Item("表单构建", "build", "/tool/build"));
        group.children.push_back(Item("代码生成", "gen", "/tool/gen"));
        group.children.push_back(Item("系统接口", "swagger", "/tool/swagger"));
        catalog.roots.push_back(std::move(group));
    }

    return catalog;
}

}  // namespace zzyl::data
