#pragma once
#include <REX/REX/Singleton.h>
#include <shared_mutex>
#include <unordered_set>
#include "CustomObjects.h"
#include "tsl/ordered_map.h"

struct LazyCache : REX::Singleton<LazyCache> {
    void Flush() const;

    std::shared_ptr<const std::unordered_map<RE::ObjectRefHandle, std::unordered_set<RE::TESForm*>,
                                           RE::BSCRC32<RE::ObjectRefHandle>>> GetTempEffects() const;
    std::shared_ptr<const std::unordered_set<RE::TESForm*>> GetTempEffects(RE::TESObjectREFR* a_ref) const;
    std::shared_ptr<const std::unordered_set<RE::EffectSetting*>> GetMagicEffects(RE::TESObjectREFR* a_ref) const;
    std::shared_ptr<const std::unordered_set<RE::BGSKeyword*>> GetKeywords(const RE::TESObjectREFR* a_ref) const;

    RE::BGSLocation* GetLocation(RE::TESObjectREFR* a_ref) const;
    bool HasTempEff(RE::TESObjectREFR* a_ref, RE::TESForm* a_tempeff) const;
    bool HasMgEff(RE::TESObjectREFR* a_ref, RE::EffectSetting* a_magiceff) const;
    bool HasKeyword(const RE::TESObjectREFR* a_ref, RE::BGSKeyword* a_keyword) const;

private:
    using MagicEffectsPtr = std::shared_ptr<const std::unordered_set<RE::EffectSetting*>>;
    using MagicEffectsEntry = std::pair<RE::ObjectRefHandle, MagicEffectsPtr>;

    mutable std::mutex temp_mutex_;
    mutable std::mutex info_mutex_;
    mutable std::shared_ptr<const std::unordered_map<RE::ObjectRefHandle, std::unordered_set<RE::TESForm*>,
                                                   RE::BSCRC32<RE::ObjectRefHandle>>> temp_effs_;
    mutable tsl::ordered_map<
        RE::ObjectRefHandle, MagicEffectsPtr, RE::BSCRC32<RE::ObjectRefHandle>,
        std::equal_to<RE::ObjectRefHandle>, std::allocator<MagicEffectsEntry>,
        std::vector<MagicEffectsEntry>> mg_effs_;
    mutable std::unordered_map<FormID, std::shared_ptr<const std::unordered_set<RE::BGSKeyword*>>> keywords_;
    mutable std::unordered_map<RE::ObjectRefHandle, RE::BGSLocation*, RE::BSCRC32<RE::ObjectRefHandle>> locations_;
};

enum class MatchType { kBaseObject, kLocation, kPerk, kKeyword, kFaction, kMagicEffect, kTempEffect };

MatchType GetMatchType_(RE::FormType type);

using Matcher = bool (*)(RE::TESObjectREFR*, RE::TESForm*);

Matcher GetMatcher_(MatchType type);

bool Matches(RE::TESObjectREFR* ref, RE::TESForm* property);


class Scanner final :
    public REX::Singleton<Scanner> {
public:
    struct Entry {
        RefID refid{0};
        RE::NiPoint3 pos{};
    };

    struct Cache {
        std::uint64_t generation{0};
        std::unordered_map<FormID, tsl::ordered_map<
                               RefID, RE::NiPoint3, std::hash<RefID>, std::equal_to<RefID>,
                               std::allocator<std::pair<RefID, RE::NiPoint3>>,
                               std::vector<std::pair<RefID, RE::NiPoint3>>>> byTrigger;
    };

    using CachePtr = std::shared_ptr<const Cache>;

    enum class Scope : std::uint8_t { kSelf = 1 << 0, kProximity = 1 << 1 };

    struct ScopedForm {
        RE::TESForm* form;
        REX::EnumSet<Scope> scope;
    };

    struct Request {
        RefInfo ref;
        std::vector<ScopedForm> forms;
    };

    void RequestRefresh(std::vector<Request> requests, bool preload_temp_effects);
    void Reset();

    [[nodiscard]] CachePtr GetCache() const;

private:
    struct WorkItem {
        std::uint64_t gen{0};

        // Built off-thread
        std::shared_ptr<Cache> next;
        std::vector<Request> requests;
    };

    using WorkItemPtr = std::shared_ptr<WorkItem>;

    [[nodiscard]] bool IsStale_(std::uint64_t gen) const;

    [[nodiscard]] WorkItemPtr BuildWorkItem_();

    void QueueScanTask_(const WorkItemPtr& work);

    void RunScanTaskOnGameThread_(const WorkItemPtr& work);

    // Game-thread helpers
    static void TryAddExteriorCell_(RE::TESWorldSpace* ws, std::int32_t x, std::int32_t y,
                                    std::unordered_set<RE::TESObjectCELL*>& cellsToScan);

    static void CollectCellsToScan_(const std::vector<RE::TESObjectREFR*>& refs,
                                    std::unordered_set<RE::TESObjectCELL*>& cellsToScan);

    static void CollectCellMatches_(RE::TESObjectREFR* ref,
                                    const std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>>& triggers,
                                    Cache& outCache);

    static void ScanCells_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                           const std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>>& triggers,
                           Cache& outCache);

    static void ScanTempEffectTargets_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                                       const std::unordered_set<RE::TESForm*>& forms,
                                       Cache& outCache);

    void Publish_(std::shared_ptr<Cache> next);

    mutable std::shared_mutex cacheMutex_{};
    CachePtr cache_{std::make_shared<Cache>()};
    std::weak_ptr<WorkItem> activeWork_;

    std::atomic<std::uint64_t> requestedGeneration_{0};
};