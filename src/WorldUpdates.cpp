#include "WorldUpdates.h"
#include "Utils.h"


RefStop& RefStop::operator=(const RefStop& other) {
    if (this != &other) {
        QueueInfo::operator=(other);
        features = other.features;
        // Manually handle any special cases for members
    }
    return *this;
}

bool RefStop::IsDue(const float curr_time) const { return stop_time <= curr_time; }


void RefStop::ApplyTint(const RE::TESObjectREFR* a_obj) {
    auto& tint_color = features.tint_color;
    if (!tint_color.id) {
        return RemoveTint();
    }
    if (tint_color.enabled) return;
    if (const auto a_3D = a_obj->Get3D()) {
        RE::NiColorA color;
        Utils::hexToRGBA(tint_color.id, color);
        a_3D->TintScenegraph(color);
        tint_color.enabled = true;
    }
}

void RefStop::ApplyArtObject(RE::TESObjectREFR* a_ref, const float duration) {
    auto& art_object = features.art_object;
    if (!art_object.id) return RemoveTempEffects(true, false);
    if (art_object.enabled) return;
    const auto a_art_obj = RE::TESForm::LookupByID<RE::BGSArtObject>(art_object.id);
    if (!a_art_obj) {
        logger::error("Art object not found.");
        return;
    }

    auto h = a_ref->GetHandle();
    SKSE::GetTaskInterface()->AddTask([h, a_art_obj, duration]() {
        const auto ref = h.get().get();
        if (!ref || !a_art_obj) return;
        ref->ApplyArtObject(a_art_obj, duration);
    });

    applied_art_objects.insert(art_object.id);

    art_object.enabled = true;
}

void RefStop::ApplyShader(RE::TESObjectREFR* a_ref, const float duration) {
    auto& effect_shader = features.effect_shader;
    if (!effect_shader.id) return RemoveTempEffects(false, true);
    if (effect_shader.enabled) return;
    const auto eff_shader = RE::TESForm::LookupByID<RE::TESEffectShader>(effect_shader.id);
    if (!eff_shader) {
        logger::error("Shader not found.");
        return;
    }

    auto h = a_ref->GetHandle();
    SKSE::GetTaskInterface()->AddTask([h, eff_shader, duration]() {
        const auto ref = h.get().get();
        if (!ref || !eff_shader) return;
        ref->ApplyEffectShader(eff_shader, duration);
    });
    // shader_ref_eff = a_shader_ref_eff_ptr;

    applied_effect_shaders.insert(effect_shader.id);

    effect_shader.enabled = true;
}

void RefStop::ApplySound(const float volume) {
    auto& sound = features.sound;
    if (!sound.id) {
        return RemoveSound();
    }
    if (features.sound.enabled) return;
    const auto soundhelper = SoundHelper::GetSingleton();
    sound.enabled = soundhelper->Play(ref_info.GetRef(), sound.id, volume);
}

RE::BSSoundHandle& RefStop::GetSoundHandle() const {
    auto* soundhelper = SoundHelper::GetSingleton();
    return soundhelper->GetHandle(ref_info.ref_id);
}

void RefStop::RemoveTint() {
    if (const auto a_refr = GetRef()) {
        if (const auto a_obj3d = a_refr->Get3D()) {
            const auto color = RE::NiColorA(0.0f, 0.0f, 0.0f, 0.0f);
            a_obj3d->TintScenegraph(color);
            features.tint_color.enabled = false;
        }
    }
}

void RefStop::RemoveTempEffects(bool remove_art, bool remove_shader) {
    remove_art = remove_art && !applied_art_objects.empty();
    remove_shader = remove_shader && !applied_effect_shaders.empty();
    if (!remove_art && !remove_shader) return;

    constexpr float removal_delay = 5.f;

    if (const auto a_ref = GetRef()) {
        if (const auto processLists = RE::ProcessLists::GetSingleton()) {
            const auto handle = a_ref->CreateRefHandle();

            processLists->ForEachMagicTempEffect([&](RE::BSTempEffect* a_effect) {
                RE::ReferenceEffect* matched = nullptr;

                if (const auto model = a_effect->As<RE::ModelReferenceEffect>()) {
                    if (remove_art && model->artObject && applied_art_objects.contains(model->artObject->GetFormID())) {
                        matched = model;
                    }
                } else if (const auto shader = a_effect->As<RE::ShaderReferenceEffect>()) {
                    if (remove_shader && shader->effectData &&
                        applied_effect_shaders.contains(shader->effectData->GetFormID())) {
                        matched = shader;
                    }
                }

                if (matched && matched->target == handle && matched->lifetime < 0.f) {
                    matched->lifetime = matched->age + removal_delay;
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
    }

    if (remove_art) {
        applied_art_objects.clear();
        features.art_object.enabled = false;
    }
    if (remove_shader) {
        applied_effect_shaders.clear();
        features.effect_shader.enabled = false;
    }
}

void RefStop::RemoveSound() {
    const auto soundhelper = SoundHelper::GetSingleton();
    soundhelper->Stop(ref_info.ref_id);
    features.sound.enabled = false;
}

void RefStop::Update(const RefStop& other) {
    if (ref_info.ref_id != other.ref_info.ref_id) {
        logger::critical("RefID not the same.");
        return;
    }

    ref_info = other.ref_info;
    update_type = other.update_type;
    inventory_watch = other.inventory_watch;

    if (features.tint_color.id != other.features.tint_color.id) {
        RemoveTint();
        features.tint_color.id = other.features.tint_color.id;
    }
    const bool remove_art = features.art_object.id != other.features.art_object.id;
    const bool remove_shader = features.effect_shader.id != other.features.effect_shader.id;
    RemoveTempEffects(remove_art, remove_shader);
    features.art_object.id = other.features.art_object.id;
    features.effect_shader.id = other.features.effect_shader.id;
    if (features.sound.id != other.features.sound.id) {
        RemoveSound();
        features.sound.id = other.features.sound.id;
    }
    if (fabs(stop_time - other.stop_time) > EPSILON) {
        stop_time = other.stop_time;
    }
}