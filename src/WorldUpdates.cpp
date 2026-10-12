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
    if (!art_object.id) return RemoveArtObject();
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
    if (!effect_shader.id) return RemoveShader();
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

void RefStop::RemoveArtObject() {
    // if (model_ref_eff) model_ref_eff->finished = true;

    if (applied_art_objects.empty()) return;
    if (const auto a_ref = GetRef()) {
        if (const auto processLists = RE::ProcessLists::GetSingleton()) {
            const auto handle = a_ref->CreateRefHandle();
            processLists->ForEachModelEffect([&](RE::ModelReferenceEffect* a_modelEffect) {
                if (a_modelEffect->target == handle && a_modelEffect->artObject) {
                    if (applied_art_objects.contains(a_modelEffect->artObject->GetFormID())) {
                        if (a_modelEffect->lifetime < 0.f) {
                            a_modelEffect->lifetime = a_modelEffect->age + 5.f;
                        }
                    }
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
    }
    applied_art_objects.clear();

    features.art_object.enabled = false;
}

void RefStop::RemoveShader() {
    // if (shader_ref_eff) shader_ref_eff->finished = true;
    // if (const auto a_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(ref_id)) {
    //	if (const auto processLists = RE::ProcessLists::GetSingleton()) {
    //		processLists->StopAllMagicEffects(*a_ref);
    //	}

    //}
    if (applied_effect_shaders.empty()) return;
    if (const auto a_ref = GetRef()) {
        if (const auto processLists = RE::ProcessLists::GetSingleton()) {
            const auto handle = a_ref->CreateRefHandle();
            processLists->ForEachShaderEffect([&](RE::ShaderReferenceEffect* a_modelEffect) {
                if (a_modelEffect->target == handle && a_modelEffect->effectData) {
                    if (applied_effect_shaders.contains(a_modelEffect->effectData->GetFormID())) {
                        if (a_modelEffect->lifetime < 0.f) {
                            a_modelEffect->lifetime = a_modelEffect->age + 5.f;
                        }
                    }
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
    }
    applied_effect_shaders.clear();
    features.effect_shader.enabled = false;
}

void RefStop::RemoveSound() {
    const auto soundhelper = SoundHelper::GetSingleton();
    soundhelper->Stop(ref_info.ref_id);
    features.sound.enabled = false;
}

bool RefStop::HasArtObject(RE::TESObjectREFR* a_ref, const RE::BGSArtObject* a_art) {
    uint32_t count = 0;
    if (const auto processLists = RE::ProcessLists::GetSingleton(); processLists) {
        const auto handle = a_ref->CreateRefHandle();
        processLists->ForEachModelEffect([&](const RE::ModelReferenceEffect* a_modelEffect) {
            if (a_modelEffect->target == handle && a_modelEffect->artObject == a_art) {
                if (!a_modelEffect->finished) {
                    count++;
                }
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }
    return count > 0;
}

void RefStop::Update(const RefStop& other) {
    if (ref_info.ref_id != other.ref_info.ref_id) {
        logger::critical("RefID not the same.");
        return;
    }

    ref_info = other.ref_info;
    update_flags = other.update_flags;
    inventory_watch = other.inventory_watch;

    if (features.tint_color.id != other.features.tint_color.id) {
        RemoveTint();
        features.tint_color.id = other.features.tint_color.id;
    }
    if (features.art_object.id != other.features.art_object.id) {
        RemoveArtObject();
        features.art_object.id = other.features.art_object.id;
    }
    if (features.effect_shader.id != other.features.effect_shader.id) {
        RemoveShader();
        features.effect_shader.id = other.features.effect_shader.id;
    }
    if (features.sound.id != other.features.sound.id) {
        RemoveSound();
        features.sound.id = other.features.sound.id;
    }
    if (fabs(stop_time - other.stop_time) > EPSILON) {
        stop_time = other.stop_time;
    }
}