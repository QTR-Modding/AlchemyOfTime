#pragma once
#include <unordered_set>
#include "CustomObjects.h"


struct InventoryWatch {  // inventory owner watch
    RE::BGSLocation* last_location = nullptr;
    std::unordered_set<RE::BGSLocation*> locations;
    std::unordered_map<RE::BGSPerk*, bool> perks;
    std::unordered_map<RE::EffectSetting*, bool> magic_effects;
    std::unordered_map<RE::TESFaction*, bool> factions;

    [[nodiscard]] bool HasChanged(RE::TESObjectREFR* owner);
};

struct QueueInfo {
    enum class UpdateFlag : std::uint32_t {
        kWorldObject = 1u << 0,
        kLocation = 1u << 1,
        kPerk = 1u << 2,
        kMagicEffect = 1u << 3,
        kFaction = 1u << 4
    };
    RefInfo ref_info;
    REX::EnumSet<UpdateFlag> update_flags;
    float stop_time = 0;
    std::shared_ptr<InventoryWatch> inventory_watch;
};