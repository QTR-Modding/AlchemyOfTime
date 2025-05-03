#include "CustomObjects.h"

bool StageInstance::operator==(const StageInstance& other) const
{
    return no == other.no && count == other.count && 
        //location == other.location &&
            fabs(start_time - other.start_time) < EPSILON && 
        fabs(_elapsed - other._elapsed) < EPSILON && xtra == other.xtra;
}

bool StageInstance::AlmostSameExceptCount(const StageInstance& other, const float curr_time) const
{
    // bcs they are in the same inventory they will have same delay magnitude
            // delay starts might be different but if the elapsed times are close enough, we don't care
    return no == other.no && 
        //location == other.location &&
            std::abs(start_time - other.start_time) < 0.015 &&
            std::abs(GetElapsed(curr_time) - other.GetElapsed(curr_time)) < 0.015 && xtra == other.xtra;
}

StageInstance& StageInstance::operator=(const StageInstance& other)
{
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

float StageInstance::GetElapsed(const float curr_time) const {
    if (fabs(_delay_mag) < EPSILON) return _elapsed;
    return (curr_time - _delay_start) * GetDelaySlope() + _elapsed;
}

float StageInstance::GetDelaySlope() const {
    return std::min(std::max(-1000.f, _delay_mag), 1000.f);
}

void StageInstance::SetNewStart(const float curr_time, const float overshot)
{
    // overshot: by how much is the schwelle already ueberschritten
    start_time = curr_time - overshot / (GetDelaySlope() + std::numeric_limits<float>::epsilon());
    _delay_start = start_time;
    _elapsed = 0;
}

void StageInstance::SetDelay(const float time, const float delay, const FormID formid)
{
    // yeni steigungla yeni ausgangspunkt yapiyoruz
    // call only from UpdateTimeModulationInInventory
    if (xtra.is_transforming) return;
    if (fabs(_delay_mag - delay) < EPSILON && _delay_formid == formid) return;

    _elapsed = GetElapsed(time);
    _delay_start = time;
	_delay_mag = delay;
    _delay_formid = formid;
}

void StageInstance::SetTransform(const float time, const FormID formid)
{
    if (xtra.is_transforming){
        if (_delay_formid != formid) {
            RemoveTransform(time);
            return SetTransform(time, formid);
        }
        return;
    } 
    SetDelay(time, 1, formid);
    xtra.is_transforming = true;
}

void StageInstance::RemoveTransform(const float curr_time)
{
    if (!xtra.is_transforming) return;
    xtra.is_transforming = false;
    _delay_start = curr_time;
    _delay_mag = 1;
    _delay_formid = 0;
}

void StageInstance::RemoveTimeMod(const float time)
{
    RemoveTransform(time);
    SetDelay(time, 1, 0);
}

StageInstancePlain StageInstance::GetPlain() const
{
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

void StageInstance::SetDelay(const StageInstancePlain& plain)
{
    _elapsed = plain._elapsed;
	_delay_start = plain._delay_start;
	_delay_mag = plain._delay_mag;
	_delay_formid = plain._delay_formid;
}

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
        logger::trace("Items size: {}, Durations size: {}, Stage names size: {}, Effects size: {}, Numbers size: {}",
                      items.size(), durations.size(), stage_names.size(), effects.size(), numbers.size());
        init_failed = true;
        return false;
    }
    if (items.size() != durations.size() || items.size() != stage_names.size() || items.size() != numbers.size()) {
		logger::error("Sizes do not match.");
        init_failed = true;
		return false;
	}
    for (auto i = 0; i < numbers.size(); i++) {
        if (!Vector::HasElement<StageNo>(numbers, i)) {
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
    for (const auto& [_formID, _transformer] : transformers) {
        const FormID _finalFormEditorID = std::get<0>(_transformer);
        const Duration _duration = std::get<1>(_transformer);
        std::vector<StageNo> _allowedStages = std::get<2>(_transformer);
        if (!GetFormByID(_formID) || !GetFormByID(_finalFormEditorID)) {
			logger::error("Formid not found.");
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
			if (!Vector::HasElement<StageNo>(numbers, _stage)) {
				logger::error("Stage {} not found in numbers.", _stage);
				init_failed = true;
				return false;
			}
		}

	}

	for (const auto& _formID : delayers | std::views::keys) {
		if (!GetFormByID(_formID)) {
			logger::error("Delayer formid {} not found.", _formID);
			init_failed = true;
			return false;
		}
	}

    for (const auto& _formID : containers) {
		if (!GetFormByID(_formID)) {
			logger::error("Container formid {} not found.", _formID);
			init_failed = true;
			return false;
		}
	}
	return true;
}

bool DefaultSettings::IsEmpty()
{
	if (numbers.empty()) {
		init_failed = true;
	    return true;
	}
	return false;
}

void DefaultSettings::Add(AddOnSettings& addon)
{
    if (addon.transformers.empty()) {
		logger::error("Transformers is empty.");
    }
	for (const auto& _formID : addon.containers) {
        if (!_formID) {
            logger::critical("AddOn has null formid.");
	        continue;
        }
		containers.insert(_formID);
    }
	for (const auto& [_formID, _delay] : addon.delayers) {
        if (!_formID) {
            logger::critical("AddOn has null formid.");
	        continue;
        }
        if (!delayers.contains(_formID)) {
			delayers_order.push_back(_formID);
		}
		delayers[_formID] = _delay;
    }
	for (auto& [_formID, _transformer] : addon.transformers) {
        if (!_formID) {
            logger::critical("AddOn has null formid.");
	        continue;
        }
		if (!transformers.contains(_formID)) {
			transformers_order.push_back(_formID);
        }
        if (auto& allowed_stages = std::get<2>(_transformer); allowed_stages.empty()) {
			for (const auto& key : numbers) {
				allowed_stages.push_back(key);
			}
		}
		transformers[_formID] = _transformer;
    }

	AddHelper(delayer_colors, addon.delayer_colors);
	AddHelper(transformer_colors, addon.transformer_colors);
	AddHelper(delayer_sounds, addon.delayer_sounds);
	AddHelper(transformer_sounds, addon.transformer_sounds);
	AddHelper(delayer_artobjects, addon.delayer_artobjects);
	AddHelper(transformer_artobjects, addon.transformer_artobjects);
	AddHelper(delayer_effect_shaders, addon.delayer_effect_shaders);
	AddHelper(transformer_effect_shaders, addon.transformer_effect_shaders);
	// containers
	for (const auto& [_formID, _containers] : addon.delayer_containers) {
		if (!_formID) {
			logger::critical("AddOn has null formid.");
			continue;
		}
		for (const auto& _container : _containers) {
			if (!_container) {
				logger::critical("AddOn has null formid.");
				continue;
			}
			delayer_containers[_formID].insert(_container);
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

void DefaultSettings::AddHelper(std::unordered_map<FormID, FormID>& dest, const std::unordered_map<FormID, FormID>& src)
{
	for (const auto& [_formID, _art] : src) {
		if (!_formID) {
			logger::critical("AddOn has null formid.");
			continue;
		}
		dest[_formID] = _art;
	}
}

RefStopFeature::operator bool() const { return id > 0 && enabled.load(); }

RefStopFeature::RefStopFeature() {
	id = 0;
	enabled = false;
}

RefStopFeature& RefStopFeature::operator=(const RefStopFeature& other)
{
	if (this != &other) {
		id = other.id;
		enabled = other.enabled.load();
	}
	return *this;
}

RefStop::~RefStop() {
	//auto soundhelper = SoundHelper::GetSingleton();
	//soundhelper->DeleteHandle(ref_id);
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

bool AddOnSettings::CheckIntegrity()
{
    for (const auto& [_formID, _transformer] : transformers) {
        const FormID _finalFormEditorID = std::get<0>(_transformer);
        const Duration _duration = std::get<1>(_transformer);
        std::vector<StageNo> _allowedStages = std::get<2>(_transformer);
        if (!GetFormByID(_formID) || !GetFormByID(_finalFormEditorID)) {
			logger::error("Form not found.");
			init_failed = true;
			return false;
		}
        if (_duration <= 0) {
			logger::error("Duration is less than or equal 0.");
			init_failed = true;
			return false;
		}
	}

	for (const auto& _formID : delayers | std::views::keys) {
		if (!GetFormByID(_formID)) {
			logger::error("Delayer form {} not found.", _formID);
			init_failed = true;
			return false;
		}
	}
	for (const auto& _formID : containers) {
		if (!GetFormByID(_formID)) {
			logger::error("Container form {} not found.", _formID);
			init_failed = true;
			return false;
		}
	}

	return true;
}

void RefStop::ApplyTint(RE::NiAVObject* a_obj3d) {
	if (!tint_color.id) return RemoveTint(a_obj3d);
	if (tint_color.enabled.load()) return;
    RE::NiColorA color;
	hexToRGBA(tint_color.id, color);
	a_obj3d->TintScenegraph(color);
	tint_color.enabled.store(true);
}

void RefStop::ApplyArtObject(RE::TESObjectREFR* a_ref, const float duration)
{
	if (!art_object.id) return RemoveArtObject();
	if (art_object.enabled.load()) return;
	const auto a_art_obj = RE::TESForm::LookupByID<RE::BGSArtObject>(art_object.id);
	if (!a_art_obj) {
		logger::error("Art object not found.");
		return;
	}
	//if (applied_art_objects.contains(art_object.id)) return;
	//if (HasArtObject(a_ref, a_art_obj)) return;
	SKSE::GetTaskInterface()->AddTask([a_ref, a_art_obj, duration]() {
		if (!a_ref || !a_art_obj) return;
	    a_ref->ApplyArtObject(a_art_obj, duration);
		});
	//model_ref_eff = a_model_ref_eff_ptr;
	applied_art_objects.insert(art_object.id);

	art_object.enabled.store(true);
}

void RefStop::ApplyShader(RE::TESObjectREFR* a_ref, const float duration)
{
	if (!effect_shader.id) return RemoveShader();
	if (effect_shader.enabled.load()) return;
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

	effect_shader.enabled.store(true);
}

void RefStop::ApplySound(const float volume)
{
	if (!sound.id) {
        return RemoveSound();
	}
	const auto soundhelper = SoundHelper::GetSingleton();
	soundhelper->Play(ref_id, sound.id, volume);
	sound.enabled.store(true);
}

void RefStop::ApplyAll(RE::TESObjectREFR* a_ref)
{
	if (const auto a_obj3d = a_ref->Get3D()) {
		ApplyTint(a_obj3d);
	}
	ApplyArtObject(a_ref);
	ApplyShader(a_ref);
	ApplySound();
}

RE::BSSoundHandle& RefStop::GetSoundHandle() const {
	auto* soundhelper = SoundHelper::GetSingleton();
	return soundhelper->GetHandle(ref_id);
}


void RefStop::RemoveTint(RE::NiAVObject* a_obj3d)
{
    const auto color = RE::NiColorA(0.0f, 0.0f, 0.0f, 0.0f);
    a_obj3d->TintScenegraph(color);
	tint_color.enabled.store(false);
}

void RefStop::RemoveArtObject()
{
	//if (model_ref_eff) model_ref_eff->finished = true;

	if (applied_art_objects.empty()) return;
	if (const auto a_ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(ref_id)) {
	    if (const auto processLists = RE::ProcessLists::GetSingleton()) {
		    const auto handle = a_ref->CreateRefHandle();
		    processLists->ForEachModelEffect([&](RE::ModelReferenceEffect* a_modelEffect) {
			    if (a_modelEffect->target == handle && a_modelEffect->artObject) {
					if (applied_art_objects.contains(a_modelEffect->artObject->GetFormID())) {
						if (a_modelEffect->lifetime<0.f) {
					        a_modelEffect->lifetime = a_modelEffect->age+5.f;
						}
					}
			    }
			    return RE::BSContainer::ForEachResult::kContinue;
		    });
	    }
	}
	applied_art_objects.clear();

	art_object.enabled.store(false);
}

void RefStop::RemoveShader()
{
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
							a_modelEffect->lifetime = a_modelEffect->age+5.f;
						}
					}
			    }
			    return RE::BSContainer::ForEachResult::kContinue;
		    });
	    }
	}
	applied_effect_shaders.clear();
	effect_shader.enabled.store(false);
}

