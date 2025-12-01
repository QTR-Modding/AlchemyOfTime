#pragma once
#include "DynamicFormTracker.h"
#include "Lorebox.h"

struct Source {
    
    using SourceData = std::unordered_map<RefID,std::vector<StageInstance>>;
    using StageDict = std::map<StageNo, Stage>;

    SourceData data;

    FormID formid = 0;
    std::string editorid;
    std::string qFormType;
    DefaultSettings settings;

    
    Source(const FormID id, const std::string& id_str,   // NOLINT(modernize-pass-by-value)
        //RE::EffectSetting* e_m, 
        const DefaultSettings* sttngs=nullptr)
        : formid(id), editorid(id_str)
          //empty_mgeff(e_m)
          { Init(sttngs); }

    [[maybe_unused]] [[nodiscard]] std::string_view GetName() const;

    void UpdateAddons();

    [[nodiscard]] RE::TESBoundObject* GetBoundObject() const { return FormReader::GetFormByID<RE::TESBoundObject>(formid, editorid); };

    std::unordered_map<RefID, std::vector<StageUpdate>> UpdateAllStages(const std::vector<RefID>& filter, float time);

    // daha once yaratilmis bi stage olmasi gerekiyo
    bool IsStage(FormID some_formid) const;

    [[nodiscard]] inline bool IsStageNo(StageNo no) const;

    [[nodiscard]] inline bool IsFakeStage(StageNo no) const;

    // assumes that the formid exists as a stage!
    [[nodiscard]] StageNo GetStageNo(FormID formid_) const;

	const Stage& GetStage(StageNo no);
    [[nodiscard]] const Stage* GetStage(StageNo no) const;

    [[nodiscard]] Duration GetStageDuration(StageNo no) const;

    [[nodiscard]] std::string GetStageName(StageNo no) const;

    StageInstance* InsertNewInstance(const StageInstance& stage_instance, RefID loc);

    StageInstance* InitInsertInstanceWO(StageNo n, Count c, RefID l, Duration t_0);

    // applies time modulation to all instances in the inventory
    [[nodiscard]] bool InitInsertInstanceInventory(StageNo n, Count c, RE::TESObjectREFR* inventory_owner,
                                                   Duration t_0);
    
    [[nodiscard]] bool MoveInstance(RefID from_ref, RefID to_ref, const StageInstance* st_inst);

    Count MoveInstances(RefID from_ref, RefID to_ref, FormID instance_formid, Count count, bool older_first);
    
    [[nodiscard]] bool IsDecayedItem(FormID _form_id) const;

    FormID inline GetModulatorInInventory(RE::TESObjectREFR* inventory_owner, StageNo a_no) const;

    FormID inline GetModulatorInWorld(const RE::TESObjectREFR* wo, StageNo a_no) const;

    inline FormID GetTransformerInInventory(RE::TESObjectREFR* inventory_owner, StageNo a_no) const;

    inline FormID GetTransformerInWorld(const RE::TESObjectREFR* wo, StageNo a_no) const;

    // always update before doing this
    void UpdateTimeModulationInInventory(RE::TESObjectREFR* inventory_owner, float _time);

    void UpdateTimeModulationInWorld(RE::TESObjectREFR* wo, StageInstance& wo_inst, float _time) const;

    float GetNextUpdateTime(const StageInstance* st_inst);
    float GetNextUpdateTime(const StageInstance* st_inst) const;

    void CleanUpData();

    void PrintData();

    void Reset();
    
    [[nodiscard]] bool IsHealthy() const { 
        return !init_failed;
	}

	[[nodiscard]] const Stage& GetDecayedStage() const { return decayed_stage; }

	[[nodiscard]] bool ShouldFreezeEvolution(const FormID loc_formid) const {
        return !settings.containers.empty() && !settings.containers.contains(loc_formid);   
    }

private:

    void Init(const DefaultSettings* defaultsettings);

    RE::FormType formtype;
    std::set<StageNo> fake_stages;
    Stage decayed_stage;
    std::unordered_map<FormID, Stage> transformed_stages;

    std::vector<StageInstance*> queued_time_modulator_updates;
    bool init_failed = false;

    StageDict stages;

    // counta karismiyor
    [[nodiscard]] bool UpdateStageInstanceHelper(StageInstance& st_inst, float curr_time, const std::unordered_set<StageNo>& a_allowed_delayer_stages);
    [[nodiscard]] bool UpdateStageInstance(StageInstance& st_inst, float curr_time);

