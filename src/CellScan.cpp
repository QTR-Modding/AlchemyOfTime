#include "CellScan.h"
#include <unordered_set>
#include "Utils.h"


void CellScanner::RequestRefresh(const std::vector<Request>& requests, const std::vector<QueueInfo>& inventories) {
    const auto gen = requestedGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1;

    auto work = BuildWorkItem_(gen, requests, inventories);
    if (!work) {
        return;
    }

    if (((work->bases->empty() && work->keywords.empty() && work->visual_effects.empty() && work->magic_effects.empty() && work->factions.empty()) || work->refInfos->empty()) &&
        work->inventory_owners.empty()) {
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
                                                     const std::vector<Request>& requests,
                                                     const std::vector<QueueInfo>& inventories) const {
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

    // CPU-only: union trigger forms + extract refInfos
    for (auto& [ref_info, forms] : requests) {
        work->refInfos->push_back(ref_info);

        for (auto form : forms) {
            if (!form) continue;
            if (const auto effect = form->As<RE::EffectSetting>()) {
                work->magic_effects.insert(effect);
            } else if (std::ranges::contains(applied_effect_trigger_types, form->GetFormType())) {
                work->visual_effects.insert(form);
            } else if (const auto keyword = form->As<RE::BGSKeyword>()) {
                work->keywords.insert(keyword->GetFormID());
            } else if (const auto faction = form->As<RE::TESFaction>()) {
                work->factions.insert(faction);
            } else if (const auto baseID = form->GetFormID(); baseID != 0) {
                work->bases->insert(baseID);
            }
        }
    }

    for (const auto& inventory : inventories) {
        if (inventory.update_flags.none(QueueInfo::UpdateFlag::kVisualEffect)) continue;
        work->inventory_owners.insert(inventory.ref_info.ref_handle);
        for (const auto form : inventory.inventory_watch->visual_effects | std::views::keys) work->visual_effects.insert(form);
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

void CellScanner::AddEntry_(const FormID triggerID, const RE::TESObjectREFR* ref, Cache& outCache,
                            const bool world_candidate) {
    Entry e;
    e.refid = ref->GetFormID();
    e.pos = Utils::WorldObject::GetPosition(ref);
    e.world_candidate = world_candidate;
    outCache.byTrigger[triggerID].push_back(e);
}

void CellScanner::ScanCells_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                             const std::unordered_set<FormID>& basesOfInterest,
                             const std::unordered_set<FormID>& keywordsOfInterest,
                             const std::unordered_set<RE::EffectSetting*>& magicEffectsOfInterest,
                             const std::unordered_set<RE::TESFaction*>& factionsOfInterest, Cache& outCache) {
    for (const auto cell : cellsToScan) {
        if (!cell) {
            continue;
        }

        auto callback = [&outCache, &basesOfInterest, &keywordsOfInterest, &magicEffectsOfInterest, &factionsOfInterest](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
            if (!ref || !Utils::WorldObject::IsActive(ref)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            const auto base = ref->GetObjectReference();
            if (!base) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            const auto baseID = base->GetFormID();
            if (basesOfInterest.contains(baseID)) {
                AddEntry_(baseID, ref, outCache);
            }

            if (!keywordsOfInterest.empty()) {
                if (const auto keywords = base->As<RE::BGSKeywordForm>()) {
                    for (const auto keyword : keywords->GetKeywords()) {
                        if (!keyword || !keywordsOfInterest.contains(keyword->GetFormID())) continue;
                        AddEntry_(keyword->GetFormID(), ref, outCache);
                    }
                }
            }

            if (!magicEffectsOfInterest.empty()) {
                for (const auto effect : Utils::WorldObject::CollectActiveMagicEffects(ref)) {
                    if (magicEffectsOfInterest.contains(effect)) AddEntry_(effect->GetFormID(), ref, outCache);
                }
            }

            if (!factionsOfInterest.empty()) {
                if (const auto actor = ref->As<RE::Actor>()) {
                    for (const auto faction : factionsOfInterest) {
                        if (actor->IsInFaction(faction)) AddEntry_(faction->GetFormID(), ref, outCache);
                    }
                } else if (const auto faction = ref->GetFactionOwner(); factionsOfInterest.contains(faction)) {
                    AddEntry_(faction->GetFormID(), ref, outCache);
                }
            }

            return RE::BSContainer::ForEachResult::kContinue;
        };

        cell->ForEachReference(callback);
    }
}

void CellScanner::ScanAppliedEffects_(const std::unordered_set<RE::TESObjectCELL*>& cellsToScan,
                                      const std::unordered_set<RE::TESForm*>& forms,
                                      const std::unordered_set<RE::ObjectRefHandle, RE::BSCRC32<RE::ObjectRefHandle>>& inventories,
                                      Cache& outCache) {
    for (const auto& [form, handle] : Utils::WorldObject::CollectAppliedEffectTargets(forms)) {
        const auto ref = handle.get();
        if (!ref) continue;

        const bool world_candidate = Utils::WorldObject::IsActive(ref.get()) && cellsToScan.contains(ref->GetParentCell());
        if (!world_candidate && !inventories.contains(handle)) continue;

        AddEntry_(form->GetFormID(), ref.get(), outCache, world_candidate);
    }
}

void CellScanner::RunScanTaskOnGameThread_(const WorkItemPtr& work) {
    if (!work || IsStale_(work->gen)) {
        return;
    }

    std::unordered_set<RE::TESObjectCELL*> cellsToScan;
    CollectCellsToScan_(*work->refInfos, cellsToScan);

    if (!work->bases->empty() || !work->keywords.empty() || !work->magic_effects.empty() || !work->factions.empty()) {
        ScanCells_(cellsToScan, *work->bases, work->keywords, work->magic_effects, work->factions, *work->next);
    }
    if (!work->visual_effects.empty()) {
        ScanAppliedEffects_(cellsToScan, work->visual_effects, work->inventory_owners, *work->next);
    }

    if (IsStale_(work->gen)) {
        return;
    }

    Publish_(std::move(work->next));
}