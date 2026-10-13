#pragma once
#include "CustomObjects.h"
#include "InventoryWatch.h"


struct RefStop : QueueInfo {
    ~RefStop() = default;

    bool operator<(const RefStop& other) const { return ref_info.ref_id < other.ref_info.ref_id; }

    RefStop& operator=(const RefStop& other);

    RefStopFeatures features;

    // RE::ShaderReferenceEffect* shader_ref_eff;
    // RE::ModelReferenceEffect* model_ref_eff;

    std::unordered_set<FormID> applied_art_objects;
    std::unordered_set<FormID> applied_effect_shaders;

    explicit RefStop(const QueueInfo& queue_info) : QueueInfo(queue_info) {}

    explicit RefStop(const RefInfo& a_info, const UpdateType a_update_type)
        : RefStop(QueueInfo{.ref_info = a_info, .update_type = a_update_type}) {}

    explicit RefStop(const RefID a_ref_id, const UpdateType a_update_type)
        : RefStop(RefInfo(a_ref_id), a_update_type) {}

    [[nodiscard]] bool IsDue(float curr_time) const;

    void ApplyTint(const RE::TESObjectREFR* a_obj);
    void ApplyArtObject(RE::TESObjectREFR* a_ref, float duration = -1.f);
    void ApplyShader(RE::TESObjectREFR* a_ref, float duration = -1.f);
    void ApplySound(float volume = 200.f);
    [[nodiscard]] RE::BSSoundHandle& GetSoundHandle() const;

    void RemoveTint();
    void RemoveTempEffects(bool remove_art, bool remove_shader);
    void RemoveSound();

    void Update(const RefStop& other);

    RE::TESObjectREFR* GetRef() const { return ref_info.GetRef(); }
};