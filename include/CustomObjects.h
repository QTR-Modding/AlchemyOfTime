#pragma once
#include <REX/REX/Singleton.h>
#include <shared_mutex>
#include <unordered_set>
#include "Utils.h"

using Duration = float;
using DurationMGEFF = std::uint32_t;
using StageNo = unsigned int;
using StageName = std::string;

struct StageEffect {
    FormID beffect; // base effect
    float magnitude; // in effectitem
    std::uint32_t duration; // in effectitem (not Duration, this is in seconds)

    StageEffect() : beffect(0), magnitude(0), duration(0) {
    }

    StageEffect(const FormID be, const float mag, const DurationMGEFF dur) : beffect(be), magnitude(mag),
                                                                             duration(dur) {
    }

    [[nodiscard]] bool IsNull() const { return beffect == 0; }
    [[nodiscard]] bool HasMagnitude() const { return magnitude != 0; }
    [[nodiscard]] bool HasDuration() const { return duration != 0; }
};


struct Stage {
    FormID formid = 0; // with which item is it represented
    Duration duration; // duration of the stage
    StageNo no; // used for sorting when multiple stages are present
    StageName name; // name of the stage
    std::vector<StageEffect> mgeffect;

    bool crafting_allowed;

    uint32_t color;


    // ReSharper disable once CppPossiblyUninitializedMember
    Stage() {
    }

    Stage(const FormID f, const Duration d, const StageNo s, StageName n, const bool ca,
          const std::vector<StageEffect>& e, const uint32_t color_ = 0)
        : formid(f), duration(d), no(s), name(std::move(n)), mgeffect(e), crafting_allowed(ca), color(color_) {
        if (!formid) logger::critical("FormID is null");
        else logger::trace("Stage: FormID {:x}, Duration {}, StageNo {}, Name {}", formid, duration, no, name);
        if (e.empty()) mgeffect.clear();
        if (duration <= 0) {
            logger::critical("Duration is 0 or negative");
            duration = 0.1f;
        }
    }

    bool operator<(const Stage& other) const {
        if (formid < other.formid) return true;
        if (other.formid < formid) return false;
        return no < other.no;
    }

    bool operator==(const Stage& other) const {
        return no == other.no && formid == other.formid && duration == other.duration;
    }

    [[nodiscard]] RE::TESBoundObject* GetBound() const;

    [[nodiscard]] bool CheckIntegrity() const;

    [[nodiscard]] const char* GetExtraText() const { return GetBound()->GetName(); }
};

struct StageInstancePlain {
    float start_time;
    StageNo no;
    Count count;

    float _elapsed;
    float _delay_start;
    float _delay_mag;
    FormID _delay_formid;

    bool is_fake = false;
    bool is_decayed = false;
    bool is_transforming = false;

    bool is_faved = false;
    bool is_equipped = false;

    FormID form_id = 0; // for fake stuff
};

struct StageInstance {
    float start_time; // start time of the stage
    StageNo no;
    Count count;
    //RefID location;  // RefID of the container where the fake food is stored or the real food itself when it is
    // out in the world
    Types::FormEditorIDX xtra;

    //StageInstance() : start_time(0), no(0), count(0), location(0) {}
    StageInstance(const float st, const StageNo n, const Count c)
        : start_time(st), no(n), count(c) {
        _elapsed = 0;
        _delay_start = start_time;
        _delay_mag = 1;
        _delay_formid = 0;
    }

    //define ==
    // assumes that they are in the same inventory
    [[nodiscard]] bool operator==(const StageInstance& other) const;

    // times are very close (abs diff less than 0.015h = 0.9min)
    // assumes that they are in the same inventory
    [[nodiscard]] bool AlmostSameExceptCount(const StageInstance& other, float curr_time) const;

    StageInstance& operator=(const StageInstance& other);

    [[nodiscard]] RE::TESBoundObject* GetBound() const;;

    [[nodiscard]] inline float GetElapsed(float curr_time) const;

    [[nodiscard]] inline float GetDelaySlope() const;

    void SetNewStart(float start_t);
    void SetNewStart(float curr_time, float overshot);

    void SetDelay(float time, float delay, FormID formid);

    void SetTransform(float time, FormID formid);

    [[nodiscard]] float GetTransformElapsed(const float curr_time) const { return GetElapsed(curr_time) - _elapsed; }

    void RemoveTransform(float curr_time);

    void RemoveTimeMod(float time);

    [[nodiscard]] float GetDelayMagnitude() const { return GetDelaySlope(); }

    [[nodiscard]] FormID GetDelayerFormID() const { return _delay_formid; }

    [[nodiscard]] float GetHittingTime(const float schranke) const {
        // _elapsed + dt*_delay_mag = schranke
        return _delay_start + (schranke - _elapsed) / (GetDelaySlope() + std::numeric_limits<float>::epsilon());
    }

    [[nodiscard]] float GetTransformHittingTime(const float schranke) const {
        if (!xtra.is_transforming) return 0;
        return GetHittingTime(schranke + _elapsed);
    }

