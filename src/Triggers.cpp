#include "Triggers.h"
#include "ActivationQueue.h"
#include "CellScan.h"
#include "Settings.h"
#include "CLibUtilsQTR/DrawDebug.hpp"
#ifndef NDEBUG
    #include "MCP.h"
#endif

namespace {

    bool AreClose(const DirectX::BoundingOrientedBox& obb1_in, const DirectX::BoundingOrientedBox& obb2_in,
                  const float threshold) {
        DirectX::BoundingOrientedBox obb1 = obb1_in;

        obb1.Extents.x += threshold;
        obb1.Extents.y += threshold;
        obb1.Extents.z += threshold;

        return obb1.Intersects(obb2_in);
    }

    bool SearchModulatorInCell_Sub(const RE::TESObjectREFR* a_origin, const RE::TESObjectREFR* ref,
                                   std::optional<DirectX::BoundingOrientedBox>& originOBB,
                                   const float proximity = Settings::proximity_range) {
        const auto cell = a_origin->GetParentCell();
        const auto targetCell = ref->GetParentCell();
        if (!cell || !targetCell ||
            (cell != targetCell && (cell->IsInteriorCell() || targetCell->IsInteriorCell() ||
                                    !a_origin->GetWorldspace() || a_origin->GetWorldspace() != ref->GetWorldspace())))
            return false;

        DirectX::BoundingOrientedBox obb2{};

        if (!originOBB) {
            originOBB.emplace();
            if (const auto a_rigidbody1 = BoundingBox::GetRigidBody(a_origin)) {
                BoundingBox::GetOBB(a_rigidbody1, *originOBB);
            } else {
                BoundingBox::GetOBB(a_origin, *originOBB);
            }
        }
        if (const auto a_rigidbody2 = BoundingBox::GetRigidBody(ref)) {
            BoundingBox::GetOBB(a_rigidbody2, obb2);
        } else {
            BoundingBox::GetOBB(ref, obb2);
        }

        if (!AreClose(*originOBB, obb2, proximity)) {
            return false;
        }

#ifndef NDEBUG
        if (UI::draw_debug) {
            DebugAPI_IMPL::DrawDebug::DrawOBB(obb2);
            DebugAPI_IMPL::DrawDebug::DrawOBB(*originOBB);

            DebugAPI_IMPL::DrawDebug::draw_line(Utils::WorldObject::GetPosition(ref),
                                                Utils::WorldObject::GetPosition(RE::PlayerCharacter::GetSingleton()),
                                                3.f, RE::NiColorA(0.f, 0.f, 1.f, 1.f));

            const RE::NiPoint3 c1{originOBB->Center.x, originOBB->Center.y, originOBB->Center.z};
            const RE::NiPoint3 c2{obb2.Center.x, obb2.Center.y, obb2.Center.z};
            // yellow debug line between centers
            DebugAPI_IMPL::DrawDebug::draw_line(c1, c2, 2, RE::NiColorA(1.f, 1.f, 0.f, 1.f));
        }
#endif

        return true;
    }
}

REX::EnumSet<Scanner::Scope> GetWorldTriggerScope(RE::TESForm* form) {
    switch (GetMatchType_(form->GetFormType())) {
        case MatchType::kLocation:
        case MatchType::kPerk:
            return {Scanner::Scope::kSelf};
        case MatchType::kTempEffect:
        case MatchType::kMagicEffect:
        case MatchType::kFaction:
        case MatchType::kKeyword:
            return {Scanner::Scope::kSelf, Scanner::Scope::kProximity};
        default:
            if (form->As<RE::TESBoundObject>()) {
                return {Scanner::Scope::kProximity};
            }
            return {};
    }
}


bool MatchesWorldTrigger(RE::TESObjectREFR* a_obj, const Triggers::Candidate& trigger,
                         WorldTriggerMatchContext& context) {
    const auto form = trigger.form;
    const auto scope = trigger.scope;
    const auto triggerID = form->GetFormID();
    if (!scope.any(Scanner::Scope::kProximity)) {
        if (scope.any(Scanner::Scope::kSelf)) {
            if (const auto location = form->As<RE::BGSLocation>()) {
                if (!context.current_location) {
                    context.current_location = LazyCache::GetSingleton()->GetLocation(a_obj);
                }
                return Utils::IsInLocation(location, *context.current_location);
            }
            return Matches(a_obj, form);
        }
        return false;
    }

    if (!context.cache) {
        context.cache = Scanner::GetSingleton()->GetCache();
    }
    const auto it = context.cache->byTrigger.find(triggerID);
    if (it == context.cache->byTrigger.end()) {
        return false;
    }
    if (scope.any(Scanner::Scope::kSelf) && it->second.contains(context.origin_id)) {
        return true;
    }

    for (const auto& [refid, pos] : it->second) {
        if (!context.origin_pos) {
            context.origin_pos = Utils::WorldObject::GetPosition(a_obj);
        }
        if (context.origin_pos->GetSquaredDistance(pos) > context.radius_squared) {
            continue;
        }
        const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid);
        if (!ref || !Utils::WorldObject::IsActive(ref)) {
            continue;
        }
        if (SearchModulatorInCell_Sub(a_obj, ref, context.origin_obb)) {
            return true;
        }
    }
    return false;
}

