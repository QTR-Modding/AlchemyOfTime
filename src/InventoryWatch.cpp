#include "InventoryWatch.h"
#include "CellScan.h"


bool InventoryWatch::HasChanged(const RE::TESObjectREFR* owner) const {
    const auto cache = Scanner::GetSingleton()->GetCache();
    const auto ownerID = owner->GetFormID();
    const auto matches = [&](const RE::TESForm* form) {
        return cache->HasMatch(ownerID, form->GetFormID());
    };

    return locations.HasChanged(matches) || perks.HasChanged(matches) || magic_effects.HasChanged(matches) ||
           factions.HasChanged(matches) || temp_effects.HasChanged(matches);
}

bool InventoryWatch::Eval(RE::TESForm* trigger, RE::TESObjectREFR* owner,
                          const Utils::Inventory::Snapshot& inventory) {
    switch (GetMatchType_(trigger->GetFormType())) {
        case MatchType::kLocation: {
            const auto location = static_cast<RE::BGSLocation*>(trigger);
            return locations.Eval(location, [&] { return Utils::IsInLocation(location, last_location); });
        }
        case MatchType::kPerk: {
            const auto perk = static_cast<RE::BGSPerk*>(trigger);
            return perks.Eval(perk, [&] { return perk->perkConditions.IsTrue(owner, owner); });
        }
        case MatchType::kMagicEffect: {
            const auto magicEffect = static_cast<RE::EffectSetting*>(trigger);
            return magic_effects.Eval(magicEffect, [&] { return inventory.HasMagicEffect(owner, magicEffect); });
        }
        case MatchType::kFaction: {
            const auto faction = static_cast<RE::TESFaction*>(trigger);
            return factions.Eval(faction, [&] { return Utils::WorldObject::MatchesFaction(owner, faction); });
        }
        case MatchType::kTempEffect:
            return temp_effects.Eval(trigger, [&] { return inventory.HasTempEffect(owner, trigger); });
        default:
            return false;
    }
}