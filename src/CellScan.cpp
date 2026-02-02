#include "CellScan.h"
#include <unordered_set>
#include "Utils.h"


void CellScanner::RequestRefresh(const std::vector<Request>& requests) {
    const auto gen = requestedGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1;

    auto work = BuildWorkItem_(gen, requests);
    if (!work) {
        return;
    }

    if (work->bases->empty() || work->refInfos->empty()) {
        Publish_(std::move(work->next));
        return;
    }

    QueueScanTask_(std::move(work));
}


CellScanner::CachePtr CellScanner::GetCache() const {
    std::shared_lock lock(cacheMutex_);
    return cache_;
}

bool CellScanner::IsStale_(const std::uint64_t gen) const {
    return gen != requestedGeneration_.load(std::memory_order_acquire);
}

CellScanner::WorkItemPtr CellScanner::BuildWorkItem_(const std::uint64_t gen,
                                                     const std::vector<Request>& requests) const {
    if (IsStale_(gen)) {
        return nullptr;
    }

    auto work = std::make_shared<WorkItem>();
    work->gen = gen;

    work->next = std::make_shared<Cache>();
    work->next->generation = gen;

    work->bases = std::make_shared<std::unordered_set<FormID>>();
    work->refInfos = std::make_shared<std::vector<RefInfo>>();

    work->refInfos->reserve(requests.size());

    // CPU-only: union bases + extract refInfos
    for (auto& [ref_info, bases] : requests) {
        work->refInfos->push_back(ref_info);

        for (auto base : bases) {
            if (base != 0) {
                work->bases->insert(base);
            }
        }
    }

    return work;
}

void CellScanner::QueueScanTask_(WorkItemPtr work) {
    SKSE::GetTaskInterface()->AddTask([this, work]() { RunScanTaskOnGameThread_(work); });
}

void CellScanner::Publish_(std::shared_ptr<Cache> next) {
    if (!next || IsStale_(next->generation)) {
        return;
    }
    std::unique_lock lock(cacheMutex_);
    cache_ = std::move(next);
}

void CellScanner::TryAddExteriorCell_(RE::TESWorldSpace* ws, const std::int32_t x, const std::int32_t y,
                                      std::unordered_set<RE::TESObjectCELL*>& cellsToScan) {
    if (!ws) {
        return;
    }

    const RE::CellID key{static_cast<std::int16_t>(y), static_cast<std::int16_t>(x)};

    if (const auto it = ws->cellMap.find(key); it != ws->cellMap.end()) {
        if (const auto c = it->second; c && !c->IsInteriorCell()) {
            cellsToScan.insert(c);
        }
    }
}

void CellScanner::CollectCellsToScan_(const std::vector<RefInfo>& refInfos,
                                      std::unordered_set<RE::TESObjectCELL*>& cellsToScan) {
    std::unordered_set<RE::TESWorldSpace*> skyAdded;
    skyAdded.reserve(refInfos.size());

    cellsToScan.reserve(refInfos.size() * 10);

    for (const auto& refInfo : refInfos) {
        const auto wo = refInfo.GetRef();
        if (!wo) {
            continue;
        }

        auto cell = wo->GetParentCell();
        if (!cell) {
            continue;
        }

        cellsToScan.insert(cell);

        if (cell->IsInteriorCell()) {
            continue;
        }

        auto ws = wo->GetWorldspace();
        if (!ws) {
            continue;
        }

        if (const auto coords = cell->GetCoordinates()) {
            const std::int32_t x = coords->cellX;
            const std::int32_t y = coords->cellY;

            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                for (std::int32_t dy = -1; dy <= 1; ++dy) {
                    TryAddExteriorCell_(ws, x + dx, y + dy, cellsToScan);
                }
            }
        }

        if (skyAdded.insert(ws).second) {
            if (auto sky = ws->GetSkyCell()) {
                cellsToScan.insert(sky);
            }
        }
    }
}

void CellScanner::ScanCells_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                             const std::unordered_set<FormID>& basesOfInterest, Cache& outCache) {
    for (const auto cell : cellsToScan) {
        if (!cell) {
            continue;
        }

        auto callback = [&outCache, &basesOfInterest](const RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
            if (!ref || ref->IsDisabled() || ref->IsDeleted() || ref->IsMarkedForDeletion()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            const auto base = ref->GetObjectReference();
            if (!base) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            const auto baseID = base->GetFormID();
            if (!basesOfInterest.contains(baseID)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            Entry e;
            e.refid = ref->GetFormID();
            e.pos = Utils::WorldObject::GetPosition(ref);
            outCache.byBase[baseID].push_back(e);

            return RE::BSContainer::ForEachResult::kContinue;
        };

        cell->ForEachReference(callback);
    }
}

void CellScanner::RunScanTaskOnGameThread_(const WorkItemPtr& work) {
    if (!work || IsStale_(work->gen)) {
        return;
    }

    std::unordered_set<RE::TESObjectCELL*> cellsToScan;
    CollectCellsToScan_(*work->refInfos, cellsToScan);

    ScanCells_(cellsToScan, *work->bases, *work->next);

    if (IsStale_(work->gen)) {
        return;
    }

    Publish_(std::move(work->next));
}