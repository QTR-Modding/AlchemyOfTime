#pragma once
#include "DynamicFormTracker.h"
#include "Lorebox.h"
#include "CLibUtilsQTR/FormReader.hpp"

struct Source {
    using SourceData = std::unordered_map<RefID, std::vector<StageInstance>>;
    using StageDict = std::map<StageNo, Stage>;

    struct WorldTriggers {
        std::vector<std::variant<RE::BGSLocation*, RE::BGSPerk*, RE::TESBoundObject*>> ordered;
        std::vector<RE::TESBoundObject*> scan_bases;
    };

    SourceData data;

    FormID formid = 0;
    std::string editorid;
    std::string qFormType;
    DefaultSettings settings;
    std::unordered_map<StageNo, WorldTriggers> world_triggers;


    Source(const FormID id, const std::string& id_str, // NOLINT(modernize-pass-by-value)
           //RE::EffectSetting* e_m, 
           const DefaultSettings* sttngs = nullptr)
        : formid(id), editorid(id_str)
    //empty_mgeff(e_m)
    {
        Init(sttngs);
    }

    [[maybe_unused]] [[nodiscard]] std::string_view GetName() const;

    void UpdateAddons();

    [[nodiscard]] RE::TESBoundObject* GetBoundObject() const;;

    std::vector<StageUpdate> UpdateAllStages(RefID a_refID, float time);

    // daha once yaratilmis bi stage olmasi gerekiyo
    bool IsStage(FormID some_formid) const;

    [[nodiscard]] bool IsStageNo(StageNo no) const;

    [[nodiscard]] bool IsFakeStage(StageNo no) const;

    // assumes that the formid exists as a stage!
    [[nodiscard]] StageNo GetStageNo(FormID formid_) const;

    const Stage& GetStage(StageNo no);
    [[nodiscard]] const Stage* TryGetStage(StageNo no) const;

    [[nodiscard]] Duration GetStageDuration(StageNo no) const;

    [[nodiscard]] std::string GetStageName(StageNo no) const;

    StageInstance* InsertNewInstance(const StageInstance& stage_instance, RefID loc);

    StageInstance* InitInsertInstanceWO(StageNo n, Count c, RefID l, Duration t_0);

    // applies time modulation to all instances in the inventory
    [[nodiscard]] bool InitInsertInstanceInventory(StageNo n, Count c, const RefInfo& a_info, UpdateTime t_0,
                                                   const InvMap& inv);

    [[nodiscard]] bool MoveInstance(RefID from_ref, RefID to_ref, const StageInstance* st_inst);
    [[nodiscard]] bool MoveInstanceAt(RefID from_ref, RefID to_ref, size_t index);

    Count MoveInstances(RefID from_ref, RefID to_ref, FormID instance_formid, Count count, bool older_first);

    [[nodiscard]] bool IsDecayedItem(FormID _form_id) const;

    void UpdateTimeModulationInWorld(RE::TESObjectREFR* wo, StageInstance& wo_inst, float _time) const;

    // always update before doing this
    void UpdateTimeModulationInInventory(QueueInfo& queue_info, UpdateTime time, const InvMap& inv);


    float GetNextUpdateTime(const StageInstance* st_inst);
    float GetNextUpdateTime(const StageInstance* st_inst) const;

    void CleanUpData();
    void CleanUpData(RefID a_loc);

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
    void RebuildWorldTriggers();
    template <class T>
    void AddWorldTriggers(const tsl::ordered_map<FormID, T>& triggers,
                          const std::unordered_map<FormID, std::unordered_set<StageNo>>& allowed_stages);

    RE::FormType formtype;
    std::set<StageNo> fake_stages;
    Stage decayed_stage;
    std::unordered_map<FormID, Stage> transformed_stages;

    std::vector<StageInstance*> queued_time_modulator_updates;
    bool init_failed = false;

    StageDict stages;

    // counta karismiyor
    [[nodiscard]] bool UpdateStageInstanceHelper(StageInstance& st_inst, float curr_time,
                                                 const std::unordered_set<StageNo>& a_allowed_delayer_stages);
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
        Utils::OverrideMGEFFs(_effects, MGEFFs, pMGEFFdurations, pMGEFFmagnitudes);
    }

    // also adds keywords
    template <typename T>
    void GatherStages() {
        for (StageNo stage_no : settings.numbers) {
            const auto stage_formid = settings.items[stage_no];
            if (!stage_formid && stage_no != 0) {
                if (std::ranges::contains(Settings::fakes_allowedQFORMS, qFormType)) {
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

    void SetDelayOfInstance(StageInstance& instance, UpdateTime time, QueueInfo& queue_info, const InvMap& a_inv) const;
    void SetDelayOfInstance(StageInstance& instance, float a_time, FormID a_modulator) const;

    [[nodiscard]] bool CheckIntegrity();

    float GetDecayTime(const StageInstance& st_inst);

    inline void InitFailed();

    void RegisterStage(FormID stage_formid, StageNo stage_no);

    template <typename T>
    FormID FetchFake(StageNo st_no);;

    // also registers to stages
    FormID FetchFake(StageNo st_no);

    StageNo GetLastStageNo();

    static FormID FindWorldTrigger(RE::TESObjectREFR* a_obj, const WorldTriggers& triggers);
};

template <typename T>
FormID Source::FetchFake(const StageNo st_no) {
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
}