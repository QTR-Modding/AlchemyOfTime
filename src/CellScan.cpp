#include "CellScan.h"
#include <unordered_set>
#include "Triggers.h"


const std::unordered_map<RE::ObjectRefHandle, std::unordered_set<RE::TESForm*>, RE::BSCRC32<RE::ObjectRefHandle>>&
LazyCache::GetTempEffects() const {
    if (!temp_effs) {
        temp_effs = Utils::WorldObject::CollectTempEffects();
    }
    return *temp_effs;
}

const std::unordered_set<RE::EffectSetting*>& LazyCache::GetMagicEffects(RE::TESObjectREFR* a_ref) const {
    const auto [it, inserted] = mg_effs.try_emplace(a_ref->GetFormID());
    if (inserted) {
        it->second = Utils::WorldObject::CollectMagicEffects(a_ref);
    }
    return it->second;
}

const std::unordered_set<RE::BGSKeyword*>& LazyCache::GetKeywords(RE::TESObjectREFR* a_ref) const {
    // keywords map is from base form id to keywords, so we need to get the base form of the reference
    const auto base = a_ref->GetObjectReference();
    if (!base) {
        static const std::unordered_set<RE::BGSKeyword*> empty;
        return empty;
    }
    const auto [it, inserted] = keywords.try_emplace(base->GetFormID());
    if (inserted) {
        if (const auto kw_form = base->As<RE::BGSKeywordForm>()) {
            for (const auto keyword : kw_form->GetKeywords()) {
                it->second.insert(keyword);
            }
        }
    }
    return it->second;
}

RE::BGSLocation* LazyCache::GetLocation(RE::TESObjectREFR* a_ref) const {
    const auto [it, inserted] = locations.try_emplace(a_ref->GetFormID());
    if (inserted) it->second = a_ref->GetCurrentLocation();
    return it->second;
}

bool LazyCache::HasTempEff(RE::TESObjectREFR* a_ref, RE::TESForm* a_tempeff) const {
    const auto& effects = GetTempEffects();
    if (const auto it = effects.find(a_ref->GetHandle()); it != effects.end()) {
        return it->second.contains(a_tempeff);
    }
    return false;
}

bool LazyCache::HasMgEff(RE::TESObjectREFR* a_ref, RE::EffectSetting* a_magiceff) const {
    return GetMagicEffects(a_ref).contains(a_magiceff);
}


bool LazyCache::HasKeyword(RE::TESObjectREFR* a_ref, RE::BGSKeyword* a_magiceff) const {
    return GetKeywords(a_ref).contains(a_magiceff);
}

MatchType GetMatchType_(RE::FormType type) {
    switch (type) {
        case RE::FormType::Location:
            return MatchType::kLocation;
        case RE::FormType::Perk:
            return MatchType::kPerk;
        case RE::FormType::MagicEffect:
            return MatchType::kMagicEffect;
        case RE::FormType::ArtObject:
        case RE::FormType::EffectShader:
            return MatchType::kTempEffect;
        case RE::FormType::LocationRefType:
        case RE::FormType::Action:
        case RE::FormType::Keyword:
            return MatchType::kKeyword;
        case RE::FormType::Faction:
            return MatchType::kFaction;
        default:
            return MatchType::kBaseObject;
    }
}

Matcher GetMatcher_(MatchType type) {
    switch (type) {
        case MatchType::kBaseObject:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form, const LazyCache&) {
                return ref->GetObjectReference() == form;
            };

        case MatchType::kLocation:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form, const LazyCache& global_cache) {
                return Utils::IsInLocation(static_cast<RE::BGSLocation*>(form), global_cache.GetLocation(ref));
            };

        case MatchType::kPerk:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form, const LazyCache&) {
                return static_cast<RE::BGSPerk*>(form)->perkConditions.IsTrue(ref, ref);
            };

        case MatchType::kKeyword:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form, const LazyCache& global_cache) {
                return global_cache.HasKeyword(ref, static_cast<RE::BGSKeyword*>(form));
            };

        case MatchType::kFaction:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form, const LazyCache&) {
                return Utils::WorldObject::MatchesFaction(ref, static_cast<RE::TESFaction*>(form));
            };

        case MatchType::kMagicEffect:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form, const LazyCache& global_cache) {
                return global_cache.HasMgEff(ref, static_cast<RE::EffectSetting*>(form));
            };

        case MatchType::kTempEffect:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form, const LazyCache& global_cache) {
                return global_cache.HasTempEff(ref, form);
            };
    }

    return nullptr;
}



