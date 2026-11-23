#pragma once

// =============================================================================
// 极简 ECS 核心：World（实体 + 组件存储 + 资源）
//
// 设计目标：
//  - 实体内为 uint32 句柄，用于 UI 树 / 页面元素的稳定标识；
//  - 组件用类型擦除池（IComponentPool）管理，访问 O(1)；
//  - 资源（Resource）用于存放全局单例数据（窗口快照、主题、路由、会话、
//    数据快照、命令队列等），与组件区分开，避免滥用实体；
//  - 刻意不做稀疏集 / 原型存储 / 系统依赖图等重型设施——本项目是桌面 UI
//    应用，实体数量 < 1000，更看重可读性与确定行为。详见架构文档第 3 章。
// =============================================================================

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace zzyl {

// 实体句柄；0 保留为无效实体。
using Entity = std::uint32_t;
inline constexpr Entity kInvalidEntity = 0u;

// -----------------------------------------------------------------------------
// 组件池接口（类型擦除，World 只持有基类指针）
// -----------------------------------------------------------------------------
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual bool Has(Entity entity) const = 0;
    virtual void Remove(Entity entity) = 0;
    virtual std::size_t Size() const = 0;
    virtual void Clear() = 0;
};

template <typename T>
class ComponentPool final : public IComponentPool {
public:
    T& Add(Entity entity, T value) {
        auto it = m_data.find(entity);
        if (it == m_data.end()) {
            it = m_data.emplace(entity, std::move(value)).first;
        } else {
            it->second = std::move(value);
        }
        return it->second;
    }

    T* TryGet(Entity entity) {
        auto it = m_data.find(entity);
        return it == m_data.end() ? nullptr : &it->second;
    }

    const T* TryGet(Entity entity) const {
        auto it = m_data.find(entity);
        return it == m_data.end() ? nullptr : &it->second;
    }

    bool Has(Entity entity) const override { return m_data.find(entity) != m_data.end(); }
    void Remove(Entity entity) override { m_data.erase(entity); }
    std::size_t Size() const override { return m_data.size(); }
    void Clear() override { m_data.clear(); }

    auto& Data() { return m_data; }
    const auto& Data() const { return m_data; }

private:
    std::unordered_map<Entity, T> m_data;
};

// -----------------------------------------------------------------------------
// 资源持有器（全局单例数据）
// -----------------------------------------------------------------------------
class IResourceHolder {
public:
    virtual ~IResourceHolder() = default;
};

template <typename T>
class ResourceHolder final : public IResourceHolder {
public:
    template <typename... Args>
    explicit ResourceHolder(Args&&... args) : value(std::forward<Args>(args)...) {}

    T value;
};

// -----------------------------------------------------------------------------
// World：实体注册表 + 组件池 + 资源表
// -----------------------------------------------------------------------------
class World {
public:
    // ---- 实体 ----
    Entity CreateEntity() {
        const Entity entity = m_nextEntity++;
        m_alive.insert(entity);
        return entity;
    }

    void DestroyEntity(Entity entity) {
        if (!IsAlive(entity)) {
            return;
        }
        for (auto& [type, pool] : m_pools) {
            pool->Remove(entity);
        }
        m_alive.erase(entity);
    }

    bool IsAlive(Entity entity) const { return m_alive.find(entity) != m_alive.end(); }
    std::size_t EntityCount() const { return m_alive.size(); }
    std::size_t ComponentTypeCount() const { return m_pools.size(); }

    template <typename F>
    void EachEntity(F&& fn) {
        for (Entity entity : m_alive) {
            fn(entity);
        }
    }

    // ---- 组件 ----
    template <typename T, typename... Args>
    T& Add(Entity entity, Args&&... args) {
        assert(IsAlive(entity) && "Add component to a dead entity");
        return Pool<T>().Add(entity, T{std::forward<Args>(args)...});
    }

    template <typename T>
    T* TryGet(Entity entity) {
        return Pool<T>().TryGet(entity);
    }

    template <typename T>
    const T* TryGet(Entity entity) const {
        return const_cast<World*>(this)->Pool<T>().TryGet(entity);
    }

    template <typename T>
    T& Get(Entity entity) {
        T* component = TryGet<T>(entity);
        assert(component != nullptr && "Get component that was not added");
        return *component;
    }

    template <typename T>
    bool Has(Entity entity) const {
        return const_cast<World*>(this)->Pool<T>().Has(entity);
    }

    template <typename T>
    void Remove(Entity entity) {
        Pool<T>().Remove(entity);
    }

    // 单组件遍历
    template <typename T, typename F>
    void Each(F&& fn) {
        auto& pool = Pool<T>();
        for (auto& [entity, component] : pool.Data()) {
            if (IsAlive(entity)) {
                fn(entity, component);
            }
        }
    }

    // 双组件遍历
    template <typename T, typename U, typename F>
    void Each(F&& fn) {
        auto& pool = Pool<T>();
        for (auto& [entity, component] : pool.Data()) {
            if (!IsAlive(entity)) {
                continue;
            }
            U* other = TryGet<U>(entity);
            if (other != nullptr) {
                fn(entity, component, *other);
            }
        }
    }

    template <typename T>
    Entity FindFirst() {
        Entity found = kInvalidEntity;
        Each<T>([&found](Entity entity, T&) {
            if (found == kInvalidEntity) {
                found = entity;
            }
        });
        return found;
    }

    // ---- 资源 ----
    // 首次插入 / 已存在时返回既有实例（忽略新参数）。
    template <typename T, typename... Args>
    T& EmplaceResource(Args&&... args) {
        const std::type_index key(typeid(T));
        auto it = m_resources.find(key);
        if (it == m_resources.end()) {
            it = m_resources
                     .emplace(key, std::make_unique<ResourceHolder<T>>(std::forward<Args>(args)...))
                     .first;
        }
        return static_cast<ResourceHolder<T>*>(it->second.get())->value;
    }

    // 覆盖写（要求 T 可移动赋值）；不存在则新建。
    template <typename T>
    T& SetResource(T value) {
        const std::type_index key(typeid(T));
        auto it = m_resources.find(key);
        if (it == m_resources.end()) {
            it = m_resources.emplace(key, std::make_unique<ResourceHolder<T>>(std::move(value))).first;
        } else {
            static_cast<ResourceHolder<T>*>(it->second.get())->value = std::move(value);
        }
        return static_cast<ResourceHolder<T>*>(it->second.get())->value;
    }

    template <typename T>
    T* TryResource() {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            return nullptr;
        }
        return &static_cast<ResourceHolder<T>*>(it->second.get())->value;
    }

    template <typename T>
    const T* TryResource() const {
        return const_cast<World*>(this)->TryResource<T>();
    }

    template <typename T>
    T& Resource() {
        T* resource = TryResource<T>();
        assert(resource != nullptr && "Resource was not registered");
        return *resource;
    }

    template <typename T>
    const T& Resource() const {
        return const_cast<World*>(this)->Resource<T>();
    }

    std::size_t ResourceCount() const { return m_resources.size(); }

private:
    template <typename T>
    ComponentPool<T>& Pool() {
        const std::type_index key(typeid(T));
        auto it = m_pools.find(key);
        if (it == m_pools.end()) {
            auto pool = std::make_unique<ComponentPool<T>>();
            ComponentPool<T>* raw = pool.get();
            m_pools.emplace(key, std::move(pool));
            return *raw;
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

    Entity m_nextEntity = 1;
    std::unordered_set<Entity> m_alive;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_pools;
    std::unordered_map<std::type_index, std::unique_ptr<IResourceHolder>> m_resources;
};

}  // namespace zzyl
