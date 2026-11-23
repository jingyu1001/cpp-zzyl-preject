#include "zzyl/core/SystemScheduler.h"

#include <algorithm>

namespace zzyl {

void SystemScheduler::Add(std::unique_ptr<ISystem> system, int order) {
    if (!system) {
        return;
    }
    Entry entry;
    entry.system = std::move(system);
    entry.order = order;
    entry.sequence = m_sequence++;
    m_entries.push_back(std::move(entry));

    std::stable_sort(m_entries.begin(), m_entries.end(), [](const Entry& lhs, const Entry& rhs) {
        const int lhsPhase = static_cast<int>(lhs.system->Phase());
        const int rhsPhase = static_cast<int>(rhs.system->Phase());
        if (lhsPhase != rhsPhase) {
            return lhsPhase < rhsPhase;
        }
        if (lhs.order != rhs.order) {
            return lhs.order < rhs.order;
        }
        return lhs.sequence < rhs.sequence;
    });
}

void SystemScheduler::Start(World& world) {
    for (auto& entry : m_entries) {
        entry.system->OnStart(world);
    }
}

void SystemScheduler::Update(World& world, const FrameContext& frame) {
    for (auto& entry : m_entries) {
        entry.system->Update(world, frame);
    }
}

void SystemScheduler::Clear() {
    m_entries.clear();
    m_sequence = 0;
}

const char* SystemScheduler::NameAt(std::size_t index) const {
    if (index >= m_entries.size()) {
        return "";
    }
    return m_entries[index].system->Name();
}

}  // namespace zzyl
