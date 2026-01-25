#include "CustomObjects.h"
#include "Settings.h"
#include "CLibUtilsQTR/FormReader.hpp"

bool StageInstance::operator==(const StageInstance& other) const {
    return no == other.no && count == other.count &&
           //location == other.location &&
           fabs(start_time - other.start_time) < EPSILON &&
           fabs(_elapsed - other._elapsed) < EPSILON && xtra == other.xtra;
}

bool StageInstance::AlmostSameExceptCount(const StageInstance& other, const float curr_time) const {
    // bcs they are in the same inventory they will have same delay magnitude
    // delay starts might be different but if the elapsed times are close enough, we don't care
    return no == other.no &&
           //location == other.location &&
           std::abs(start_time - other.start_time) < 0.015 &&
           std::abs(GetElapsed(curr_time) - other.GetElapsed(curr_time)) < 0.015 && xtra == other.xtra;
}

StageInstance& StageInstance::operator=(const StageInstance& other) {
    if (this != &other) {
        start_time = other.start_time;
        no = other.no;
        count = other.count;
        //location = other.location;

        xtra = other.xtra;

        _elapsed = other._elapsed;
        _delay_start = other._delay_start;
        _delay_mag = other._delay_mag;
        _delay_formid = other._delay_formid;
    }
    return *this;
}

RE::TESBoundObject* StageInstance::GetBound() const {
    return FormReader::GetFormByID<RE::TESBoundObject>(xtra.form_id);
}

float StageInstance::GetElapsed(const float curr_time) const {
    if (fabs(_delay_mag) < EPSILON) return _elapsed;
    return (curr_time - _delay_start) * GetDelaySlope() + _elapsed;
}

float StageInstance::GetDelaySlope() const {
    return std::min(std::max(-Settings::max_modulator_strength, _delay_mag), Settings::max_modulator_strength);
}

void StageInstance::SetNewStart(const float start_t) {
    start_time = start_t;
    _delay_start = start_time;
    _elapsed = 0;
}

void StageInstance::SetNewStart(const float curr_time, const float overshot) {
    // overshot: by how much is the schwelle already ueberschritten
    start_time = curr_time - overshot / (GetDelaySlope() + std::numeric_limits<float>::epsilon());
    _delay_start = start_time;
    _elapsed = 0;
}

void StageInstance::SetDelay(const float time, const float delay, const FormID formid) {
    // yeni steigungla yeni ausgangspunkt yapiyoruz
    // call only from UpdateTimeModulationInInventory
    if (xtra.is_transforming) return;
    if (fabs(_delay_mag - delay) < EPSILON && _delay_formid == formid) return;

    _elapsed = GetElapsed(time);
    _delay_start = time;
    _delay_mag = delay;
    _delay_formid = formid;
}

void StageInstance::SetTransform(const float time, const FormID formid) {
    if (xtra.is_transforming) {
        if (_delay_formid != formid) {
            RemoveTransform(time);
            return SetTransform(time, formid);
        }
        return;
    }
    SetDelay(time, 1, formid);
    xtra.is_transforming = true;
}

void StageInstance::RemoveTransform(const float curr_time) {
    if (!xtra.is_transforming) return;
    xtra.is_transforming = false;
    _delay_start = curr_time;
    _delay_mag = 1;
    _delay_formid = 0;
}

void StageInstance::RemoveTimeMod(const float time) {
    RemoveTransform(time);
    SetDelay(time, 1, 0);
}

StageInstancePlain StageInstance::GetPlain() const {
    StageInstancePlain plain;
    plain.start_time = start_time;
    plain.no = no;
    plain.count = count;

    plain.is_fake = xtra.is_fake;
    plain.is_decayed = xtra.is_decayed;
    plain.is_transforming = xtra.is_transforming;

    plain._elapsed = _elapsed;
    plain._delay_start = _delay_start;
    plain._delay_mag = _delay_mag;
    plain._delay_formid = _delay_formid;

    if (xtra.is_fake) plain.form_id = xtra.form_id;

    return plain;
}

