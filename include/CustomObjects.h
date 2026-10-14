#pragma once
#include <REX/REX/Singleton.h>
#include <shared_mutex>
#include <unordered_set>

class SoundHelper : public REX::Singleton<SoundHelper> {
    std::unordered_map<RefID, RE::BSSoundHandle> handles;

    std::shared_mutex mutex;

public:
    RE::BSSoundHandle& GetHandle(const RefID refid) {
        if (std::shared_lock lock(mutex); handles.contains(refid)) return handles.at(refid);
        std::unique_lock lock(mutex);
        auto [it, inserted] = handles.try_emplace(refid, RE::BSSoundHandle{});
        return it->second;
    }

    void DeleteHandle(RefID refid);


    void Stop(RefID refid);
    bool Play(const RE::TESObjectREFR* ref, FormID sound_id, float volume);
};

struct RefStopFeature {
    uint32_t id = 0;
    bool enabled = false;

    explicit operator bool() const;

    RefStopFeature();

    explicit RefStopFeature(const uint32_t i) : id(i) {
    }

    RefStopFeature& operator=(const RefStopFeature& other);
};

struct RefStopFeatures {
    RefStopFeature tint_color;
    RefStopFeature art_object;
    RefStopFeature effect_shader;
    RefStopFeature sound;
    RefStopFeatures() = default;

    RefStopFeatures(const uint32_t a_tint_id, const uint32_t a_art_id,
                    const uint32_t a_effshd_id,
                    const uint32_t a_sound_id)
        : tint_color(a_tint_id), art_object(a_art_id), effect_shader(a_effshd_id), sound(a_sound_id) {
    }
};

struct RefInfo {
    RefID ref_id = 0;
    FormID base_id = 0;
    mutable RE::ObjectRefHandle ref_handle{};

    explicit RefInfo(const RefID id) : ref_id(id) {
        if (const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(ref_id)) {
            ref_handle = ref->GetHandle();
            if (const auto base = ref->GetBaseObject()) {
                base_id = base->GetFormID();
            }
        }
    }

    RefInfo(const RefID a_ref_id, const FormID a_base_id)
        : ref_id(a_ref_id), base_id(a_base_id) {
    }

    RE::TESObjectREFR* GetRef() const {
        if (const auto ref = ref_handle.get().get()) {
            if (ref->GetFormID() == ref_id) {
                return ref;
            }
        }
        if (const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(ref_id)) {
            ref_handle.reset();
            ref_handle = ref->GetHandle();
            return ref;
        }
        return nullptr;
    }
};