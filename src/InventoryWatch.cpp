#include "InventoryWatch.h"
#include "Utils.h"


bool InventoryWatch::HasChanged(RE::TESObjectREFR* owner) {
    if (!locations.empty()) {
        const auto current = owner->GetCurrentLocation();
        if (current != last_location) {
            for (const auto location : locations) {
                if (Utils::IsInLocation(location, last_location) != Utils::IsInLocation(location, current)) {
                    return true;
                }
            }
            last_location = current;
        }
    }
    for (const auto& [perk, matched] : perks) {
        if (perk->perkConditions.IsTrue(owner, owner) != matched) return true;
    }
    if (!magic_effects.empty()) {
        const auto active = Utils::WorldObject::CollectActiveMagicEffects(owner);
        for (const auto& [effect, matched] : magic_effects) {
            if (active.contains(effect) != matched) return true;
        }
    }
    for (const auto& [faction, matched] : factions) {
        if (Utils::WorldObject::MatchesFaction(owner, faction) != matched) return true;
    }
    return false;
}