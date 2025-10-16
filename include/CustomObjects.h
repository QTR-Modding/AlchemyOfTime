#pragma once
#include <unordered_set>
#include "Utils.h"
#include "ClibUtil/singleton.hpp"

using Duration = float;
using DurationMGEFF = std::uint32_t;
using StageNo = unsigned int;
using StageName = std::string;

struct StageEffect {
    FormID beffect;          // base effect
    float magnitude;         // in effectitem
    std::uint32_t duration;  // in effectitem (not Duration, this is in seconds)

    StageEffect() : beffect(0), magnitude(0), duration(0) {}
    StageEffect(const FormID be, const float mag, const DurationMGEFF dur) : beffect(be), magnitude(mag), duration(dur) {}

    [[nodiscard]] bool IsNull() const { return beffect == 0; }
    [[nodiscard]] bool HasMagnitude() const { return magnitude != 0; }
    [[nodiscard]] bool HasDuration() const { return duration != 0; }
};


struct Stage {
    FormID formid = 0;  // with which item is it represented
    Duration duration;  // duration of the stage
    StageNo no;         // used for sorting when multiple stages are present
    StageName name;     // name of the stage
    std::vector<StageEffect> mgeffect;

    bool crafting_allowed;

    uint32_t color;


    Stage(){}
    Stage(const FormID f, const Duration d, const StageNo s, StageName n, const bool ca, const std::vector<StageEffect>& e, const uint32_t color_ = 0)
        : formid(f), duration(d), no(s), name(std::move(n)), mgeffect(e) ,crafting_allowed(ca), color(color_) {
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

    [[nodiscard]] RE::TESBoundObject* GetBound() const { return FormReader::GetFormByID<RE::TESBoundObject>(formid); }

    [[nodiscard]] bool CheckIntegrity() const;

    [[nodiscard]] const char* GetExtraText() const { return GetBound()->GetName(); }

};

struct StageInstancePlain{
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

    FormID form_id=0; // for fake stuff
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
        : start_time(st), no(n), count(c)
    {
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

    [[nodiscard]] RE::TESBoundObject* GetBound() const { return FormReader::GetFormByID<RE::TESBoundObject>(xtra.form_id); };

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

    [[nodiscard]] float GetTransformHittingTime(const float schranke) const{
        if (!xtra.is_transforming) return 0;
		return GetHittingTime(schranke+_elapsed);
    }

    [[nodiscard]] StageInstancePlain GetPlain() const;

    void SetDelay(const StageInstancePlain& plain);

private:
    float _elapsed; // y coord of the ausgangspunkt/elapsed time since the stage started
    float _delay_start;  // x coord of the ausgangspunkt
    float _delay_mag; // slope
    FormID _delay_formid; // formid of the time modulator

};

struct StageUpdate {
    const Stage* oldstage;
    const Stage* newstage;
    Count count=0;
    Duration update_time=0;
    bool new_is_fake=false;

    StageUpdate(const Stage* old, const Stage* new_, const Count c,
        const Duration u_t,
        const bool fake)
		: oldstage(old), newstage(new_), count(c), 
        update_time(u_t), 
        new_is_fake(fake) {}
};

struct AddOnSettings {
    std::unordered_set<FormID> containers;

    std::unordered_map<FormID,float> delayers;
    std::unordered_set<FormID> delayers_order;
    std::unordered_map<FormID,uint32_t> delayer_colors;
    std::unordered_map<FormID,FormID> delayer_sounds;
	std::unordered_map<FormID, FormID> delayer_artobjects;
	std::unordered_map<FormID, FormID> delayer_effect_shaders;
    std::unordered_map<FormID,std::unordered_set<FormID>> delayer_containers;
	std::unordered_map<FormID, std::unordered_set<StageNo>> delayer_allowed_stages;

    std::unordered_map<FormID, std::pair<FormID, Duration>> transformers;
	std::unordered_set<FormID> transformers_order;
    std::unordered_map<FormID,uint32_t> transformer_colors;
    std::unordered_map<FormID,FormID> transformer_sounds;
	std::unordered_map<FormID, FormID> transformer_artobjects;
	std::unordered_map<FormID, FormID> transformer_effect_shaders;
    std::unordered_map<FormID,std::unordered_set<FormID>> transformer_containers;
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

class SoundHelper : public clib_util::singleton::ISingleton<SoundHelper> {
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
	void Play(RefID refid, FormID sound_id, float volume);
};

struct RefStopFeature {
	uint32_t id = 0;
	std::atomic<bool> enabled = false;

    explicit operator bool() const;

    RefStopFeature();
    explicit RefStopFeature(const uint32_t i) : id(i) {}

    RefStopFeature& operator=(const RefStopFeature& other);

};

struct RefStop {

    ~RefStop();

    bool operator<(const RefStop& other) const {
        return ref_id < other.ref_id;
    }

    RefStop& operator=(const RefStop& other);
    
    RefID ref_id = 0;
    float stop_time = 0;
	RefStopFeature tint_color;
	RefStopFeature art_object;
	RefStopFeature effect_shader;
	RefStopFeature sound;

	//RE::ShaderReferenceEffect* shader_ref_eff;
	//RE::ModelReferenceEffect* model_ref_eff;

	std::unordered_set<FormID> applied_art_objects;
	std::unordered_set<FormID> applied_effect_shaders;

    RefStop() = default;
    explicit RefStop(RefID ref_id_);
	RefStop(const RefID ref_id_, const float stop_t, const uint32_t color, const FormID art_id, const FormID shader_id, const FormID sound_id)
		: ref_id(ref_id_),stop_time(stop_t), tint_color(color), art_object(art_id), effect_shader(shader_id), sound(sound_id) {
	}

	[[nodiscard]] bool IsDue(float curr_time) const;

    void ApplyTint(RE::NiAVObject* a_obj3d);
    void ApplyArtObject(RE::TESObjectREFR* a_ref, float duration=-1.f);
	void ApplyShader(RE::TESObjectREFR* a_ref, float duration=-1.f);
	void ApplySound(float volume=200.f);
    void ApplyAll(RE::TESObjectREFR* a_ref);
    [[nodiscard]] RE::BSSoundHandle& GetSoundHandle() const;

	void RemoveTint(RE::NiAVObject* a_obj3d);
	void RemoveArtObject();
	void RemoveShader();
	void RemoveSound();

    static bool HasArtObject(RE::TESObjectREFR* a_ref, const RE::BGSArtObject* a_art);

	void Update(const RefStop& other);

    RE::TESObjectREFR* GetRef() const {
        return RE::TESForm::LookupByID<RE::TESObjectREFR>(ref_id);
    }
};