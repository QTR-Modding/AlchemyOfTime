#include "InventoryWatch.h"
#include "CellScan.h"


bool InventoryWatch::HasChanged(RE::TESObjectREFR* owner) {
    const auto cache = Scanner::GetSingleton()->GetCache();
    const auto ownerID = owner->GetFormID();
    const auto matches = [&](const RE::TESForm* form) {
        return cache->HasMatch(ownerID, form->GetFormID());
    };

    for (const auto location : locations) {
        if (matches(location) != Utils::IsInLocation(location, last_location)) return true;
    }
    return perks.HasChanged(matches) || magic_effects.HasChanged(matches) || factions.HasChanged(matches) ||
           temp_effects.HasChanged(matches);
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
        case RE::FormType::ArtObject:
        case RE::FormType::EffectShader:
            return temp_effects.Eval(trigger, [&] { return inventory.HasTempEffect(owner, trigger); });
        default:
            return false;
    }
}