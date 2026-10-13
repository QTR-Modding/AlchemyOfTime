#pragma once
#include "InventoryWatch.h"
#include "Stages.h"

enum class UpdatePhase { kCatchUp, kCurrent };

constexpr auto poll_formtypes =
    std::array{RE::FormType::Location, RE::FormType::Perk, RE::FormType::MagicEffect, RE::FormType::Faction,
               RE::FormType::ArtObject, RE::FormType::EffectShader};

struct Triggers {
    std::vector<RE::TESForm*> ordered;  // from configs, in the given order
    std::vector<RE::TESForm*>
        scan_forms;  // excludes Locations and Perks because those are checked directly on the item
};

FormID FindWorldTrigger(RE::TESObjectREFR* a_obj, const Triggers& triggers);

FormID GetTriggerInInventory(std::ranges::input_range auto&& trigger_ids,
                             const std::unordered_map<FormID, std::unordered_set<StageNo>>& allowed_stages,
                             const std::unordered_map<FormID, std::unordered_set<FormID>>& containers,
                             const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                             const StageInstance& instance, UpdatePhase phase);

FormID GetTriggerInInventory(std::ranges::input_range auto&& trigger_ids,
    const std::unordered_map<FormID, std::unordered_set<StageNo>>& allowed_stages,
    const std::unordered_map<FormID, std::unordered_set<FormID>>& containers,
    const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info, const StageInstance& instance,
    const UpdatePhase phase) {
    const auto& inv = inventory.items;
    if (inv.empty()) return 0;

    for (const auto a_trigger_id : trigger_ids) {
        if (!allowed_stages.at(a_trigger_id).contains(instance.no)) continue;
        if (const auto it = containers.find(a_trigger_id);
            it != containers.end() && !it->second.empty() && !it->second.contains(queue_info.ref_info.base_id)) {
            continue;   
        }
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
                queue_info.update_type = QueueInfo::UpdateType::kInventory;
            }

            if (auto& watch = *queue_info.inventory_watch; watch.Eval(trigger, owner, inventory)) {
                return a_trigger_id;
            }
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