std::pair<FormID, TriggerKind> FindWorldTrigger(RE::TESObjectREFR* a_obj, const Triggers& triggers,
                                               const FormID source_id, const StageInstance& instance) {
    if (!a_obj || triggers.ordered.empty()) {
        return {};
    }

    const float r = Settings::search_radius;
    WorldTriggerMatchContext context{
        .origin_id = a_obj->GetFormID(),
        .radius_squared = (r > 0.0f) ? (r * r) : std::numeric_limits<float>::infinity()
    };
    for (const auto& trigger : triggers.ordered) {
        if (MatchesWorldTrigger(a_obj, trigger, context) &&
            CanActivateTrigger(context.origin_id, source_id, instance, trigger.form->GetFormID(),
                               trigger.kind, trigger.activation_delay)) {
            return {trigger.form->GetFormID(), trigger.kind};
        }
    }
    return {};
}

bool IsInventoryTriggerAllowed(const AddOnSettings& settings, const FormID trigger_id, const TriggerKind kind,
                               const StageNo stage, const FormID container) {
    const auto allowed = [&](const auto& stages, const auto& containers) {
        const auto it = containers.find(trigger_id);
        return stages.at(trigger_id).contains(stage) &&
               (it == containers.end() || it->second.empty() || it->second.contains(container));
    };
    switch (kind) {
        case TriggerKind::kTransformer:
            return settings.transformers.contains(trigger_id) &&
                   allowed(settings.transformer_allowed_stages, settings.transformer_containers);
        case TriggerKind::kDelayer:
            return settings.delayers.contains(trigger_id) &&
                   allowed(settings.delayer_allowed_stages, settings.delayer_containers);
    }
    return false;
}

bool IsActiveTrigger(const StageInstance& instance, const FormID trigger_id, const TriggerKind kind) {
    if (trigger_id != instance.GetDelayerFormID()) {
        return false;
    }
    switch (kind) {
        case TriggerKind::kTransformer:
            return instance.xtra.is_transforming;
        case TriggerKind::kDelayer:
            return !instance.xtra.is_transforming;
    }
    return false;
}

bool CanActivateTrigger(const RefID refid, const FormID source_id, const StageInstance& instance,
                        const FormID trigger_id, const TriggerKind kind, const float activation_delay) {
    return IsActiveTrigger(instance, trigger_id, kind) ||
           ActivationQueue::GetSingleton()->CanActivate(
               refid, {source_id, instance.xtra.form_id, trigger_id, kind}, activation_delay);
}

bool MatchesInventoryTrigger(const AddOnSettings& settings, const FormID trigger_id, const TriggerKind kind,
                             const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                             const StageInstance& instance, const UpdatePhase phase) {
    if (inventory.items.empty() ||
        !IsInventoryTriggerAllowed(settings, trigger_id, kind, instance.no, queue_info.ref_info.base_id)) {
        return false;
    }
    const auto trigger = RE::TESForm::LookupByID(trigger_id);
    if (!trigger) {
        return false;
    }

    const auto type = GetMatchType_(trigger->GetFormType());
    if (std::ranges::contains(poll_matchtypes, type)) {
        if (phase == UpdatePhase::kCatchUp) {
            return IsActiveTrigger(instance, trigger_id, kind);
        }
        const auto owner = queue_info.ref_info.GetRef();
        if (!owner) {
            return false;
        }
        if (!queue_info.inventory_watch) {
            queue_info.inventory_watch = std::make_shared<InventoryWatch>();
            queue_info.update_type = QueueInfo::UpdateType::kInventory;
        }
        return queue_info.inventory_watch->Eval(trigger, type, owner, inventory);
    }
    if (const auto keyword = trigger->As<RE::BGSKeyword>()) {
        return inventory.HasKeyword(keyword);
    }
    if (const auto obj = trigger->As<RE::TESBoundObject>()) {
        const auto it = inventory.items.find(obj);
        return it != inventory.items.end() && it->second.first > 0;
    }
    return false;
}

std::pair<FormID, TriggerKind> GetTriggerInInventory(const AddOnSettings& settings, const FormID source_id,
                            const Utils::Inventory::Snapshot& inventory, QueueInfo& queue_info,
                            const StageInstance& instance, const UpdatePhase phase) {
    const auto find_trigger = [&](const auto& triggers, const TriggerKind kind) -> FormID {
        for (const auto& [trigger_id, definition] : triggers) {
            if (MatchesInventoryTrigger(settings, trigger_id, kind, inventory, queue_info, instance, phase) &&
                CanActivateTrigger(queue_info.ref_info.ref_id, source_id, instance,
                                   trigger_id, kind, definition.activation_delay)) {
                return trigger_id;
            }
        }
        return 0;
    };
    if (const auto trigger = find_trigger(settings.transformers, TriggerKind::kTransformer)) {
        return {trigger, TriggerKind::kTransformer};
    }
    return {find_trigger(settings.delayers, TriggerKind::kDelayer), TriggerKind::kDelayer};
}
