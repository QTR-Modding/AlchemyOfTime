#include "CustomObjects.h"
#include "Settings.h"


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

void SoundHelper::DeleteHandle(const RefID refid) {
    std::unique_lock lock(mutex);
    const auto it = handles.find(refid);
    if (it == handles.end()) return;
    Stop(refid);
    handles.erase(it);
}

void SoundHelper::Stop(const RefID refid) {
    std::unique_lock lock(mutex);
    const auto it = handles.find(refid);
    if (it == handles.end()) {
        return;
    }
    RE::BSSoundHandle& handle = it->second;
    if (!handle.IsPlaying()) {
        return;
    }
    handle.FadeOutAndRelease(1000);
    //handle.Stop();
}

bool SoundHelper::Play(const RE::TESObjectREFR* ref, const FormID sound_id, const float volume) {
    if (!sound_id) return false;
    const auto sound = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(sound_id);
    if (!sound) {
        logger::error("Sound not found.");
        return false;
    }
    if (!ref) {
        logger::error("Ref not found.");
        return false;
    }
    const auto ref_node = ref->Get3D();
    if (!ref_node) return false;
    std::unique_lock lock(mutex);
    auto& sound_handle = handles[ref->GetFormID()];

    if (sound_handle.IsPlaying()) {
        return false;
    }

    RE::BSAudioManager::GetSingleton()->GetSoundHandle(sound_handle, sound);
    sound_handle.SetObjectToFollow(ref_node);
    sound_handle.SetVolume(volume);
    if (!sound_handle.IsValid()) {
        logger::error("SoundHandle not valid.");
    } else {
        sound_handle.FadeInPlay(1000);
        return true;
    }

    return false;
}