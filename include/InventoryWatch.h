#pragma once
#include <unordered_set>
#include "CustomObjects.h"

// it contains the results from the last check/loop so it can compare it in the next update when the new info comes
struct InventoryWatch {  // inventory owner watch
    RE::BGSLocation* last_location = nullptr;
    std::unordered_set<RE::BGSLocation*> locations;
    std::unordered_map<RE::BGSPerk*, bool> perks;
    std::unordered_map<RE::EffectSetting*, bool> magic_effects;
    std::unordered_map<RE::TESFaction*, bool> factions;

    [[nodiscard]] bool HasChanged(RE::TESObjectREFR* owner);
};


// inventory path's ticket into that shared loop.
// 1. `Manager::UpdateWO` creates a **`RefStop`** to queue the world item's next update.
// 2. `Manager::UpdateInventory` creates a **`QueueInfo`**, passes it through inventory trigger selection, then wraps it
// in `RefStop` for queuing.
// 3. The shared loop later works with **`QueueInfo` copies** of those queued records.
struct QueueInfo {
    enum class UpdateFlag : std::uint32_t {
        kWorldObject = 1u << 0,
        kInventory = 1u << 1,
    };
    RefInfo ref_info;
    REX::EnumSet<UpdateFlag> update_flags;
    float stop_time = 0;
    std::shared_ptr<InventoryWatch> inventory_watch;
};