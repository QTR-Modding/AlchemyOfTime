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
           art_objects.HasChanged(matches) || effect_shaders.HasChanged(matches);
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
        case RE::FormType::ArtObject: {
            const auto effect = static_cast<RE::BGSArtObject*>(trigger);
            return art_objects.Eval(effect, [&] { return inventory.HasTempEffect(owner, effect); });
        }
        case RE::FormType::EffectShader: {
            const auto effect = static_cast<RE::TESEffectShader*>(trigger);
            return effect_shaders.Eval(effect, [&] { return inventory.HasTempEffect(owner, effect); });
        }
        default:
            return false;
    }
}