    template <typename T>
    void ApplyMGEFFSettings(T* stage_form, std::vector<StageEffect>& settings_effs) {
        RE::BSTArray<RE::Effect*> _effects = FormTraits<T>::GetEffects(stage_form);
        std::vector<FormID> MGEFFs;
        std::vector<uint32_t> pMGEFFdurations;
        std::vector<float> pMGEFFmagnitudes;

        // I need this many empty effects
        int n_empties = static_cast<int>(_effects.size()) - static_cast<int>(settings_effs.size());
        n_empties = std::max(n_empties, 0);

        for (auto& settings_eff : settings_effs) {
            MGEFFs.push_back(settings_eff.beffect);
            pMGEFFdurations.push_back(settings_eff.duration);
            pMGEFFmagnitudes.push_back(settings_eff.magnitude);
        }

        for (int j = 0; j < n_empties; j++) {
            MGEFFs.push_back(0);
            pMGEFFdurations.push_back(0);
            pMGEFFmagnitudes.push_back(0);
        }
        OverrideMGEFFs(_effects, MGEFFs, pMGEFFdurations, pMGEFFmagnitudes);
        
    }

    // also adds keywords
    template <typename T>
    void GatherStages()  {
        for (StageNo stage_no: settings.numbers) {
            const auto stage_formid = settings.items[stage_no];
            if (!stage_formid && stage_no != 0) {
                if (std::ranges::contains(Settings::fakes_allowedQFORMS, qFormType))
                {
                    fake_stages.insert(stage_no);
                    continue;
                }
                logger::critical("No ID given and copy items not allowed for this type {}", qFormType);
				return;
            }

            if (stage_no == 0) RegisterStage(formid, stage_no);
			else {
                if (auto stage_form = FormReader::GetFormByID<T>(stage_formid, ""); !stage_form) {
                    logger::error("Stage form {} not found.", stage_formid);
					continue;
				}
                RegisterStage(stage_formid, stage_no);
			}
        }
    }

    [[nodiscard]] size_t GetNStages() const;
    
    [[nodiscard]] Stage GetFinalStage() const;

    [[nodiscard]] Stage GetTransformedStage(FormID key_formid) const;

    void SetDelayOfInstances(float some_time, RE::TESObjectREFR* inventory_owner);

    void SetDelayOfInstance(StageInstance& instance, float curr_time, RE::TESObjectREFR* a_loc, bool inventory_owner=true) const;

    void SetDelayOfInstance(StageInstance& instance, float a_time, FormID a_modulator) const;
   
    [[nodiscard]] bool CheckIntegrity();

    float GetDecayTime(const StageInstance& st_inst);

    inline void InitFailed();

    void RegisterStage(FormID stage_formid, StageNo stage_no);

    template <typename T>
    FormID FetchFake(const StageNo st_no) {
        auto* DFT = DynamicFormTracker::GetSingleton();
        if (editorid.empty()) {
		    logger::error("Editorid is empty.");
		    return 0;
	    }
        const FormID new_formid = DFT->FetchCreate<T>(formid, editorid, static_cast<uint32_t>(st_no));

        if (auto stage_form = FormReader::GetFormByID<T>(new_formid)) {
            RegisterStage(new_formid, st_no);
            if (!stages.contains(st_no)) {
                logger::error("Stage {} not found in stages.", st_no);
                return 0;
            }

            // Update name of the fake form
            const auto& name = stages.at(st_no).name;
            const auto og_name = RE::TESForm::LookupByID(formid)->GetName();
            const auto new_name = std::string(og_name) + " (" + name + ")";
            if (!name.empty() && std::strcmp(stage_form->fullName.c_str(), new_name.c_str()) != 0) {
                stage_form->fullName = new_name;
                logger::trace("Updated name of fake form to {}", name);
            }

            // Update value of the fake form
            const auto temp_value = settings.costoverrides.contains(st_no) ? settings.costoverrides.at(st_no) : -1;
            if (temp_value >= 0) FormTraits<T>::SetValue(stage_form, temp_value);
            // Update weight of the fake form
            const auto temp_weight = settings.weightoverrides.contains(st_no) ? settings.weightoverrides.at(st_no) : -1;
            if (temp_weight >= 0) FormTraits<T>::SetWeight(stage_form, temp_weight);

			// Update magic effects of the fake form
            if (settings.effects.contains(st_no) && !settings.effects.at(st_no).empty() &&
                std::ranges::contains(Settings::mgeffs_allowedQFORMS, qFormType)) {
                // change mgeff of fake form
                ApplyMGEFFSettings(stage_form, settings.effects.at(st_no));
            }
        } else {
		    logger::error("Could not create copy form for source {}", editorid);
		    return 0;
	    }
     
        return new_formid;
    };

    // also registers to stages
    FormID FetchFake(StageNo st_no);

    StageNo GetLastStageNo();

    static FormID SearchNearbyModulators(const RE::TESObjectREFR* a_obj, const std::vector<FormID>& candidates);

    static void SearchModulatorInCell(FormID& result, const RE::TESObjectREFR* a_origin, const RE::TESObjectCELL* a_cell, const std::unordered_set<FormID>&
                                      modulators, float range=0);
};
