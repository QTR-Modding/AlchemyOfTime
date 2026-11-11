#pragma once
#include "CellScan.h"
#include "InventoryWatch.h"
#include "Presets.h"
#include "CLibUtilsQTR/BoundingBox.hpp"

enum class UpdatePhase { kCatchUp, kCurrent };

constexpr auto poll_matchtypes =
    std::array{MatchType::kLocation, MatchType::kPerk, MatchType::kMagicEffect, MatchType::kFaction,
               MatchType::kTempEffect};

REX::EnumSet<Scanner::Scope> GetWorldTriggerScope(RE::TESForm* form);

struct Triggers {
    struct Candidate {
        RE::TESForm* form;
        REX::EnumSet<Scanner::Scope> scope;
        TriggerKind kind;
        float activation_delay;
    };

    std::vector<Candidate> ordered; // from configs, in the given order
    std::vector<Scanner::ScopedForm> scan_forms;
    bool preload_temp_effects{false};
};

struct WorldTriggerMatchContext {
    RefID origin_id;
    float radius_squared;
    Scanner::CachePtr cache;
    std::optional<RE::BGSLocation*> current_location;
    std::optional<RE::NiPoint3> origin_pos;
    std::optional<DirectX::BoundingOrientedBox> origin_obb;
    std::vector<std::pair<const Triggers::Candidate*, bool>> proximity_matches;
};

bool MatchesWorldTrigger(RE::TESObjectREFR* a_obj, const Triggers::Candidate& trigger,
                         WorldTriggerMatchContext& context);
std::pair<FormID, TriggerKind> FindWorldTrigger(RE::TESObjectREFR* a_obj, const Triggers& triggers,
                                                FormID source_id, const StageInstance& instance, float update_time,
                                                WorldTriggerMatchContext& context);

bool IsInventoryTriggerAllowed(const AddOnSettings& settings, FormID trigger_id, TriggerKind kind,
                               StageNo stage, FormID container);
bool IsActiveTrigger(const StageInstance& instance, FormID trigger_id, TriggerKind kind);
bool CanActivateTrigger(RefID refid, FormID source_id, const StageInstance& instance,
                        FormID trigger_id, TriggerKind kind, float activation_delay, float update_time);
bool MatchesInventoryTrigger(const AddOnSettings& settings, FormID trigger_id, TriggerKind kind,
                             const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                             const StageInstance& instance, UpdatePhase phase);

std::pair<FormID, TriggerKind> GetTriggerInInventory(const AddOnSettings& settings, FormID source_id,
                                                     const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                                                     const StageInstance& instance, UpdatePhase phase,
                                                     float update_time);