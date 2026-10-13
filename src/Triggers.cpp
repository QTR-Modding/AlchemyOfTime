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

FormID FindWorldTrigger(RE::TESObjectREFR* a_obj, const Triggers& triggers) {
    if (!a_obj || triggers.ordered.empty()) return 0;

    const auto cache = triggers.scan_forms.empty() ? nullptr : CellScanner::GetSingleton()->GetCache();
    const auto originPos = Utils::WorldObject::GetPosition(a_obj);

    const float r = Settings::search_radius;
    const float r2 = (r > 0.0f) ? (r * r) : std::numeric_limits<float>::infinity();

    const auto current_location = a_obj->GetCurrentLocation();
    for (const auto form : triggers.ordered) {
        // doesnt need scan
        switch (form->GetFormType()) {
            case RE::FormType::Location: {
                const auto location = static_cast<RE::BGSLocation*>(form);
                if (Utils::IsInLocation(location, current_location)) {
                    return form->GetFormID();
                }
                continue;
            }
            case RE::FormType::Perk: {
                const auto perk = static_cast<RE::BGSPerk*>(form);
                if (perk->perkConditions.IsTrue(a_obj, a_obj)) {
                    return form->GetFormID();
                }
                continue;
            }
            default:
                break;
        }
        if (!cache) continue;
        const bool appliedEffect =
            std::ranges::contains(CellScanner::applied_effect_trigger_types, form->GetFormType());
        const auto triggerID = form->GetFormID();
        const auto it = cache->byTrigger.find(triggerID);
        if (it == cache->byTrigger.end()) {
            continue;
        }

        for (const auto& e : it->second) {
            if (appliedEffect && e.refid == a_obj->GetFormID()) return triggerID;
            if (originPos.GetSquaredDistance(e.pos) > r2) {
                continue;
            }

            const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(e.refid);
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