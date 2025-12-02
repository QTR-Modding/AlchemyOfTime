#pragma once

namespace BoundingBox {
    std::array<RE::NiPoint3, 8> Get(const RE::TESObjectREFR* a_obj);
    void Draw(const RE::TESObjectREFR* a_obj);
    void Draw(const std::array<RE::NiPoint3, 8>& box);
    RE::NiPoint3 ClosestPoint(const RE::NiPoint3& from, const RE::TESObjectREFR* to);
}
