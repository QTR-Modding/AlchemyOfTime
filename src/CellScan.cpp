#include "CellScan.h"
#include <unordered_set>
#include "Triggers.h"


void LazyCache::Flush() const {
    std::scoped_lock lock(temp_mutex_, info_mutex_);
    temp_effs_.reset();
    mg_effs_.clear();
    keywords_.clear();
    locations_.clear();
}

std::shared_ptr<const std::unordered_map<RE::ObjectRefHandle, std::unordered_set<RE::TESForm*>,
                                       RE::BSCRC32<RE::ObjectRefHandle>>> LazyCache::GetTempEffects() const {
    std::lock_guard lock(temp_mutex_);
    if (!temp_effs_) {
        std::vector<std::pair<RE::ObjectRefHandle, RE::TESForm*>> targets;
        if (const auto processLists = RE::ProcessLists::GetSingleton()) {
            processLists->ForEachMagicTempEffect([&](RE::BSTempEffect* tempEffect) {
                const auto effect = tempEffect->As<RE::ModelReferenceEffect>();
                if (effect && !effect->finished && effect->artObject) {
                    if (effect->target) targets.emplace_back(effect->target, effect->artObject);
                } else if (const auto shader = tempEffect->As<RE::ShaderReferenceEffect>();
                           shader && !shader->finished && shader->effectData) {
                    if (shader->target) targets.emplace_back(shader->target, shader->effectData);
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
        if (targets.empty()) {
            static const auto empty = std::make_shared<const std::unordered_map<
                RE::ObjectRefHandle, std::unordered_set<RE::TESForm*>, RE::BSCRC32<RE::ObjectRefHandle>>>();
            temp_effs_ = empty;
            return temp_effs_;
        }
        auto collected = std::make_shared<std::unordered_map<RE::ObjectRefHandle, std::unordered_set<RE::TESForm*>,
                                                              RE::BSCRC32<RE::ObjectRefHandle>>>();
        for (const auto& [target, form] : targets) {
            (*collected)[target].insert(form);
        }
        temp_effs_ = std::move(collected);
    }
    return temp_effs_;
}

std::shared_ptr<const std::unordered_set<RE::TESForm*>> LazyCache::GetTempEffects(RE::TESObjectREFR* a_ref) const {
    auto effects = GetTempEffects();
    if (const auto it = effects->find(a_ref->GetHandle()); it != effects->end()) {
        return {std::move(effects), &it->second};
    }
    static const auto empty = std::make_shared<const std::unordered_set<RE::TESForm*>>();
    return empty;
}

std::shared_ptr<const std::unordered_set<RE::EffectSetting*>> LazyCache::GetMagicEffects(RE::TESObjectREFR* a_ref) const {
    const auto handle = a_ref->GetHandle();
    std::lock_guard lock(info_mutex_);
    auto& effects = mg_effs_[handle];
    if (!effects) {
        std::shared_ptr<std::unordered_set<RE::EffectSetting*>> collected;
        if (const auto target = a_ref->GetMagicTarget()) {
            if (const auto list = target->GetActiveEffectList()) {
                for (const auto effect : *list) {
                    if (!effect || effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled) ||
                        effect->conditionStatus == RE::ActiveEffect::ConditionStatus::kFalse) continue;
                    if (const auto base = effect->GetBaseObject()) {
                        if (!collected) collected = std::make_shared<std::unordered_set<RE::EffectSetting*>>();
                        collected->insert(base);
                    }
                }
            }
        }
        if (collected) {
            effects = std::move(collected);
        } else {
            static const auto empty = std::make_shared<const std::unordered_set<RE::EffectSetting*>>();
            effects = empty;
        }
    }
    return effects;
}

std::shared_ptr<const std::unordered_set<RE::BGSKeyword*>> LazyCache::GetKeywords(const RE::TESObjectREFR* a_ref) const {
    static const auto empty = std::make_shared<const std::unordered_set<RE::BGSKeyword*>>();
    // keywords map is from base form id to keywords, so we need to get the base form of the reference
    const auto base = a_ref->GetObjectReference();
    if (!base) {
        return empty;
    }
    std::lock_guard lock(info_mutex_);
    auto& keywords = keywords_[base->GetFormID()];
    if (!keywords) {
        std::shared_ptr<std::unordered_set<RE::BGSKeyword*>> collected;
        if (const auto kw_form = base->As<RE::BGSKeywordForm>()) {
            for (const auto keyword : kw_form->GetKeywords()) {
                if (!collected) collected = std::make_shared<std::unordered_set<RE::BGSKeyword*>>();
                collected->insert(keyword);
            }
        }
        keywords = collected ? std::move(collected) : empty;
    }
    return keywords;
}

RE::BGSLocation* LazyCache::GetLocation(RE::TESObjectREFR* a_ref) const {
    const auto handle = a_ref->GetHandle();
    std::lock_guard lock(info_mutex_);
    const auto [it, inserted] = locations_.try_emplace(handle);
    if (inserted) it->second = a_ref->GetCurrentLocation();
    return it->second;
}

bool LazyCache::HasTempEff(RE::TESObjectREFR* a_ref, RE::TESForm* a_tempeff) const {
    const auto effects = GetTempEffects();
    if (const auto it = effects->find(a_ref->GetHandle()); it != effects->end()) {
        return it->second.contains(a_tempeff);
    }
    return false;
}

bool LazyCache::HasMgEff(RE::TESObjectREFR* a_ref, RE::EffectSetting* a_magiceff) const {
    return GetMagicEffects(a_ref)->contains(a_magiceff);
}


bool LazyCache::HasKeyword(const RE::TESObjectREFR* a_ref, RE::BGSKeyword* a_keyword) const {
    return GetKeywords(a_ref)->contains(a_keyword);
}

MatchType GetMatchType_(const RE::FormType type) {
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

Matcher GetMatcher_(const MatchType type) {
    switch (type) {
        case MatchType::kBaseObject:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form) {
                return ref->GetObjectReference() == form;
            };

        case MatchType::kLocation:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form) {
                return Utils::IsInLocation(static_cast<RE::BGSLocation*>(form), LazyCache::GetSingleton()->GetLocation(ref));
            };

        case MatchType::kPerk:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form) {
                return static_cast<RE::BGSPerk*>(form)->perkConditions.IsTrue(ref, ref);
            };

        case MatchType::kKeyword:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form) {
                return LazyCache::GetSingleton()->HasKeyword(ref, static_cast<RE::BGSKeyword*>(form));
            };

        case MatchType::kFaction:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form) {
                return Utils::WorldObject::MatchesFaction(ref, static_cast<RE::TESFaction*>(form));
            };

        case MatchType::kMagicEffect:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form) {
                return LazyCache::GetSingleton()->HasMgEff(ref, static_cast<RE::EffectSetting*>(form));
            };

        case MatchType::kTempEffect:
            return [](RE::TESObjectREFR* ref, RE::TESForm* form) {
                return LazyCache::GetSingleton()->HasTempEff(ref, form);
            };
    }

    return nullptr;
}