void RefStop::RemoveSound()
{
	const auto soundhelper = SoundHelper::GetSingleton();
	soundhelper->Stop(ref_id);
	sound.enabled.store(false);
}

void RefStop::RemoveAll(RE::NiAVObject* a_obj3d)
{
	RemoveTint(a_obj3d);
	RemoveArtObject();
	RemoveShader();
	RemoveSound();
}

bool RefStop::HasArtObject(RE::TESObjectREFR* a_ref, const RE::BGSArtObject* a_art) {
	uint32_t count=0;
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

void RefStop::Update(const RefStop& other)
{
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

void SoundHelper::DeleteHandle(const RefID refid)
{
	if (!handles.contains(refid)) return;
	Stop(refid);
	std::unique_lock lock(mutex);
	handles.erase(refid);
}

void SoundHelper::Stop(const RefID refid)
{
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

void SoundHelper::Play(const RefID refid, const FormID sound_id, const float volume)
{
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

	
	if (!handles.contains(refid)) {
		std::unique_lock lock(mutex);
		handles[refid] = RE::BSSoundHandle();
	}

	auto& sound_handle = handles.at(refid);
	if (sound_handle.IsPlaying()) {
		return;
	}
	RE::BSAudioManager::GetSingleton()->BuildSoundDataFromDescriptor(sound_handle, sound);
	sound_handle.SetObjectToFollow(ref->Get3D());
	sound_handle.SetVolume(volume);
	if (!sound_handle.IsValid()) {
		logger::error("SoundHandle not valid.");
	}
	else {
		sound_handle.FadeInPlay(1000);
	}
}
