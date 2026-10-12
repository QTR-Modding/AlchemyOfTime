#pragma once
#include "tsl/ordered_map.h"
#include "Stages.h"
#include "Data.h"


constexpr auto poll_formtypes =
    std::array{RE::FormType::Location, RE::FormType::Perk, RE::FormType::MagicEffect, RE::FormType::Faction};


FormID GetTriggerInInventory(std::ranges::input_range auto&& trigger_ids,
                             const std::unordered_map<FormID, std::unordered_set<StageNo>>& allowed_stages,
                             const std::unordered_map<FormID, std::unordered_set<FormID>>& containers,
                             const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                             const StageInstance& instance, const UpdatePhase phase) {
    const auto& inv = inventory.items;
    if (inv.empty()) return 0;

    for (const auto a_trigger_id : trigger_ids) {
        if (!allowed_stages.at(a_trigger_id).contains(instance.no)) continue;
        if (const auto it = containers.find(a_trigger_id);
            it != containers.end() && !it->second.empty() && !it->second.contains(queue_info.ref_info.base_id))
            continue;

        const auto trigger = RE::TESForm::LookupByID(a_trigger_id);
        if (!trigger) continue;

        if (std::ranges::contains(poll_formtypes, trigger->GetFormType())) {
            if (phase == UpdatePhase::kCatchUp) {
                if (a_trigger_id == instance.GetDelayerFormID()) return a_trigger_id;
                continue;
            }

            const auto owner = queue_info.ref_info.GetRef();
            if (!owner) continue;
            if (!queue_info.inventory_watch) {
                queue_info.inventory_watch = std::make_shared<InventoryWatch>();
                queue_info.inventory_watch->last_location = owner->GetCurrentLocation();
            }

            auto& watch = *queue_info.inventory_watch;
            // ReSharper disable once CppDependentTemplateWithoutTemplateKeyword
            if (const auto location = trigger->As<RE::BGSLocation>()) {
                queue_info.update_flags.set(QueueInfo::UpdateFlag::kLocation);
                watch.locations.insert(location);
                if (Utils::IsInLocation(location, watch.last_location)) return a_trigger_id;
                // ReSharper disable once CppDependentTemplateWithoutTemplateKeyword
            } else if (const auto perk = trigger->As<RE::BGSPerk>()) {
                queue_info.update_flags.set(QueueInfo::UpdateFlag::kPerk);
                const auto [it, inserted] = watch.perks.try_emplace(perk);
                if (inserted) it->second = perk->perkConditions.IsTrue(owner, owner);
                if (it->second) return a_trigger_id;
            } else if (const auto magicEffect = trigger->As<RE::EffectSetting>()) {
                queue_info.update_flags.set(QueueInfo::UpdateFlag::kMagicEffect);
                const auto [it, inserted] = watch.magic_effects.try_emplace(magicEffect);
                if (inserted) it->second = inventory.HasMagicEffect(owner, magicEffect);
                if (it->second) return a_trigger_id;
            } else if (const auto faction = trigger->As<RE::TESFaction>()) {
                queue_info.update_flags.set(QueueInfo::UpdateFlag::kFaction);
                const auto [it, inserted] = watch.factions.try_emplace(faction);
                if (inserted) it->second = Utils::WorldObject::MatchesFaction(owner, faction);
                if (it->second) return a_trigger_id;
            }
            // ReSharper disable once CppDependentTemplateWithoutTemplateKeyword
        } else if (const auto keyword = trigger->As<RE::BGSKeyword>()) {
            if (inventory.HasKeyword(keyword)) return a_trigger_id;
        } else if (const auto obj = trigger->As<RE::TESBoundObject>()) {
            if (const auto it = inv.find(obj); it != inv.end() && it->second.first > 0) {
                return a_trigger_id;
            }
        }
    }
    return 0;
}