// 1. `Matches`: **Does this ref have this property ? **Returns a bool.
bool Matches(RE::TESObjectREFR* ref, RE::TESForm* property) {
    const auto type = GetMatchType_(property->GetFormType());
    const auto matcher = GetMatcher_(type);
    return matcher && matcher(ref, property);
}

namespace {
    // 2. `ForEachMatch_`: **Which of these properties does this ref have? **Calls `record` for every match.
    template <class Record>
    void ForEachMatch_(RE::TESObjectREFR* ref, const MatchType type,
                       const std::unordered_set<RE::TESForm*>& forms, Record&& record) {
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
                recordMulti(*LazyCache::GetSingleton()->GetKeywords(ref));
                return;

            case MatchType::kMagicEffect:
                recordMulti(*LazyCache::GetSingleton()->GetMagicEffects(ref));
                return;

            case MatchType::kTempEffect: {
                const auto effects = LazyCache::GetSingleton()->GetTempEffects();
                if (const auto it = effects->find(ref->GetHandle()); it != effects->end()) {
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
                    if (matcher(ref, form)) record(form);
                }
            }
        }
    }
}

void Scanner::RequestRefresh(std::vector<Request> requests, const bool preload_temp_effects) {
    auto work = BuildWorkItem_();
    LazyCache::GetSingleton()->Flush();

    if (preload_temp_effects || std::ranges::any_of(requests, [](const Request& request) {
        return std::ranges::any_of(request.forms, [](const ScopedForm& entry) {
            return GetMatchType_(entry.form->GetFormType()) == MatchType::kTempEffect;
        });
    })) {
        // ReSharper disable once CppExpressionWithoutSideEffects
        LazyCache::GetSingleton()->GetTempEffects();
    }

    if (!work) {
        return;
    }

    work->requests = std::move(requests);
    QueueScanTask_(std::move(work));
}

