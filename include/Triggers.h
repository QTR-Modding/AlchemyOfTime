#pragma once
#include "CellScan.h"
#include "InventoryWatch.h"
#include "Stages.h"
#include "Presets.h"

enum class UpdatePhase { kCatchUp, kCurrent, kScheduled };

constexpr auto poll_matchtypes =
    std::array{MatchType::kLocation, MatchType::kPerk, MatchType::kMagicEffect, MatchType::kFaction,
               MatchType::kTempEffect};

REX::EnumSet<Scanner::Scope> GetWorldTriggerScope(RE::TESForm* form);

struct Triggers {
    std::vector<Scanner::ScopedForm> ordered; // from configs, in the given order
    std::vector<Scanner::ScopedForm> scan_forms;
    bool preload_temp_effects{false};
};

bool PassesActivationDelay(FormID trigger, FormID active_trigger,
                           const std::unordered_map<FormID, float>& activation_delays, UpdatePhase phase);

FormID FindWorldTrigger(RE::TESObjectREFR* a_obj, const Triggers& triggers, const StageInstance& instance,
                        const AddOnSettings& settings, UpdatePhase phase);

FormID GetTriggerInInventory(std::ranges::input_range auto&& trigger_ids,
                             const std::unordered_map<FormID, std::unordered_set<StageNo>>& allowed_stages,
                             const std::unordered_map<FormID, std::unordered_set<FormID>>& containers,
                             const std::unordered_map<FormID, float>& activation_delays,
                             const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                             const StageInstance& instance, UpdatePhase phase);

FormID GetTriggerInInventory(std::ranges::input_range auto&& trigger_ids,
                             const std::unordered_map<FormID, std::unordered_set<StageNo>>& allowed_stages,
                             const std::unordered_map<FormID, std::unordered_set<FormID>>& containers,
                             const std::unordered_map<FormID, float>& activation_delays,
                             const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                             const StageInstance& instance,
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

        const auto type = GetMatchType_(trigger->GetFormType());
        const bool polls_conditions = std::ranges::contains(poll_matchtypes, type);
        if (polls_conditions && phase == UpdatePhase::kCatchUp) {
            if (a_trigger_id == instance.GetDelayerFormID()) {
                return a_trigger_id;
            }
            continue;
        }
        const auto owner = polls_conditions ? queue_info.ref_info.GetRef() : nullptr;
        if (polls_conditions && !owner) {
            continue;
        }
        if (phase != UpdatePhase::kCatchUp &&
            (polls_conditions || activation_delays.contains(a_trigger_id)) && !queue_info.inventory_watch) {
            queue_info.inventory_watch = std::make_shared<InventoryWatch>();
            queue_info.update_type = QueueInfo::UpdateType::kInventory;
        }
        bool matches = false;
        if (polls_conditions) {
            matches = queue_info.inventory_watch->Eval(trigger, type, owner, inventory);
        } else if (const auto keyword = trigger->As<RE::BGSKeyword>()) {
            matches = inventory.HasKeyword(keyword);
        } else if (const auto obj = trigger->As<RE::TESBoundObject>()) {
            if (const auto it = inv.find(obj); it != inv.end() && it->second.first > 0) {
                matches = true;
            }
        }
        if (matches && PassesActivationDelay(a_trigger_id, instance.GetDelayerFormID(), activation_delays, phase)) {
            return a_trigger_id;
        }
    }
    return 0;
}