#include "InventoryWatch.h"
#include "CellScan.h"


bool InventoryWatch::HasChanged(RE::TESObjectREFR* owner) {
    if (!locations.list.empty()) {
        const auto current = LazyCache::GetSingleton()->GetLocation(owner);
        if (current != last_location) {
            if (locations.HasChanged([&](RE::BGSLocation* location) {
                return Utils::IsInLocation(location, current);
            })) return true;
            last_location = current;
        }
    }

    const auto matches = [&](RE::TESForm* form) {
        return Matches(owner, form);
    };
    const Utils::Inventory::Snapshot inventory{};

    return perks.HasChanged(matches) ||
           magic_effects.HasChanged([&](RE::EffectSetting* effect) { return inventory.HasMagicEffect(owner, effect); }) ||
           factions.HasChanged(matches) ||
           temp_effects.HasChanged([&](RE::TESForm* effect) { return inventory.HasTempEffect(owner, effect); });
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