void StageInstance::SetDelay(const StageInstancePlain& plain) {
    _elapsed = plain._elapsed;
    _delay_start = plain._delay_start;
    _delay_mag = plain._delay_mag;
    _delay_formid = plain._delay_formid;
}

RE::TESBoundObject* Stage::GetBound() const { return FormReader::GetFormByID<RE::TESBoundObject>(formid); }

bool Stage::CheckIntegrity() const {
    if (!formid || !GetBound()) {
        logger::error("FormID or bound is null");
        return false;
    }
    if (duration <= 0) {
        logger::error("Duration is 0 or negative");
        return false;
    }
    return true;
}


bool DefaultSettings::CheckIntegrity() {
    if (items.empty() || durations.empty() || stage_names.empty() || effects.empty() || numbers.empty()) {
        logger::error("One of the maps is empty.");
        init_failed = true;
        return false;
    }
    if (items.size() != durations.size() || items.size() != stage_names.size() || items.size() != numbers.size()) {
        logger::error("Sizes do not match.");
        init_failed = true;
        return false;
    }
    for (auto i = 0; i < numbers.size(); i++) {
        if (!std::ranges::contains(numbers, i)) {
            logger::error("Key {} not found in numbers.", i);
            return false;
        }
        if (!items.contains(i) || !crafting_allowed.contains(i) || !durations.contains(i) || !stage_names.contains(i) ||
            !effects.contains(i)) {
            logger::error("Key {} not found in all maps.", i);
            init_failed = true;
            return false;
        }

        if (durations[i] <= 0) {
            logger::error("Duration is less than or equal 0.");
            init_failed = true;
            return false;
        }

        if (!costoverrides.contains(i)) costoverrides[i] = -1;
        if (!weightoverrides.contains(i)) weightoverrides[i] = -1.0f;
    }
    if (!decayed_id) {
        logger::error("Decayed id is 0.");
        init_failed = true;
        return false;
    }
    for (const auto& [a_formID, _transformer] : transformers) {
        const FormID _finalFormEditorID = _transformer.first;
        const Duration _duration = _transformer.second;
        const auto& _allowedStages = transformer_allowed_stages.at(a_formID);
        if (!FormReader::GetFormByID(a_formID) || !FormReader::GetFormByID(_finalFormEditorID)) {
            logger::error("Formid not found.");
            init_failed = true;
            return false;
        }
        if (!transformers_order.contains(a_formID)) {
            logger::error("Transformer formid {:x} not found in transformers_order.", a_formID);
            init_failed = true;
            return false;
        }
        if (_duration <= 0) {
            logger::error("Duration is less than or equal 0.");
            init_failed = true;
            return false;
        }
        if (_allowedStages.empty()) {
            logger::error("Allowed stages is empty.");
            init_failed = true;
            return false;
        }
        for (const auto& _stage : _allowedStages) {
            if (!std::ranges::contains(numbers, _stage)) {
                logger::error("Stage {} not found in numbers.", _stage);
                init_failed = true;
                return false;
            }
        }
    }

    for (const auto& a_formID : delayers | std::views::keys) {
        const auto& _allowedStages = delayer_allowed_stages.at(a_formID);
        if (!FormReader::GetFormByID(a_formID)) {
            logger::error("Delayer formid {:x} not found.", a_formID);
            init_failed = true;
            return false;
        }
        if (!delayers_order.contains(a_formID)) {
            logger::error("Delayer formid {:x} not found in delayers_order.", a_formID);
            init_failed = true;
            return false;
        }
        if (_allowedStages.empty()) {
            logger::error("Allowed stages is empty for delayer formid {:x}.", a_formID);
            init_failed = true;
            return false;
        }
        for (const auto& _stage : _allowedStages) {
            if (!std::ranges::contains(numbers, _stage)) {
                logger::error("Stage {} not found in numbers for delayer formid {:x}.", _stage, a_formID);
                init_failed = true;
                return false;
            }
        }
    }

    for (const auto& _formID : containers) {
        if (!FormReader::GetFormByID(_formID)) {
            logger::error("Container formid {:x} not found.", _formID);
            init_failed = true;
            return false;
        }
    }
    return true;
}