    [[nodiscard]] StageInstancePlain GetPlain() const;

    void SetDelay(const StageInstancePlain& plain);

private:
    float _elapsed; // y coord of the ausgangspunkt/elapsed time since the stage started
    float _delay_start; // x coord of the ausgangspunkt
    float _delay_mag; // slope
    FormID _delay_formid; // formid of the time modulator
};

struct StageUpdate {
    const Stage* oldstage;
    const Stage* newstage;
    Count count = 0;
    Duration update_time = 0;
    bool new_is_fake = false;

    StageUpdate(const Stage* old, const Stage* new_, const Count c,
                const Duration u_t,
                const bool fake)
        : oldstage(old), newstage(new_), count(c),
          update_time(u_t),
          new_is_fake(fake) {
    }
};

struct AddOnSettings {
    std::unordered_set<FormID> containers;

    std::unordered_map<FormID, float> delayers;
    std::unordered_set<FormID> delayers_order;
    std::unordered_map<FormID, uint32_t> delayer_colors;
    std::unordered_map<FormID, FormID> delayer_sounds;
    std::unordered_map<FormID, FormID> delayer_artobjects;
    std::unordered_map<FormID, FormID> delayer_effect_shaders;
    std::unordered_map<FormID, std::unordered_set<FormID>> delayer_containers;
    std::unordered_map<FormID, std::unordered_set<StageNo>> delayer_allowed_stages;

    std::unordered_map<FormID, std::pair<FormID, Duration>> transformers;
    std::unordered_set<FormID> transformers_order;
    std::unordered_map<FormID, uint32_t> transformer_colors;
    std::unordered_map<FormID, FormID> transformer_sounds;
    std::unordered_map<FormID, FormID> transformer_artobjects;
    std::unordered_map<FormID, FormID> transformer_effect_shaders;
    std::unordered_map<FormID, std::unordered_set<FormID>> transformer_containers;
    std::unordered_map<FormID, std::unordered_set<StageNo>> transformer_allowed_stages;


    [[nodiscard]] bool IsHealthy() const { return !init_failed; }

    [[nodiscard]] bool CheckIntegrity();

private:
    bool init_failed = false;
};

struct DefaultSettings : AddOnSettings {
    std::map<StageNo, FormID> items = {};
    std::map<StageNo, Duration> durations = {};
    std::map<StageNo, StageName> stage_names = {};
    std::map<StageNo, bool> crafting_allowed = {};
    std::map<StageNo, int> costoverrides = {};
    std::map<StageNo, float> weightoverrides = {};
    std::map<StageNo, std::vector<StageEffect>> effects = {};
    std::vector<StageNo> numbers = {};
    FormID decayed_id = 0;
    std::map<StageNo, uint32_t> colors = {};
    std::map<StageNo, FormID> sounds = {};
    std::map<StageNo, FormID> artobjects = {};
    std::map<StageNo, FormID> effect_shaders = {};

    [[nodiscard]] bool IsHealthy() const { return !init_failed; }

    [[nodiscard]] bool CheckIntegrity();

    [[nodiscard]] bool IsEmpty();

    void Add(AddOnSettings& addon);
    static void AddHelper(std::unordered_map<FormID, FormID>& dest, const std::unordered_map<FormID, FormID>& src);
    static void AddHelper(std::unordered_set<FormID>& dest, const std::unordered_set<FormID>& src);

private:
    bool init_failed = false;
};

using CustomSettings = std::map<std::vector<std::string>, DefaultSettings>;

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


struct RefStop {
    ~RefStop() = default;

    bool operator<(const RefStop& other) const { return ref_info.ref_id < other.ref_info.ref_id; }

    RefStop& operator=(const RefStop& other);

    RefInfo ref_info;
    float stop_time = 0;
    RefStopFeatures features;

    //RE::ShaderReferenceEffect* shader_ref_eff;
    //RE::ModelReferenceEffect* model_ref_eff;

    std::unordered_set<FormID> applied_art_objects;
    std::unordered_set<FormID> applied_effect_shaders;

    explicit RefStop(const RefID ref_id_) : ref_info(ref_id_) {
    }

    RefStop(const RefID ref_id_, const float stop_t, const RefStopFeatures& a_features)
        : ref_info(ref_id_), stop_time(stop_t), features(a_features) {
    }

    [[nodiscard]] bool IsDue(float curr_time) const;

    void ApplyTint(const RE::TESObjectREFR* a_obj);
    void ApplyArtObject(RE::TESObjectREFR* a_ref, float duration = -1.f);
    void ApplyShader(RE::TESObjectREFR* a_ref, float duration = -1.f);
    void ApplySound(float volume = 200.f);
    [[nodiscard]] RE::BSSoundHandle& GetSoundHandle() const;

    void RemoveTint();
    void RemoveArtObject();
    void RemoveShader();
    void RemoveSound();

    static bool HasArtObject(RE::TESObjectREFR* a_ref, const RE::BGSArtObject* a_art);

    void Update(const RefStop& other);

    RE::TESObjectREFR* GetRef() const {
        return ref_info.GetRef();
    }
};