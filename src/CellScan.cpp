#include "CellScan.h"
#include <unordered_set>
#include "Utils.h"


void CellScanner::RequestRefresh(const std::vector<Request>& requests) {
    const auto gen = requestedGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1;

    auto work = BuildWorkItem_(gen, requests);
    if (!work) {
        return;
    }

    if (work->triggers.empty() || work->refInfos->empty()) {
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

void CellScanner::WorkItem::AddTrigger(RE::TESForm* form) {
    if (!form) return;
    MatchType matchType;
    switch (form->GetFormType()) {
        case RE::FormType::MagicEffect:
            matchType = MatchType::kMagicEffect;
            break;
        case RE::FormType::ArtObject:
        case RE::FormType::EffectShader:
            matchType = MatchType::kTempEffect;
            break;
        case RE::FormType::LocationRefType:
        case RE::FormType::Action:
        case RE::FormType::Keyword:
            matchType = MatchType::kKeyword;
            break;
        case RE::FormType::Faction:
            matchType = MatchType::kFaction;
            break;
        default:
            if (form->GetFormID() == 0) return;
            matchType = MatchType::kBaseObject;
            break;
    }
    triggers[matchType].insert(form);
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

    work->refInfos = std::make_shared<std::vector<RefInfo>>();

    work->refInfos->reserve(requests.size());

    // CPU-only: union trigger forms + extract refInfos
    for (auto& [ref_info, forms] : requests) {
        work->refInfos->push_back(ref_info);

        for (auto form : forms) {
            work->AddTrigger(form);
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

void CellScanner::AddEntry_(const FormID triggerID, const RE::TESObjectREFR* ref, Cache& outCache) {
    Entry e;
    e.refid = ref->GetFormID();
    e.pos = Utils::WorldObject::GetPosition(ref);
    outCache.byTrigger[triggerID].push_back(e);
}

void CellScanner::CollectCellMatches_(RE::TESObjectREFR* ref,
                                      const std::unordered_map<MatchType, std::unordered_set<RE::TESForm*>>& triggers,
                                      Cache& outCache) {
    const auto base = ref->GetObjectReference();
    if (!base) return;

    for (const auto& [type, forms] : triggers) {
        switch (type) {
            case MatchType::kBaseObject:
                if (forms.contains(base)) AddEntry_(base->GetFormID(), ref, outCache);
                break;
            case MatchType::kKeyword:
                if (const auto keywords = base->As<RE::BGSKeywordForm>()) {
                    for (const auto keyword : keywords->GetKeywords()) {
                        if (!keyword || !forms.contains(keyword)) continue;
                        AddEntry_(keyword->GetFormID(), ref, outCache);
                    }
                }
                break;
            case MatchType::kMagicEffect:
                for (const auto effect : Utils::WorldObject::CollectActiveMagicEffects(ref)) {
                    if (forms.contains(effect)) AddEntry_(effect->GetFormID(), ref, outCache);
                }
                break;
            case MatchType::kFaction:
                if (const auto actor = ref->As<RE::Actor>()) {
                    for (const auto form : forms) {
                        const auto faction = static_cast<RE::TESFaction*>(form);
                        if (actor->IsInFaction(faction)) AddEntry_(faction->GetFormID(), ref, outCache);
                    }
                } else if (const auto faction = ref->GetFactionOwner(); forms.contains(faction)) {
                    AddEntry_(faction->GetFormID(), ref, outCache);
                }
                break;
            case MatchType::kTempEffect:
                break;
        }
    }
}

void CellScanner::ScanCells_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
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

void CellScanner::ScanAppliedEffects_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                                      const std::unordered_set<RE::TESForm*>& forms, Cache& outCache) {
    for (const auto& [form, handle] : Utils::WorldObject::CollectAppliedEffectTargets(forms)) {
        const auto ref = handle.get();
        if (!ref || !Utils::WorldObject::IsActive(ref.get()) ||
            !cellsToScan.contains(ref->GetParentCell())) continue;

        AddEntry_(form->GetFormID(), ref.get(), outCache);
    }
}

void CellScanner::RunScanTaskOnGameThread_(const WorkItemPtr& work) {
    if (!work || IsStale_(work->gen)) {
        return;
    }

    std::unordered_set<RE::TESObjectCELL*> cellsToScan;
    CollectCellsToScan_(*work->refInfos, cellsToScan);

    const auto it = work->triggers.find(MatchType::kTempEffect);

    // Base objects, keywords, factions and magic effects are read from each reference.
    if (it == work->triggers.end() || work->triggers.size() > 1) {
        ScanCells_(cellsToScan, work->triggers, *work->next);
    }
    // ArtObject and EffectShader instances are read from the game's shared temp-effect list,
    if (it != work->triggers.end()) {
        ScanAppliedEffects_(cellsToScan, it->second, *work->next);
    }

    if (IsStale_(work->gen)) {
        return;
    }

    Publish_(std::move(work->next));
}