bool DefaultSettings::IsEmpty() {
    if (numbers.empty()) {
        init_failed = true;
        return true;
    }
    return false;
}


// check it
void DefaultSettings::Add(AddOnSettings& addon) {
    // containers
    AddHelper(containers, addon.containers);

    // delayers
    for (const auto& [a_formID, _delay] : addon.delayers) {
        if (!a_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        if (!delayers.contains(a_formID)) {
            delayers_order.insert(a_formID);
        }
        delayers[a_formID] = _delay;

        if (addon.delayer_allowed_stages.contains(a_formID)) {
            AddHelper(delayer_allowed_stages[a_formID], addon.delayer_allowed_stages.at(a_formID));
        }
        if (delayer_allowed_stages.at(a_formID).empty()) {
            delayer_allowed_stages[a_formID] = std::unordered_set(numbers.begin(), numbers.end());
        }
    }
    // transformers
    for (auto& [a_formID, _transformer] : addon.transformers) {
        if (!a_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        if (!transformers.contains(a_formID)) {
            transformers_order.insert(a_formID);
        }
        transformers[a_formID] = _transformer;
        if (addon.transformer_allowed_stages.contains(a_formID)) {
            AddHelper(transformer_allowed_stages[a_formID], addon.transformer_allowed_stages.at(a_formID));
        }
        if (transformer_allowed_stages.at(a_formID).empty()) {
            transformer_allowed_stages.at(a_formID) = std::unordered_set(numbers.begin(), numbers.end());
        }
    }

    AddHelper(delayer_colors, addon.delayer_colors);
    AddHelper(delayer_sounds, addon.delayer_sounds);
    AddHelper(delayer_artobjects, addon.delayer_artobjects);
    AddHelper(delayer_effect_shaders, addon.delayer_effect_shaders);
    AddHelper(transformer_colors, addon.transformer_colors);
    AddHelper(transformer_sounds, addon.transformer_sounds);
    AddHelper(transformer_artobjects, addon.transformer_artobjects);
    AddHelper(transformer_effect_shaders, addon.transformer_effect_shaders);

    // delayer/transformer containers
    for (const auto& [a_formID, _containers] : addon.delayer_containers) {
        if (!a_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        for (const auto& _container : _containers) {
            if (!_container) {
                logger::critical("AddOn has null formid.");
                continue;
            }
            delayer_containers[a_formID].insert(_container);
        }
    }
    for (const auto& [_formID, _containers] : addon.transformer_containers) {
        if (!_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        for (const auto& _container : _containers) {
            if (!_container) {
                logger::critical("AddOn has null formid.");
                continue;
            }
            transformer_containers[_formID].insert(_container);
        }
    }
}

void DefaultSettings::AddHelper(std::unordered_map<FormID, FormID>& dest,
                                const std::unordered_map<FormID, FormID>& src) {
    for (const auto& [_formID, _art] : src) {
        if (!_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        dest[_formID] = _art;
    }
}

void DefaultSettings::AddHelper(std::unordered_set<FormID>& dest, const std::unordered_set<FormID>& src) {
    for (const auto& _formID : src) {
        if (!_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        dest.insert(_formID);
    }
}

RefStopFeature::operator bool() const { return id > 0 && enabled; }

RefStopFeature::RefStopFeature() {
    id = 0;
    enabled = false;
}

RefStopFeature& RefStopFeature::operator=(const RefStopFeature& other) {
    if (this != &other) {
        id = other.id;
        enabled = other.enabled;
    }
    return *this;
}

RefStop& RefStop::operator=(const RefStop& other) {
    if (this != &other) {
        ref_id = other.ref_id;
        stop_time = other.stop_time;
        tint_color = other.tint_color;
        art_object = other.art_object;
        effect_shader = other.effect_shader;
        sound = other.sound;

        // Manually handle any special cases for members
    }
    return *this;
}

RefStop::RefStop(const RefID ref_id_) {
    ref_id = ref_id_;
}

bool RefStop::IsDue(const float curr_time) const { return stop_time <= curr_time; }

// check it
bool AddOnSettings::CheckIntegrity() {
    for (const auto& _formID : containers) {
        if (!FormReader::GetFormByID(_formID)) {
            logger::error("Container form {} not found.", _formID);
            init_failed = true;
            return false;
        }
    }

    std::unordered_set<FormID> all_delayers;
    for (const auto& a_formID : delayers | std::views::keys) {
        if (!FormReader::GetFormByID(a_formID)) {
            logger::error("Delayer form {} not found.", a_formID);
            init_failed = true;
            return false;
        }
        all_delayers.insert(a_formID);
    }

    if (all_delayers != delayers_order) {
        logger::error("Delayers order does not match the keys in delayers map.");
        init_failed = true;
        return false;
    }

    std::unordered_set<FormID> all_transformers;
    for (const auto& [a_formid, _transformer] : transformers) {
        const FormID _finalFormEditorID = _transformer.first;
        const Duration _duration = _transformer.second;
        if (!FormReader::GetFormByID(a_formid) || !FormReader::GetFormByID(_finalFormEditorID)) {
            logger::error("Form not found.");
            init_failed = true;
            return false;
        }
        if (_duration <= 0) {
            logger::error("Duration is less than or equal 0.");
            init_failed = true;
            return false;
        }
        all_transformers.insert(a_formid);
    }
    if (all_transformers != transformers_order) {
        logger::error("Transformers order does not match the keys in transformers map.");
        init_failed = true;
        return false;
    }

    return true;
}

void RefStop::ApplyTint(const RE::TESObjectREFR* a_obj) {
    if (!tint_color.id) {
        return RemoveTint();
    }
    if (tint_color.enabled) return;
    if (const auto a_3D = a_obj->Get3D()) {
        RE::NiColorA color;
        hexToRGBA(tint_color.id, color);
        a_3D->TintScenegraph(color);
        tint_color.enabled = true;
    }
}

void RefStop::ApplyArtObject(RE::TESObjectREFR* a_ref, const float duration) {
    if (!art_object.id) return RemoveArtObject();
    if (art_object.enabled) return;
    const auto a_art_obj = RE::TESForm::LookupByID<RE::BGSArtObject>(art_object.id);
    if (!a_art_obj) {
        logger::error("Art object not found.");
        return;
    }

    SKSE::GetTaskInterface()->AddTask([a_ref, a_art_obj, duration]() {
        if (!a_ref || !a_art_obj) return;
        a_ref->ApplyArtObject(a_art_obj, duration);
    });

    applied_art_objects.insert(art_object.id);

    art_object.enabled = true;
}

void RefStop::ApplyShader(RE::TESObjectREFR* a_ref, const float duration) {
    if (!effect_shader.id) return RemoveShader();
    if (effect_shader.enabled) return;
    const auto eff_shader = RE::TESForm::LookupByID<RE::TESEffectShader>(effect_shader.id);
    if (!eff_shader) {
        logger::error("Shader not found.");
        return;
    }
    SKSE::GetTaskInterface()->AddTask([a_ref, eff_shader, duration]() {
        if (!a_ref || !eff_shader) return;
        a_ref->ApplyEffectShader(eff_shader, duration);
    });
    //shader_ref_eff = a_shader_ref_eff_ptr;

    effect_shader.enabled = true;
}

void RefStop::ApplySound(const float volume) {
    if (!sound.id) {
        return RemoveSound();
    }
    const auto soundhelper = SoundHelper::GetSingleton();
    soundhelper->Play(ref_id, sound.id, volume);
    sound.enabled = true;
}

RE::BSSoundHandle& RefStop::GetSoundHandle() const {
    auto* soundhelper = SoundHelper::GetSingleton();
    return soundhelper->GetHandle(ref_id);
}


void RefStop::RemoveTint() {
    if (const auto a_refr = RE::TESForm::LookupByID<RE::TESObjectREFR>(ref_id)) {
        if (const auto a_obj3d = a_refr->Get3D()) {
            const auto color = RE::NiColorA(0.0f, 0.0f, 0.0f, 0.0f);
            a_obj3d->TintScenegraph(color);
            tint_color.enabled = false;
        }
    }
}

void RefStop::RemoveArtObject() {
    //if (model_ref_eff) model_ref_eff->finished = true;

    if (applied_art_objects.empty()) return;
    if (const auto a_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(ref_id)) {
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

    art_object.enabled = false;
}

void RefStop::RemoveShader() {
    //if (shader_ref_eff) shader_ref_eff->finished = true;
    //if (const auto a_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(ref_id)) {
    //	if (const auto processLists = RE::ProcessLists::GetSingleton()) {
    //		processLists->StopAllMagicEffects(*a_ref);
    //	}

    //}
    if (applied_effect_shaders.empty()) return;
    if (const auto a_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(ref_id)) {
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
    effect_shader.enabled = false;
}

void RefStop::RemoveSound() {
    const auto soundhelper = SoundHelper::GetSingleton();
    soundhelper->Stop(ref_id);
    sound.enabled = false;
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
    if (ref_id != other.ref_id) {
        logger::critical("RefID not the same.");
        return;
    }
    if (tint_color.id != other.tint_color.id) {
        tint_color.id = other.tint_color.id;
    }
    if (art_object.id != other.art_object.id) {
        art_object.id = other.art_object.id;
    }
    if (effect_shader.id != other.effect_shader.id) {
        effect_shader.id = other.effect_shader.id;
    }
    if (sound.id != other.sound.id) {
        sound.id = other.sound.id;
    }
    if (fabs(stop_time - other.stop_time) > EPSILON) {
        stop_time = other.stop_time;
    }
}

void SoundHelper::DeleteHandle(const RefID refid) {
    if (!handles.contains(refid)) return;
    Stop(refid);
    std::unique_lock lock(mutex);
    handles.erase(refid);
}

void SoundHelper::Stop(const RefID refid) {
    std::shared_lock lock(mutex);
    if (!handles.contains(refid)) {
        return;
    }
    RE::BSSoundHandle& handle = handles.at(refid);
    if (!handle.IsPlaying()) {
        return;
    }
    handle.FadeOutAndRelease(1000);
    //handle.Stop();
}

void SoundHelper::Play(const RefID refid, const FormID sound_id, const float volume) {
    if (!sound_id) return;
    const auto sound = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(sound_id);
    if (!sound) {
        logger::error("Sound not found.");
        return;
    }
    const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid);
    if (!ref) {
        logger::error("Ref not found.");
        return;
    }
    const auto ref_node = ref->Get3D();
    if (!ref_node) {
        logger::warn("Ref has no 3D.");
        return;
    }

    if (std::unique_lock lock(mutex); !handles.contains(refid)) {
        handles[refid] = RE::BSSoundHandle();
    }

    std::shared_lock lock(mutex);

    auto& sound_handle = handles.at(refid);
    if (sound_handle.IsPlaying()) {
        return;
    }
    RE::BSAudioManager::GetSingleton()->BuildSoundDataFromDescriptor(sound_handle, sound);
    sound_handle.SetObjectToFollow(ref_node);
    sound_handle.SetVolume(volume);
    if (!sound_handle.IsValid()) {
        logger::error("SoundHandle not valid.");
    } else {
        sound_handle.FadeInPlay(1000);
    }
}