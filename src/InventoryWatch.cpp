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
    for (const auto& [perk, matched] : perks.list) {
        if (perk->perkConditions.IsTrue(owner, owner) != matched) return true;
    }
    if (!magic_effects.list.empty()) {
        const auto active = Utils::WorldObject::CollectActiveMagicEffects(owner);
        for (const auto& [effect, matched] : magic_effects.list) {
            if (active.contains(effect) != matched) return true;
        }
    }
    for (const auto& [faction, matched] : factions.list) {
        if (Utils::WorldObject::MatchesFaction(owner, faction) != matched) return true;
    }
    return false;
}

bool InventoryWatch::Eval(RE::TESForm* trigger, RE::TESObjectREFR* owner,
                          const Utils::Inventory::Snapshot& inventory) {
    switch (trigger->GetFormType()) {
        case RE::FormType::Location: {
            const auto location = static_cast<RE::BGSLocation*>(trigger);
            locations.insert(location);
            return Utils::IsInLocation(location, last_location);
        }
        case RE::FormType::Perk: {
            const auto perk = static_cast<RE::BGSPerk*>(trigger);
            return perks.Eval(perk, [&] { return perk->perkConditions.IsTrue(owner, owner); });
        }
        case RE::FormType::MagicEffect: {
            const auto magicEffect = static_cast<RE::EffectSetting*>(trigger);
            return magic_effects.Eval(magicEffect, [&] { return inventory.HasMagicEffect(owner, magicEffect); });
        }
        case RE::FormType::Faction: {
            const auto faction = static_cast<RE::TESFaction*>(trigger);
            return factions.Eval(faction, [&] { return Utils::WorldObject::MatchesFaction(owner, faction); });
        }
        default:
            return false;
    }
}