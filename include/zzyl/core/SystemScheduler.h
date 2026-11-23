#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "zzyl/core/System.h"

namespace zzyl {

// 系统调度器：按 (Phase, order, 注册序号) 稳定排序后顺序执行。
class SystemScheduler {
public:
    // order 仅在同一 Phase 内生效，数值小的先执行。
    void Add(std::unique_ptr<ISystem> system, int order = 0);

    void Start(World& world);
    void Update(World& world, const FrameContext& frame);
    void Clear();

    std::size_t Count() const { return m_entries.size(); }
    const char* NameAt(std::size_t index) const;

private:
    struct Entry {
        std::unique_ptr<ISystem> system;
        int order = 0;
        std::size_t sequence = 0;
    };

    std::vector<Entry> m_entries;
    std::size_t m_sequence = 0;
};

}  // namespace zzyl