// 1. `Matches`: **Does this ref have this property ? **Returns a bool.
bool Matches(RE::TESObjectREFR* ref, RE::TESForm* property, const LazyCache& global_cache) {
    const auto type = GetMatchType_(property->GetFormType());
    const auto matcher = GetMatcher_(type);
    return matcher && matcher(ref, property, global_cache);
}

namespace {
    // 2. `ForEachMatch_`: **Which of these properties does this ref have? **Calls `record` for every match.
    template <class Record>
    void ForEachMatch_(RE::TESObjectREFR* ref, MatchType type,
                       const std::unordered_set<RE::TESForm*>& forms,
                       const LazyCache& bank, Record&& record) {
        const auto recordMulti = [&](const auto& effects) {
            for (const auto form : effects) {
                if (forms.contains(form)) record(form);
            }
        };

        switch (type) {
            case MatchType::kBaseObject: {
                const auto base = ref->GetObjectReference();
                if (base && forms.contains(base)) {
                    record(base);
                }
                return;
            }

            case MatchType::kKeyword:
                recordMulti(bank.GetKeywords(ref));
                return;

            case MatchType::kMagicEffect:
                recordMulti(bank.GetMagicEffects(ref));
                return;

            case MatchType::kTempEffect: {
                const auto& effects = bank.GetTempEffects();
                if (const auto it = effects.find(ref->GetHandle()); it != effects.end()) {
                    recordMulti(it->second);
                }
                return;
            }

            case MatchType::kFaction: {
                if (const auto actor = ref->As<RE::Actor>()) {
                    for (const auto form : forms) {
                        if (actor->IsInFaction(static_cast<RE::TESFaction*>(form))) record(form);
                    }
                } else if (const auto faction = ref->GetFactionOwner();
                           faction && forms.contains(faction)) {
                    record(faction);
                }
                return;
            }

            default: {
                const auto matcher = GetMatcher_(type);
                if (!matcher) return;

                for (const auto form : forms) {
                    if (matcher(ref, form, bank)) record(form);
                }
            }
        }
    }
}

void Scanner::RequestRefresh(std::vector<Request> requests) {
    const auto gen = requestedGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1;

    auto work = BuildWorkItem_(gen, std::move(requests));
    if (!work) {
        return;
    }

    if (work->requests.empty()) {
        Publish_(std::move(work->next));
        return;
    }

    QueueScanTask_(std::move(work));
}


Scanner::CachePtr Scanner::GetCache() const {
    std::shared_lock lock(cacheMutex_);
    return cache_;
}

std::unordered_set<RE::TESForm*> Scanner::GetTempEffects(RE::TESObjectREFR* owner) {
    std::unique_lock lock(cacheMutex_);
    const bool initialize = !cache_->global_cache.temp_effs;
    const auto& effects = cache_->global_cache.GetTempEffects();
    if (initialize) {
        // Discard refreshes captured before this initial observation completed.
        requestedGeneration_.fetch_add(1, std::memory_order_acq_rel);
    }
    const auto it = effects.find(owner->GetHandle());
    return it != effects.end() ? it->second : std::unordered_set<RE::TESForm*>{};
}

bool Scanner::IsStale_(const std::uint64_t gen) const {
    return gen != requestedGeneration_.load(std::memory_order_acquire);
}

Scanner::WorkItemPtr Scanner::BuildWorkItem_(const std::uint64_t gen,
                                                     std::vector<Request>&& requests) const {
    if (IsStale_(gen)) return nullptr;

    auto work = std::make_shared<WorkItem>();
    work->gen = gen;
    work->next = std::make_shared<Cache>();
    work->next->generation = gen;
    work->requests = std::move(requests);
    for (const auto& request : work->requests) {
        for (const auto& entry : request.forms) {
            if (GetMatchType_(entry.form->GetFormType()) == MatchType::kTempEffect) {
                work->collect_temp_effects = true;
            }
        }
    }

    return work;
}

void Scanner::QueueScanTask_(const WorkItemPtr& work) {
    if (IsStale_(work->gen)) return;

    if (work->collect_temp_effects) {
        // ReSharper disable once CppExpressionWithoutSideEffects
        work->next->global_cache.GetTempEffects();
    }

    if (IsStale_(work->gen)) return;
    SKSE::GetTaskInterface()->AddTask([this, work]() { RunScanTaskOnGameThread_(work); });
}

