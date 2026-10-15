#include "Triggers.h"
#include "CellScan.h"
#include "CLibUtilsQTR/BoundingBox.hpp"
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
                                   const float proximity = Settings::proximity_range) {
        const auto cell = a_origin->GetParentCell();
        const auto targetCell = ref->GetParentCell();
        if (!cell || !targetCell ||
            (cell != targetCell && (cell->IsInteriorCell() || targetCell->IsInteriorCell() ||
                                    !a_origin->GetWorldspace() || a_origin->GetWorldspace() != ref->GetWorldspace())))
            return false;

        DirectX::BoundingOrientedBox obb1{};
        DirectX::BoundingOrientedBox obb2{};

        if (const auto a_rigidbody1 = BoundingBox::GetRigidBody(a_origin)) {
            BoundingBox::GetOBB(a_rigidbody1, obb1);
        } else {
            BoundingBox::GetOBB(a_origin, obb1);
        }
        if (const auto a_rigidbody2 = BoundingBox::GetRigidBody(ref)) {
            BoundingBox::GetOBB(a_rigidbody2, obb2);
        } else {
            BoundingBox::GetOBB(ref, obb2);
        }

        if (!AreClose(obb1, obb2, proximity)) {
            return false;
        }

#ifndef NDEBUG
        if (UI::draw_debug) {
            DebugAPI_IMPL::DrawDebug::DrawOBB(obb2);
            DebugAPI_IMPL::DrawDebug::DrawOBB(obb1);

            DebugAPI_IMPL::DrawDebug::draw_line(Utils::WorldObject::GetPosition(ref),
                                                Utils::WorldObject::GetPosition(RE::PlayerCharacter::GetSingleton()),
                                                3.f, RE::NiColorA(0.f, 0.f, 1.f, 1.f));

            const RE::NiPoint3 c1{obb1.Center.x, obb1.Center.y, obb1.Center.z};
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


FormID FindWorldTrigger(RE::TESObjectREFR* a_obj, const Triggers& triggers) {
    if (!a_obj || triggers.ordered.empty()) return 0;

    Scanner::CachePtr cache;
    std::optional<RE::BGSLocation*> current_location;
    const auto originID = a_obj->GetFormID();
    std::optional<RE::NiPoint3> originPos;

    const float r = Settings::search_radius;
    const float r2 = (r > 0.0f) ? (r * r) : std::numeric_limits<float>::infinity();

    for (const auto& [form, scope] : triggers.ordered) {
        const auto triggerID = form->GetFormID();
        if (!scope.any(Scanner::Scope::kProximity)) {
            if (scope.any(Scanner::Scope::kSelf)) {
                if (const auto location = form->As<RE::BGSLocation>()) {
                    if (!current_location) current_location = LazyCache::GetSingleton()->GetLocation(a_obj);
                    if (Utils::IsInLocation(location, *current_location)) return triggerID;
                } else if (Matches(a_obj, form)) return triggerID;
            }
            continue;
        }

        if (!cache) cache = Scanner::GetSingleton()->GetCache();
        const auto it = cache->byTrigger.find(triggerID);
        if (it == cache->byTrigger.end()) {
            continue;
        }
        if (scope.any(Scanner::Scope::kSelf) && it->second.contains(originID)) return triggerID;

        for (const auto& [refid, pos] : it->second) {
            if (!originPos) originPos = Utils::WorldObject::GetPosition(a_obj);
            if (originPos->GetSquaredDistance(pos) > r2) {
                continue;
            }

            const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid);
            if (!ref || !Utils::WorldObject::IsActive(ref)) {
                continue;
            }

            if (SearchModulatorInCell_Sub(a_obj, ref)) {
                return triggerID;
            }
        }
    }

    return 0;
}