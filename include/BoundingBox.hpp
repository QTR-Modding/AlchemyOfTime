#pragma once
#include "Utils.h"
#include "DrawDebug.hpp"

using DebugAPI_IMPL::DrawDebug::draw_line;

namespace BoundingBox {
    inline RE::bhkRigidBody* GetRigidBody(const RE::TESObjectREFR* refr) {
        const auto object3D = refr->GetCurrent3D();
        if (!object3D) return nullptr;
        if (const auto body = object3D->GetCollisionObject()) {
            return body->GetRigidBody();
        }
        return nullptr;
    }

    inline std::array<RE::NiPoint3, 8> Get(const RE::TESObjectREFR* a_obj) {
        // Prefer Havok worldspace AABB if available
        if (const auto body = GetRigidBody(a_obj)) {
            RE::hkAabb aabb;
            body->GetAabbWorldspace(aabb);
            float minComp[4]{};
            float maxComp[4]{};
            _mm_store_ps(minComp, aabb.min.quad);
            _mm_store_ps(maxComp, aabb.max.quad);
            constexpr float havokToSkyrim = 69.9915f;
            RE::NiPoint3 minWorld{minComp[0] * havokToSkyrim, minComp[1] * havokToSkyrim, minComp[2] * havokToSkyrim};
            RE::NiPoint3 maxWorld{maxComp[0] * havokToSkyrim, maxComp[1] * havokToSkyrim, maxComp[2] * havokToSkyrim};

            auto v1 = RE::NiPoint3{minWorld.x, minWorld.y, minWorld.z};
            auto v2 = RE::NiPoint3{maxWorld.x, minWorld.y, minWorld.z};
            auto v3 = RE::NiPoint3{maxWorld.x, maxWorld.y, minWorld.z};
            auto v4 = RE::NiPoint3{minWorld.x, maxWorld.y, minWorld.z};
            auto v5 = RE::NiPoint3{minWorld.x, minWorld.y, maxWorld.z};
            auto v6 = RE::NiPoint3{maxWorld.x, minWorld.y, maxWorld.z};
            auto v7 = RE::NiPoint3{maxWorld.x, maxWorld.y, maxWorld.z};
            auto v8 = RE::NiPoint3{minWorld.x, maxWorld.y, maxWorld.z};
            return {v1, v2, v3, v4, v5, v6, v7, v8};
        }
        // Fallback: use ref local bounds (intended gameplay bounds), then rotate/translate
        RE::NiPoint3 minLocal = a_obj->GetBoundMin();
        RE::NiPoint3 maxLocal = a_obj->GetBoundMax();
        const float scale = a_obj->GetScale();
        minLocal *= scale;
        maxLocal *= scale;

        const auto node = a_obj->GetCurrent3D();
        RE::NiMatrix3 R;
        RE::NiPoint3 T;
        if (node) {
            R = node->world.rotate;
            T = node->world.translate;
        } else {
            R.SetEulerAnglesXYZ(a_obj->GetAngle());
            T = a_obj->GetPosition();
        }

        auto makeCorner = [&](const float x, const float y, const float z) -> RE::NiPoint3 {
            const RE::NiPoint3 local{x, y, z};
            return T + R * local;
        };

        auto v1 = makeCorner(minLocal.x, minLocal.y, minLocal.z);
        auto v2 = makeCorner(maxLocal.x, minLocal.y, minLocal.z);
        auto v3 = makeCorner(maxLocal.x, maxLocal.y, minLocal.z);
        auto v4 = makeCorner(minLocal.x, maxLocal.y, minLocal.z);
        auto v5 = makeCorner(minLocal.x, minLocal.y, maxLocal.z);
        auto v6 = makeCorner(maxLocal.x, minLocal.y, maxLocal.z);
        auto v7 = makeCorner(maxLocal.x, maxLocal.y, maxLocal.z);
        auto v8 = makeCorner(minLocal.x, maxLocal.y, maxLocal.z);

        return {v1, v2, v3, v4, v5, v6, v7, v8};
    }

    inline void Draw(const std::array<RE::NiPoint3, 8>& a_box) {
        const auto& v1 = a_box[0];
        const auto& v2 = a_box[1];
        const auto& v3 = a_box[2];
        const auto& v4 = a_box[3];
        const auto& v5 = a_box[4];
        const auto& v6 = a_box[5];
        const auto& v7 = a_box[6];
        const auto& v8 = a_box[7];

        // bottom
        draw_line(v1, v2, 1);
        draw_line(v2, v3, 1);
        draw_line(v3, v4, 1);
        draw_line(v4, v1, 1);
        // top
        draw_line(v5, v6, 1);
        draw_line(v6, v7, 1);
        draw_line(v7, v8, 1);
        draw_line(v8, v5, 1);
        // sides
        draw_line(v1, v5, 1);
        draw_line(v2, v6, 1);
        draw_line(v3, v7, 1);
        draw_line(v4, v8, 1);
    }

    inline void Draw(const RE::TESObjectREFR* a_obj) {
        const auto box = Get(a_obj);
        Draw(box);
    }

    inline RE::NiPoint3 ClosestPoint(const RE::NiPoint3& a_point_from, const RE::TESObjectREFR* a_obj_to) {
        using RE::NiPoint3;
        // Use ref local bounds for consistency with Get()
        NiPoint3 minLocal = a_obj_to->GetBoundMin();
        NiPoint3 maxLocal = a_obj_to->GetBoundMax();
        const float scale = a_obj_to->GetScale();
        minLocal *= scale;
        maxLocal *= scale;

        const auto node = a_obj_to->GetCurrent3D();
        RE::NiMatrix3 R;
        NiPoint3 T;
        if (node) {
            R = node->world.rotate;
            T = node->world.translate;
        } else {
            R.SetEulerAnglesXYZ(a_obj_to->GetAngle());
            T = WorldObject::GetPosition(a_obj_to);
        }

        const NiPoint3 localCenter = (minLocal + maxLocal) * 0.5f;
        const NiPoint3 half = (maxLocal - minLocal) * 0.5f;
        const NiPoint3 worldCenter = T + R * localCenter;

        // Transform point to OBB local space
        const RE::NiMatrix3 RT = R.Transpose();
        const NiPoint3 d = a_point_from - worldCenter;
        const NiPoint3 localP = RT * d;

        // Clamp
        const NiPoint3 clamped{
            std::clamp(localP.x, -half.x, half.x),
            std::clamp(localP.y, -half.y, half.y),
            std::clamp(localP.z, -half.z, half.z)
        };

        return worldCenter + R * clamped;
    }
}