void Scanner::Reset() {
    auto next = std::make_shared<Cache>();
    {
        std::unique_lock lock(cacheMutex_);
        next->generation = requestedGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1;
        cache_ = std::move(next);
    }
    LazyCache::GetSingleton()->Flush();
}


Scanner::CachePtr Scanner::GetCache() const {
    std::shared_lock lock(cacheMutex_);
    return cache_;
}

bool Scanner::IsStale_(const std::uint64_t gen) const {
    return gen != requestedGeneration_.load(std::memory_order_acquire);
}

Scanner::WorkItemPtr Scanner::BuildWorkItem_() {
    std::unique_lock lock(cacheMutex_);
    if (!activeWork_.expired()) return nullptr;

    auto work = std::make_shared<WorkItem>();
    work->gen = requestedGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1;
    work->next = std::make_shared<Cache>();
    work->next->generation = work->gen;
    activeWork_ = work;

    return work;
}

void Scanner::QueueScanTask_(const WorkItemPtr& work) {
    if (IsStale_(work->gen)) return;

    if (work->requests.empty()) {
        Publish_(std::move(work->next));
        return;
    }
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
                                      Cache& outCache) {

    std::optional<Entry> entry;
    const auto addEntry = [&](const RE::TESForm* form) {
        if (!entry) {
            entry.emplace(ref->GetFormID(), Utils::WorldObject::GetPosition(ref));
        }
        outCache.byTrigger[form->GetFormID()].try_emplace(entry->refid, entry->pos);
    };

    for (const auto& [type, forms] : triggers) {
        ForEachMatch_(ref, type, forms, addEntry);
    }
}

void Scanner::ScanCells_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                         const std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>>& triggers,
                         Cache& outCache) {
    for (const auto cell : cellsToScan) {
        if (!cell) {
            continue;
        }


        auto callback = [&outCache, &triggers](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
            if (!ref || !Utils::WorldObject::IsActive(ref)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            CollectCellMatches_(ref, triggers, outCache);
            return RE::BSContainer::ForEachResult::kContinue;
        };

        cell->ForEachReference(callback);
    }
}

void Scanner::ScanTempEffectTargets_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                                     const std::unordered_set<RE::TESForm*>& forms,
                                     Cache& outCache) {
    if (cellsToScan.empty()) return;

    const auto collected = LazyCache::GetSingleton()->GetTempEffects();
    for (const auto& [handle, effects] : *collected) {
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
            const auto type = GetMatchType_(a_form->GetFormType());
            if (a_scope.any(Scope::kSelf) && GetMatcher_(type)(a_ref, a_form)) {
                if (!entry) {
                    entry.emplace(a_ref->GetFormID(), Utils::WorldObject::GetPosition(a_ref));
                }
                work->next->byTrigger[a_form->GetFormID()].try_emplace(entry->refid, entry->pos);
                continue;
            }

            if (!a_scope.any(Scope::kProximity)) continue;

            proximity_forms[type].insert(a_form);

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
        ScanCells_(cells, proximity_forms, *work->next);
    }
    if (!temp_forms.empty()) {
        ScanTempEffectTargets_(cells, temp_forms.mapped(), *work->next);
    }
    Publish_(std::move(work->next));
}