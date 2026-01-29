#pragma once
#include <REX/REX/Singleton.h>
#include <shared_mutex>
#include <unordered_set>

#include "CustomObjects.h"

class CellScanner final :
    public REX::Singleton<CellScanner> {
public:
    struct Entry {
        RefID refid{0};
        RE::NiPoint3 pos{};
    };

    struct Cache {
        std::uint64_t generation{0};
        std::unordered_map<FormID, std::vector<Entry>> byBase;
    };

    using CachePtr = std::shared_ptr<const Cache>;

    using Request = std::pair<RefInfo, std::vector<FormID>>;

    void RequestRefresh(const std::vector<Request>& requests);

    [[nodiscard]] CachePtr GetCache() const;

private:
    struct WorkItem {
        std::uint64_t gen{0};

        // Built off-thread
        std::shared_ptr<Cache> next;
        std::shared_ptr<std::unordered_set<FormID>> bases;
        std::shared_ptr<std::vector<RefInfo>> refInfos;
    };

    using WorkItemPtr = std::shared_ptr<WorkItem>;

    [[nodiscard]] bool IsStale_(std::uint64_t gen) const;

    [[nodiscard]] WorkItemPtr BuildWorkItem_(std::uint64_t gen, const std::vector<Request>& requests) const;

    void QueueScanTask_(WorkItemPtr work);

    void RunScanTaskOnGameThread_(const WorkItemPtr& work);

    // Game-thread helpers
    static void TryAddExteriorCell_(RE::TESWorldSpace* ws, std::int32_t x, std::int32_t y,
                                    std::unordered_set<RE::TESObjectCELL*>& cellsToScan);

    static void CollectCellsToScan_(const std::vector<RefInfo>& refInfos,
                                    std::unordered_set<RE::TESObjectCELL*>& cellsToScan);

    static void ScanCells_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                           const std::unordered_set<FormID>& basesOfInterest, Cache& outCache);

    void Publish_(std::shared_ptr<Cache> next);

    mutable std::shared_mutex cacheMutex_{};
    CachePtr cache_{std::make_shared<Cache>()};

    std::atomic<std::uint64_t> requestedGeneration_{0};
};