#pragma once
#include "CustomObjects.h"
#include "Utils.h"

template <typename T>
struct InventoryWatchList {
    std::unordered_map<T*, bool> list;

    template <typename Evaluate>
    bool Eval(T* a_trigger, Evaluate&& evaluate) {
        const auto [it, inserted] = list.try_emplace(a_trigger);
        if (inserted) it->second = evaluate();
        return it->second;
    }

    template <typename Evaluate>
    bool HasChanged(Evaluate&& evaluate) const {
        for (const auto& [trigger, previous] : list) {
            if (evaluate(trigger) != previous) return true;
        }
        return false;
    }
};

// it contains the results from the last check/loop so it can compare it in the next update when the new info comes
struct InventoryWatch {  // inventory owner watch
    RE::BGSLocation* last_location = nullptr;
    std::unordered_set<RE::BGSLocation*> locations;
    InventoryWatchList<RE::BGSPerk> perks;
    InventoryWatchList<RE::EffectSetting> magic_effects;
    InventoryWatchList<RE::TESFaction> factions;
    InventoryWatchList<RE::TESForm> temp_effects;

    [[nodiscard]] bool HasChanged(RE::TESObjectREFR* owner);
    bool Eval(RE::TESForm* trigger, RE::TESObjectREFR* owner,
              const Utils::Inventory::Snapshot& inventory);

    template <typename Visit>
    void ForEachTrigger(Visit&& visit) const;
};


// inventory path's ticket into that shared loop.
// 1. `Manager::UpdateWO` creates a **`RefStop`** to queue the world item's next update.
// 2. `Manager::UpdateInventory` creates a **`QueueInfo`**, passes it through inventory trigger selection, then wraps it
// in `RefStop` for queuing.
// 3. The shared loop later works with **`QueueInfo` copies** of those queued records.
struct QueueInfo {
    enum class UpdateType { kNone, kWorldObject, kInventory };
    RefInfo ref_info;
    UpdateType update_type = UpdateType::kNone;
    float stop_time = 0;
    std::shared_ptr<InventoryWatch> inventory_watch;
};


template <typename Visit>
void InventoryWatch::ForEachTrigger(Visit&& visit) const {
    for (const auto trigger : locations) visit(trigger);

    const auto visitList = [&visit](const auto& watch) {
        for (const auto trigger : watch.list | std::views::keys) {
            visit(trigger);
        }
    };

    visitList(perks);
    visitList(magic_effects);
    visitList(factions);
    visitList(temp_effects);
}