void Scanner::Publish_(std::shared_ptr<Cache> next) {
    if (!next) return;
    std::unique_lock lock(cacheMutex_);
    if (IsStale_(next->generation)) return;
    cache_ = std::move(next);
}

void Scanner::TryAddExteriorCell_(RE::TESWorldSpace* ws, const std::int32_t x, const std::int32_t y,
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

void Scanner::CollectCellsToScan_(const std::vector<RE::TESObjectREFR*>& refs,
                                      std::unordered_set<RE::TESObjectCELL*>& cellsToScan) {
    std::unordered_set<RE::TESWorldSpace*> skyAdded;
    skyAdded.reserve(refs.size());

    cellsToScan.reserve(refs.size() * 10);

    for (const auto wo : refs) {
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

void Scanner::CollectCellMatches_(RE::TESObjectREFR* ref,
                                      const std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>>& triggers,
                                      const LazyCache& global_cache,
                                      Cache& outCache) {

    std::optional<Entry> entry;
    const auto addEntry = [&](const RE::TESForm* form) {
        if (!entry) {
            entry.emplace(ref->GetFormID(), Utils::WorldObject::GetPosition(ref));
        }
        outCache.byTrigger[form->GetFormID()].try_emplace(entry->refid, entry->pos);
    };

    for (const auto& [type, forms] : triggers) {
        ForEachMatch_(ref, type, forms, global_cache, addEntry);
    }
}

void Scanner::ScanCells_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                         const std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>>& triggers,
                         const LazyCache& global_cache,
                         Cache& outCache) {
    for (const auto cell : cellsToScan) {
        if (!cell) {
            continue;
        }


        auto callback = [&outCache, &triggers, &global_cache](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
            if (!ref || !Utils::WorldObject::IsActive(ref)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            CollectCellMatches_(ref, triggers, global_cache, outCache);
            return RE::BSContainer::ForEachResult::kContinue;
        };

        cell->ForEachReference(callback);
    }
}

void Scanner::ScanTempEffectTargets_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                                     const std::unordered_set<RE::TESForm*>& forms,
                                     const LazyCache& global_cache, Cache& outCache) {
    if (cellsToScan.empty()) return;

    for (const auto& [handle, effects] : global_cache.GetTempEffects()) {
        std::optional<Entry> entry;
        for (const auto form : effects) {
            if (!forms.contains(form)) continue;

            if (!entry) {
                const auto ref = handle.get();
                if (!ref || !Utils::WorldObject::IsActive(ref.get()) ||
                    !cellsToScan.contains(ref->GetParentCell())) break;
                entry.emplace(ref->GetFormID(), Utils::WorldObject::GetPosition(ref.get()));
            }
            outCache.byTrigger[form->GetFormID()].try_emplace(entry->refid, entry->pos);
        }
    }
}

void Scanner::RunScanTaskOnGameThread_(const WorkItemPtr& work) {
    if (IsStale_(work->gen)) return;

    std::vector<RE::TESObjectREFR*> cellrefs;
    std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>> proximity_forms;
    for (auto& [ref_info, forms] : work->requests) {
        bool added = false;
        const auto a_ref = ref_info.GetRef();
        if (!a_ref) continue;
        std::optional<Entry> entry;

        for (auto& [a_form, a_scope] : forms) {
            if (a_scope.any(Scope::kSelf) && Matches(a_ref, a_form, work->next->global_cache)) {
                if (!entry) {
                    entry.emplace(a_ref->GetFormID(), Utils::WorldObject::GetPosition(a_ref));
                }
                work->next->byTrigger[a_form->GetFormID()].try_emplace(entry->refid, entry->pos);
                continue;
            }

            if (!a_scope.any(Scope::kProximity)) continue;

            proximity_forms[GetMatchType_(a_form->GetFormType())].insert(a_form);

            if (!added) {
                cellrefs.push_back(a_ref);
                added = true;
            }
        }
    }

    std::unordered_set<RE::TESObjectCELL*> cells;
    CollectCellsToScan_(cellrefs, cells);
    const auto temp_forms = proximity_forms.extract(MatchType::kTempEffect);
    if (!proximity_forms.empty()) {
        ScanCells_(cells, proximity_forms, work->next->global_cache, *work->next);
    }
    if (!temp_forms.empty()) {
        ScanTempEffectTargets_(cells, temp_forms.mapped(), work->next->global_cache, *work->next);
    }
    Publish_(std::move(work->next));
}