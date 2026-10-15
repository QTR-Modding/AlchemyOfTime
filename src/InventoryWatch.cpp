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

    const auto perk_matcher = GetMatcher_(MatchType::kPerk);
    if (perks.HasChanged([&](RE::BGSPerk* perk) { return perk_matcher(owner, perk); })) return true;
    if (!magic_effects.list.empty()) {
        const auto active = LazyCache::GetSingleton()->GetMagicEffects(owner);
        if (magic_effects.HasChanged([&](RE::EffectSetting* effect) { return active->contains(effect); })) return true;
    }
    const auto faction_matcher = GetMatcher_(MatchType::kFaction);
    if (factions.HasChanged([&](RE::TESFaction* faction) { return faction_matcher(owner, faction); })) return true;
    if (!temp_effects.list.empty()) {
        const auto active = LazyCache::GetSingleton()->GetTempEffects(owner);
        return temp_effects.HasChanged([&](RE::TESForm* effect) { return active->contains(effect); });
    }
    return false;
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
            return perks.Eval(perk, [&] { return GetMatcher_(MatchType::kPerk)(owner, perk); });
        }
        case MatchType::kMagicEffect: {
            const auto magicEffect = static_cast<RE::EffectSetting*>(trigger);
            return magic_effects.Eval(magicEffect, [&] { return inventory.HasMagicEffect(owner, magicEffect); });
        }
        case MatchType::kFaction: {
            const auto faction = static_cast<RE::TESFaction*>(trigger);
            return factions.Eval(faction, [&] { return GetMatcher_(MatchType::kFaction)(owner, faction); });
        }
        case MatchType::kTempEffect:
            return temp_effects.Eval(trigger, [&] { return inventory.HasTempEffect(owner, trigger); });
        default:
            return false;
    }
}