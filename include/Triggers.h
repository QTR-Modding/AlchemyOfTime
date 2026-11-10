#pragma once
#include "CellScan.h"
#include "InventoryWatch.h"
#include "Presets.h"

enum class UpdatePhase { kCatchUp, kCurrent };

constexpr auto poll_matchtypes =
    std::array{MatchType::kLocation, MatchType::kPerk, MatchType::kMagicEffect, MatchType::kFaction,
               MatchType::kTempEffect};

REX::EnumSet<Scanner::Scope> GetWorldTriggerScope(RE::TESForm* form);

struct Triggers {
    std::vector<Scanner::ScopedForm> ordered; // from configs, in the given order
    std::vector<Scanner::ScopedForm> scan_forms;
    bool preload_temp_effects{false};
};

FormID FindWorldTrigger(RE::TESObjectREFR* a_obj, const Triggers& triggers);

bool IsInventoryTriggerAllowed(const AddOnSettings& settings, FormID trigger_id, TriggerKind kind,
                               StageNo stage, FormID container);
bool IsActiveInventoryTrigger(const StageInstance& instance, FormID trigger_id, TriggerKind kind);
bool MatchesInventoryTrigger(const AddOnSettings& settings, FormID trigger_id, TriggerKind kind,
                             const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                             const StageInstance& instance, UpdatePhase phase);

std::pair<FormID, TriggerKind> GetTriggerInInventory(const AddOnSettings& settings, FormID source_id,
                            const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                            const StageInstance& instance, UpdatePhase phase);
