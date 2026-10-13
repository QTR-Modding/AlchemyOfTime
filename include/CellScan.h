#pragma once
#include <REX/REX/Singleton.h>
#include <shared_mutex>
#include <unordered_set>
#include "CustomObjects.h"

class CellScanner final :
    public REX::Singleton<CellScanner> {
public:
    static constexpr std::array applied_effect_trigger_types{RE::FormType::ArtObject, RE::FormType::EffectShader, RE::FormType::MagicEffect};

    struct Entry {
        RefID refid{0};
        RE::NiPoint3 pos{};
    };

    struct Cache {
        std::uint64_t generation{0};
        std::unordered_map<FormID, std::vector<Entry>> byTrigger;
    };

    using CachePtr = std::shared_ptr<const Cache>;

    using Request = std::pair<RefInfo, std::vector<RE::TESForm*>>;

    void RequestRefresh(const std::vector<Request>& requests);

    [[nodiscard]] CachePtr GetCache() const;

private:
    enum class MatchType {
        kBaseObject,
        kKeyword,
        kFaction,
        kMagicEffect,
        kTempEffect
    };

    struct WorkItem {
        std::uint64_t gen{0};

        // Built off-thread
        std::shared_ptr<Cache> next;
        std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>> triggers;
        std::shared_ptr<std::vector<RefInfo>> refInfos;

        void AddTrigger(RE::TESForm* form);
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

    static void CollectCellMatches_(RE::TESObjectREFR* ref,
                                    const std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>>& triggers,
                                    Cache& outCache);

    static void ScanCells_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                           const std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>>& triggers,
                           Cache& outCache);

    static void ScanAppliedEffects_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                                    const std::unordered_set<RE::TESForm*>& forms, Cache& outCache);

    void Publish_(std::shared_ptr<Cache> next);

    mutable std::shared_mutex cacheMutex_{};
    CachePtr cache_{std::make_shared<Cache>()};

    std::atomic<std::uint64_t> requestedGeneration_{0};
};