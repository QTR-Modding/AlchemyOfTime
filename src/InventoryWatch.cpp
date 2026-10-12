#include "InventoryWatch.h"
#include "Utils.h"


bool InventoryWatch::HasChanged(RE::TESObjectREFR* owner) {
    if (!locations.empty()) {
        if (const auto current = owner->GetCurrentLocation(); current != last_location) {
            for (const auto location : locations) {
                if (Utils::IsInLocation(location, last_location) != Utils::IsInLocation(location, current)) {
                    return true;
                }
            }
            last_location = current;
        }
    }
    if (perks.HasChanged([&](const RE::BGSPerk* perk) { return perk->perkConditions.IsTrue(owner, owner); })) {
        return true;
    }
    if (!magic_effects.list.empty()) {
        const auto active = Utils::WorldObject::CollectActiveMagicEffects(owner);
        if (magic_effects.HasChanged([&](RE::EffectSetting* effect) { return active.contains(effect); })) {
            return true;
        }
    }
    if (factions.HasChanged([&](const RE::TESFaction* faction) { return Utils::WorldObject::MatchesFaction(owner, faction); })) {
        return true;
    }
    if (art_objects.HasChanged([&](RE::BGSArtObject* effect) { return Utils::WorldObject::HasTempEffect(owner, effect); })) {
        return true;
    }
    return effect_shaders.HasChanged([&](RE::TESEffectShader* effect) { return Utils::WorldObject::HasTempEffect(owner, effect); });
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
            return art_objects.Eval(effect, [&] { return Utils::WorldObject::HasTempEffect(owner, effect); });
        }
        case RE::FormType::EffectShader: {
            const auto effect = static_cast<RE::TESEffectShader*>(trigger);
            return effect_shaders.Eval(effect, [&] { return Utils::WorldObject::HasTempEffect(owner, effect); });
        }
        default:
            return false;
    }
}