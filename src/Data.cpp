#include "Data.h"
#include "CellScan.h"
#include "CLibUtilsQTR/DrawDebug.hpp"
#include "CLibUtilsQTR/BoundingBox.hpp"
#include "MCP.h"
#include "Manager.h"

void Source::Init(const DefaultSettings* defaultsettings) {
    if (!defaultsettings) {
        logger::error("Default settings is null.");
        InitFailed();
        return;
    }

    const auto* bound = GetBoundObject();
    if (!bound) {
        logger::error("Form not found.");
        InitFailed();
        return;
    }

    formid = bound->GetFormID();
    editorid = clib_util::editorID::get_editorID(bound);

    if (!formid || editorid.empty()) {
        logger::error("EditorID is empty.");
        InitFailed();
        return;
    }

    if (!Settings::IsItem(formid, "", true)) {
        logger::error("Form is not a suitable item.");
        InitFailed();
        return;
    }

    qFormType = Settings::GetQFormType(bound);
    if (qFormType.empty()) {
        logger::error("FormType is not one of the predefined types.");
        InitFailed();
        return;
    }

    // get settings
    settings = *defaultsettings;
    // put addons
    if (const auto addon = Settings::GetAddOnSettings(bound); addon && addon->IsHealthy()) {
        settings.Add(*addon);
    }

    formtype = bound->GetFormType();

    if (!stages.empty()) {
        logger::error("Stages shouldn't be already populated.");
        InitFailed();
        return;
    }

    // get stages

    // POPULATE THIS
    if (qFormType == "FOOD") {
        if (formtype == RE::FormType::AlchemyItem) GatherStages<RE::AlchemyItem>();
        else if (formtype == RE::FormType::Ingredient) GatherStages<RE::IngredientItem>();
    } else if (qFormType == "INGR") GatherStages<RE::IngredientItem>();
    else if (qFormType == "MEDC" || qFormType == "POSN") GatherStages<RE::AlchemyItem>();
    else if (qFormType == "ARMO") GatherStages<RE::TESObjectARMO>();
    else if (qFormType == "WEAP") GatherStages<RE::TESObjectWEAP>();
    else if (qFormType == "SCRL") GatherStages<RE::ScrollItem>();
    else if (qFormType == "BOOK") GatherStages<RE::TESObjectBOOK>();
    else if (qFormType == "SLGM") GatherStages<RE::TESSoulGem>();
    else if (qFormType == "MISC") GatherStages<RE::TESObjectMISC>();
    //else if (qFormType == "NPC") GatherStages<RE::TESNPC>();
    else {
        logger::error("QFormType is not one of the predefined types.");
        InitFailed();
        return;
    }

    // decayed stage
    decayed_stage = GetFinalStage();

    // transformed stages
    for (const auto& key : settings.transformers | std::views::keys) {
        const auto temp_stage = GetTransformedStage(key);
        transformed_stages[key] = temp_stage;
    }

    if (!CheckIntegrity()) {
        logger::critical("CheckIntegrity failed");
        InitFailed();
    }
}

std::string_view Source::GetName() const {
    if (const auto form = FormReader::GetFormByID(formid, editorid)) return form->GetName();
    return "";
}

void Source::UpdateAddons() {
    const auto form = FormReader::GetFormByID(formid, editorid);
    if (!form) {
        logger::error("UpdateAddons: Form not found.");
        return;
    }
    if (const auto addon = Settings::GetAddOnSettings(form); addon && addon->IsHealthy()) {
        settings.Add(*addon);
    }

    if (!settings.CheckIntegrity()) {
        logger::critical("Default settings integrity check failed.");
        InitFailed();
    }
}

RE::TESBoundObject* Source::GetBoundObject() const {
    return FormReader::GetFormByID<RE::TESBoundObject>(formid, editorid);
}

std::vector<StageUpdate> Source::UpdateAllStages(RefID a_refID, const float time) {
    if (init_failed) {
        logger::critical("UpdateAllStages: Initialisation failed.");
        return {};
    }

    std::vector<StageUpdate> updated_instances;
    if (data.empty()) {
        logger::warn("No data found for source {}", editorid);
        return updated_instances;
    }
    if (!data.contains(a_refID)) {
        logger::warn("RefID {} not found in data.", a_refID);
        return updated_instances;
    }
    for (auto& instances = data.at(a_refID); auto& instance : instances) {
        const Stage* old_stage = IsStageNo(instance.no) ? &GetStage(instance.no) : nullptr;
        if (UpdateStageInstance(instance, time)) {
            const Stage* new_stage = nullptr;
            if (instance.xtra.is_transforming) {
                instance.xtra.is_decayed = true;
                instance.xtra.is_fake = false;
                const auto temp_formid = instance.GetDelayerFormID();
                if (!transformed_stages.contains(temp_formid)) {
                    logger::error("Transformed stage not found.");
                    continue;
                }
                new_stage = &transformed_stages.at(temp_formid);
                instance.xtra.is_transforming = false;
            } else if (instance.xtra.is_decayed || !IsStageNo(instance.no)) {
                new_stage = &decayed_stage;
            }
            auto is_fake_ = IsFakeStage(instance.no);
            updated_instances.emplace_back(old_stage, new_stage ? new_stage : &GetStage(instance.no),
                                           instance.count, instance.start_time, is_fake_);
        }
    }
    return updated_instances;
}


bool Source::IsStage(const FormID some_formid) const {
    return std::ranges::any_of(stages | std::views::values, [&](const auto& stage) {
        return stage.formid == some_formid;
    });
}

inline bool Source::IsStageNo(const StageNo no) const {
    return stages.contains(no) || fake_stages.contains(no);
}

inline bool Source::IsFakeStage(const StageNo no) const {
    return fake_stages.contains(no);
}

StageNo Source::GetStageNo(const FormID formid_) const {
    for (auto& [key, value] : stages) {
        if (value.formid == formid_) return key;
    }
    return 0;
}

const Stage& Source::GetStage(const StageNo no) {
    static const Stage empty_stage;
    if (!IsStageNo(no)) {
        logger::error("Stage {} not found.", no);
        return empty_stage;
    }
    if (stages.contains(no)) return stages.at(no);
    if (IsFakeStage(no)) {
        if (const auto stage_formid = FetchFake(no); stage_formid != 0) {
            const auto& fake_stage = stages.at(no);
            return fake_stage;
        }
        logger::error("Stage {} formid is 0.", no);
        return empty_stage;
    }
    logger::error("Stage {} not found.", no);
    return empty_stage;
}

const Stage* Source::TryGetStage(const StageNo no) const {
    if (stages.contains(no)) return &stages.at(no);
    return nullptr;
}

Duration Source::GetStageDuration(const StageNo no) const {
    if (const auto it = stages.find(no); it != stages.end()) {
        return it->second.duration;
    }
    if (fake_stages.contains(no)) {
        if (settings.durations.contains(no)) {
            return settings.durations.at(no);
        }
    }
    return 0;
}


std::string Source::GetStageName(const StageNo no) const {
    if (stages.contains(no)) return stages.at(no).name;
    return "";
}

StageInstance* Source::InsertNewInstance(const StageInstance& stage_instance, const RefID loc) {
    if (init_failed) {
        logger::critical("InsertData: Initialisation failed.");
        return nullptr;
    }

    const auto n = stage_instance.no;
    if (!IsStageNo(n)) {
        logger::error("Stage {} does not exist.", n);
        return nullptr;
    }
    if (stage_instance.count <= 0) {
        logger::error("Count is less than or equal 0.");
        return nullptr;
    }
    /*if (stage_instance.location == 0) {
		logger::error("Location is 0.");
		return false;
	}*/
    if (stage_instance.xtra.form_id != GetStage(n).formid) {
        logger::error("Formid does not match the stage formid.");
        return nullptr;
    }
    if (stage_instance.xtra.is_fake != IsFakeStage(n)) {
        logger::error("Fake status does not match the stage fake status.");
        return nullptr;
    }
    if (stage_instance.xtra.is_decayed) {
        logger::error("Decayed status is true.");
        return nullptr;
    }
    if (stage_instance.xtra.crafting_allowed != GetStage(n).crafting_allowed) {
        logger::error("Crafting allowed status does not match the stage crafting allowed status.");
        return nullptr;
    }

    if (!data.contains(loc)) {
        data[loc] = {};
    }

    data.at(loc).push_back(stage_instance);

    // fillout the xtra of the emplaced instance
    // get the emplaced instance
    /*auto& emplaced_instance = data.back();
    emplaced_instance.xtra.form_id = stages[n].formid;
    emplaced_instance.xtra.editor_id = clib_util::editorID::get_editorID(stages[n].GetBound());
    emplaced_instance.xtra.crafting_allowed = stages[n].crafting_allowed;
    if (IsFakeStage(n)) emplaced_instance.xtra.is_fake = true;*/

    M->InstanceCountUpdate(1);

    return &data.at(loc).back();
}

StageInstance* Source::InitInsertInstanceWO(StageNo n, const Count c, const RefID l, const Duration t_0) {
    if (init_failed) {
        logger::critical("InitInsertInstance: Initialisation failed.");
        return nullptr;
    }
    if (!IsStageNo(n)) {
        logger::error("Stage {} does not exist.", n);
        return nullptr;
    }
    StageInstance new_instance(t_0, n, c);
    new_instance.xtra.form_id = GetStage(n).formid;
    new_instance.xtra.editor_id = clib_util::editorID::get_editorID(GetStage(n).GetBound());
    new_instance.xtra.crafting_allowed = GetStage(n).crafting_allowed;
    if (IsFakeStage(n)) new_instance.xtra.is_fake = true;

    return InsertNewInstance(new_instance, l);
}

bool Source::InitInsertInstanceInventory(const StageNo n, const Count c, const RefInfo& a_info,
                                         const Duration t_0, const InvMap& inv) {
    // isme takilma
    if (!InitInsertInstanceWO(n, c, a_info.ref_id, t_0)) {
        logger::error("InitInsertInstance failed.");
        return false;
    }

    SetDelayOfInstance(data[a_info.ref_id].back(), t_0, a_info.base_id, inv);
    return true;
}

bool Source::MoveInstance(const RefID from_ref, const RefID to_ref, const StageInstance* st_inst) {
    if (!st_inst) {
        return false;
    }

    auto mit = data.find(from_ref);
    if (mit == data.end()) {
        return false;
    }

    auto& from_instances = mit->second;
    if (from_instances.empty()) {
        return false;
    }

    // Ensure st_inst points into from_instances
    const StageInstance* base = from_instances.data();
    const StageInstance* end = base + from_instances.size();
    if (st_inst < base || st_inst >= end) {
        return false;
    }

    const size_t idx = static_cast<size_t>(st_inst - base);

    StageInstance moved = from_instances[idx];
    from_instances.erase(from_instances.begin() + idx);

    if (to_ref > 0) {
        data[to_ref].push_back(std::move(moved));
    } else {
        M->InstanceCountUpdate(-1);
    }

    return true;
}

bool Source::MoveInstanceAt(const RefID from_ref, const RefID to_ref, const size_t index) {
    if (!data.contains(from_ref)) return false;

    auto& from_instances = data.at(from_ref);
    if (index >= from_instances.size()) return false;

    StageInstance moved = from_instances[index];
    from_instances.erase(from_instances.begin() + static_cast<std::ptrdiff_t>(index));

    if (to_ref > 0) {
        data[to_ref].push_back(std::move(moved));
    } else {
        M->InstanceCountUpdate(-1);
    }

    return true;
}

Count Source::MoveInstances(const RefID from_ref, const RefID to_ref, const FormID instance_formid, Count count,
                            const bool older_first) {
    // older_first: true to move older instances first
    if (data.empty()) {
        logger::warn("No data found for source {}", editorid);
        return count;
    }
    if (init_failed) {
        logger::critical("MoveInstances: Initialisation failed.");
        return count;
    }
    if (count <= 0) {
        logger::error("Count is less than or equal 0.");
        return count;
    }
    if (!instance_formid) {
        logger::error("Instance formid is 0.");
        return count;
    }
    if (from_ref == to_ref) {
        logger::error("From and to refs are the same.");
        return count;
    }

    if (!data.contains(from_ref)) {
        logger::error("From refid not found in data.");
        return count;
    }

    std::vector<size_t> instances_candidates = {};
    size_t index_ = 0;
    for (const auto& st_inst : data.at(from_ref)) {
        if (st_inst.xtra.form_id == instance_formid && st_inst.count > 0) {
            instances_candidates.push_back(index_);
        }
        index_++;
    }

    if (instances_candidates.empty()) {
        logger::info("No instances found for formid {:x} and location {:x}", instance_formid, from_ref);
        return count;
    }

    const auto curr_time = RE::Calendar::GetSingleton()->GetHoursPassed();
    if (older_first) {
        std::ranges::sort(instances_candidates,
                          [this, from_ref, curr_time](const size_t a, const size_t b) {
                              return this->data.at(from_ref)[a].GetElapsed(curr_time) >
                                     this->data.at(from_ref)[b].GetElapsed(curr_time); // move the older stuff
                          });
    } else {
        std::ranges::sort(instances_candidates,
                          [this, from_ref, curr_time](const size_t a, const size_t b) {
                              return this->data.at(from_ref)[a].GetElapsed(curr_time) <
                                     this->data.at(from_ref)[b].GetElapsed(curr_time); // move the newer stuff
                          });
    }

    std::vector<size_t> removed_indices;
    for (size_t index : instances_candidates) {
        if (!count) break;

        StageInstance* instance;
        if (removed_indices.empty()) {
            instance = &data.at(from_ref)[index];
        } else {
            int shift = 0;
            for (const size_t removed_index : removed_indices) {
                if (index == removed_index) {
                    logger::critical("Index is equal to removed index.");
                    return count;
                }
                if (index > removed_index) shift++;
            }
            instance = &data.at(from_ref)[index - shift];
        }

        if (count <= instance->count) {
            instance->count -= count;
            StageInstance new_instance(*instance);
            new_instance.count = count;
            if (to_ref > 0 && !InsertNewInstance(new_instance, to_ref)) {
                logger::error("InsertNewInstance failed.");
                return 0;
            }
            count = 0;
        } else {
            const auto count_temp = count;
            count -= instance->count;
            if (!MoveInstance(from_ref, to_ref, instance)) {
                logger::error("MoveInstance failed.");
                return count_temp;
            }
            removed_indices.push_back(index);
        }
    }
    return count;
}

bool Source::IsDecayedItem(const FormID _form_id) const {
    // if it is one of the transformations counts as decayed
    if (decayed_stage.formid == _form_id) return true;
    return std::ranges::any_of(settings.transformers | std::views::values,
                               [&](const auto& trns_tpl) {
                                   return trns_tpl.first == _form_id;
                               });
}

inline FormID Source::GetModulatorInWorld(const RE::TESObjectREFR* wo, const StageNo a_no) const {
    std::vector<FormID> candidates;
    candidates.reserve(settings.delayers_order.size());

    for (const auto& dlyr_fid : settings.delayers_order) {
        if (!settings.delayer_allowed_stages.at(dlyr_fid).contains(a_no)) {
            continue;
        }
        candidates.push_back(dlyr_fid);
    }

    if (const auto hit = SearchNearbyModulatorsCached(wo, candidates); hit) {
        return hit;
    }

    return 0;
}

inline FormID Source::GetTransformerInWorld(const RE::TESObjectREFR* wo, const StageNo a_no) const {
    std::vector<FormID> candidates;
    candidates.reserve(settings.transformers_order.size());

    for (const auto& trns_fid : settings.transformers_order) {
        if (!settings.transformer_allowed_stages.at(trns_fid).contains(a_no)) {
            continue;
        }
        candidates.push_back(trns_fid);
    }

    if (const auto hit = SearchNearbyModulatorsCached(wo, candidates); hit) {
        return hit;
    }

    return 0;
}

void Source::UpdateTimeModulationInWorld(RE::TESObjectREFR* wo, StageInstance& wo_inst, const float _time) const {
    SetDelayOfInstance(wo_inst, _time, wo);
}

float Source::GetNextUpdateTime(const StageInstance* st_inst) {
    if (!st_inst) {
        logger::error("Stage instance is null.");
        return 0;
    }
    if (!IsHealthy()) {
        logger::critical("GetNextUpdateTime: Source is not healthy.");
        logger::critical("GetNextUpdateTime: Source formid: {}, qformtype: {}", formid, qFormType);
        return 0;
    }
    if (st_inst->xtra.is_decayed) return 0;
    if (!IsStageNo(st_inst->no)) {
        logger::error("Stage {} does not exist.", st_inst->no);
        return 0;
    }

    const auto delay_slope = st_inst->GetDelaySlope();
    if (std::abs(delay_slope) < EPSILON) {
        //logger::warn("Delay slope is 0.");
        return 0;
    }

    if (st_inst->xtra.is_transforming) {
        const auto transformer_form_id = st_inst->GetDelayerFormID();
        if (!settings.transformers.contains(transformer_form_id)) return 0.0f;
        const auto trnsfrm_duration = std::get<1>(settings.transformers.at(transformer_form_id));
        return st_inst->GetTransformHittingTime(trnsfrm_duration);
    }

    const auto schranke = delay_slope > 0 ? GetStageDuration(st_inst->no) : 0.f;

    return st_inst->GetHittingTime(schranke);
}

FormID Source::GetModulatorInInventory(const InvMap& inv, const FormID ownerBase, const StageNo no) const {
    for (auto dlyr_fid : settings.delayers_order) {
        if (!settings.delayer_allowed_stages.at(dlyr_fid).contains(no)) continue;
        auto obj = RE::TESForm::LookupByID<RE::TESBoundObject>(dlyr_fid);
        if (!obj) continue;
        if (auto it = inv.find(obj); it != inv.end() && it->second.first > 0) {
            auto contIt = settings.delayer_containers.find(dlyr_fid);
            if (contIt == settings.delayer_containers.end() || contIt->second.empty() ||
                contIt->second.contains(ownerBase))
                return dlyr_fid;
        }
    }
    return 0;
}

FormID Source::GetTransformerInInventory(const InvMap& inv, const FormID ownerBase, const StageNo no) const {
    for (auto trns_fid : settings.transformers_order) {
        if (!settings.transformer_allowed_stages.at(trns_fid).contains(no)) continue;
        auto obj = RE::TESForm::LookupByID<RE::TESBoundObject>(trns_fid);
        if (!obj) continue;
        if (auto it = inv.find(obj); it != inv.end() && it->second.first > 0) {
            auto contIt = settings.transformer_containers.find(trns_fid);
            if (contIt == settings.transformer_containers.end() || contIt->second.empty() ||
                contIt->second.contains(ownerBase))
                return trns_fid;
        }
    }
    return 0;
}

void Source::SetDelayOfInstances(const float t, const RefInfo& a_info, const InvMap& inv) {
    const auto loc = a_info.ref_id;
    if (!data.contains(loc)) return;

    const auto ownerBase = a_info.base_id;

    for (auto& inst : data.at(loc)) {
        if (inst.count <= 0) continue;
        if (ShouldFreezeEvolution(ownerBase)) {
            inst.RemoveTimeMod(t);
            inst.SetDelay(t, 0, 0);
            continue;
        }

        if (const auto tr = GetTransformerInInventory(inv, ownerBase, inst.no))
            SetDelayOfInstance(inst, t, tr);
        else if (const auto dl = GetModulatorInInventory(inv, ownerBase, inst.no))
            SetDelayOfInstance(inst, t, dl);
        else
            inst.RemoveTimeMod(t);
    }
}

void Source::UpdateTimeModulationInInventory(const RefInfo& a_info, const float t, const InvMap& inv) {
    if (!data.contains(a_info.ref_id)) return;
    if (data.at(a_info.ref_id).empty()) return;
    SetDelayOfInstances(t, a_info, inv);
}


float Source::GetNextUpdateTime(const StageInstance* st_inst) const {
    if (!st_inst) {
        logger::error("Stage instance is null.");
        return 0;
    }
    if (!IsHealthy()) {
        logger::critical("GetNextUpdateTime: Source is not healthy.");
        logger::critical("GetNextUpdateTime: Source formid: {}, qformtype: {}", formid, qFormType);
        return 0;
    }
    if (st_inst->xtra.is_decayed) return 0;
    if (!IsStageNo(st_inst->no)) {
        logger::error("Stage {} does not exist.", st_inst->no);
        return 0;
    }

    const auto delay_slope = st_inst->GetDelaySlope();
    if (std::abs(delay_slope) < EPSILON) {
        //logger::warn("Delay slope is 0.");
        return 0;
    }

    if (st_inst->xtra.is_transforming) {
        const auto transformer_form_id = st_inst->GetDelayerFormID();
        if (!settings.transformers.contains(transformer_form_id)) return 0.0f;
        const auto trnsfrm_duration = std::get<1>(settings.transformers.at(transformer_form_id));
        return st_inst->GetTransformHittingTime(trnsfrm_duration);
    }

    const auto schranke = delay_slope > 0 ? GetStageDuration(st_inst->no) : 0.f;

    return st_inst->GetHittingTime(schranke);
}

void Source::CleanUpData() {
    if (!CheckIntegrity()) {
        logger::critical("CheckIntegrity failed");
        InitFailed();
    }

    if (init_failed) {
        logger::critical("CleanUpData: Initialisation failed.");
        return;
    }
    if (data.empty()) {
        logger::info("No data found for source {}", editorid);
        return;
    }

    uint32_t removed = 0;

    const auto curr_time = RE::Calendar::GetSingleton()->GetHoursPassed();
    for (auto& instances : data | std::views::values) {
        if (instances.empty()) continue;
        if (instances.size() > 1) {
            for (auto it = instances.begin(); it + 1 != instances.end(); ++it) {
                for (auto it2 = it + 1; it2 != instances.end(); ++it2) {
                    if (it == it2) continue;
                    if (it2->count <= 0) continue;
                    if (it->AlmostSameExceptCount(*it2, curr_time)) {
                        it->count += it2->count;
                        it2->count = 0;
                    }
                }
            }
        }
        for (auto it = instances.begin(); it != instances.end();) {
            if (it->count <= 0 ||
                it->start_time > curr_time ||
                (it->xtra.is_decayed || !IsStageNo(it->no))) {
                it = instances.erase(it);
                ++removed;
                continue;
            }

            //check if current time modulator is valid
            const auto curr_delayer = it->GetDelayerFormID();
            if (it->xtra.is_transforming) {
                if (!settings.transformers.contains(curr_delayer)) {
                    logger::warn("Transformer FormID {:x} not found in default settings.", curr_delayer);
                    it->RemoveTimeMod(curr_time);
                }
            } else if (curr_delayer != 0 && !settings.delayers.contains(curr_delayer)) {
                logger::warn("Delayer FormID {:x} not found in default settings.", curr_delayer);
                it->RemoveTimeMod(curr_time);
            }

            if (curr_time - GetDecayTime(*it) > static_cast<float>(Settings::nForgettingTime)) {
                it = instances.erase(it);
                ++removed;
                continue;
            }
            ++it;
        }
    }

    for (auto it = data.begin(); it != data.end();) {
        if (it->second.empty()) {
            it = data.erase(it);
        } else ++it;
    }

    if (removed) {
        M->InstanceCountUpdate(-static_cast<int32_t>(removed));
    }
}

void Source::CleanUpData(const RefID a_loc) {
    /*if (!CheckIntegrity()) {
        logger::critical("CheckIntegrity failed");
        InitFailed();
    }*/

    if (init_failed) {
        logger::critical("CleanUpData: Initialisation failed.");
        return;
    }
    if (data.empty()) {
        return;
    }

    const auto curr_time = RE::Calendar::GetSingleton()->GetHoursPassed();
    const auto it_instances = data.find(a_loc);
    if (it_instances == data.end()) {
        return;
    }
    auto& instances = it_instances->second;
    if (instances.empty()) {
        return;
    }

    uint32_t removed = 0;

    if (instances.size() > 1) {
        for (auto it = instances.begin(); it + 1 != instances.end(); ++it) {
            for (auto it2 = it + 1; it2 != instances.end(); ++it2) {
                if (it == it2) continue;
                if (it2->count <= 0) continue;
                if (it->AlmostSameExceptCount(*it2, curr_time)) {
                    it->count += it2->count;
                    it2->count = 0;
                }
            }
        }
    }
    for (auto it = instances.begin(); it != instances.end();) {
        if (it->count <= 0 || it->start_time > curr_time || (it->xtra.is_decayed || !IsStageNo(it->no))) {
            it = instances.erase(it);
            ++removed;
            continue;
        }

        // check if current time modulator is valid
        const auto curr_delayer = it->GetDelayerFormID();
        if (it->xtra.is_transforming) {
            if (!settings.transformers.contains(curr_delayer)) {
                logger::warn("Transformer FormID {:x} not found in default settings.", curr_delayer);
                it->RemoveTimeMod(curr_time);
            }
        } else if (curr_delayer != 0 && !settings.delayers.contains(curr_delayer)) {
            logger::warn("Delayer FormID {:x} not found in default settings.", curr_delayer);
            it->RemoveTimeMod(curr_time);
        }

        if (curr_time - GetDecayTime(*it) > static_cast<float>(Settings::nForgettingTime)) {
            it = instances.erase(it);
            ++removed;
            continue;
        }
        ++it;
    }

    if (instances.empty()) {
        data.erase(it_instances);
    }

    if (removed) {
        M->InstanceCountUpdate(-static_cast<int32_t>(removed));
    }
}

void Source::PrintData() {
    if (init_failed) {
        logger::critical("PrintData: Initialisation failed.");
        return;
    }
    int n_print = 0;
    logger::info("Printing data for source -{}-", editorid);
    for (auto& [loc,instances] : data) {
        if (data[loc].empty()) continue;
        logger::info("Location: {}", loc);
        for (auto& instance : instances) {
            logger::info(
                "No: {}, Count: {}, Start time: {}, Stage Duration {}, Delay Mag {}, Delayer {}, isfake {}, istransforming {}, isdecayed {}",
                instance.no, instance.count, instance.start_time, GetStage(instance.no).duration,
                instance.GetDelayMagnitude(), instance.GetDelayerFormID(), instance.xtra.is_fake,
                instance.xtra.is_transforming, instance.xtra.is_decayed);
            if (n_print > 200) {
                logger::info("Print limit reached.");
                break;
            }
            n_print++;
        }
        if (n_print > 200) {
            logger::info("Print limit reached.");
            break;
        }
    }
}

void Source::Reset() {
    formid = 0;
    editorid = "";
    stages.clear();
    data.clear();
    init_failed = false;
}

bool Source::UpdateStageInstanceHelper(StageInstance& st_inst, const float curr_time,
                                       const std::unordered_set<StageNo>& a_allowed_delayer_stages) {
    if (st_inst.xtra.is_decayed) return false;

    const bool is_backwards = st_inst.GetDelaySlope() < 0;
    const auto a_schranke = is_backwards ? 0.f : GetStage(st_inst.no).duration;

    if (const auto hit_t = st_inst.GetHittingTime(a_schranke); hit_t <= curr_time) {
        if (is_backwards && st_inst.no > 0) {
            --st_inst.no;
        } else if (!is_backwards) {
            ++st_inst.no;
        } else {
            st_inst.SetNewStart(curr_time);
            return false;
        }
        const bool is_stage = IsStageNo(st_inst.no);
        const auto& new_stage = is_stage ? GetStage(st_inst.no) : decayed_stage;
        st_inst.xtra.form_id = new_stage.formid;
        st_inst.xtra.editor_id = clib_util::editorID::get_editorID(new_stage.GetBound());
        st_inst.xtra.is_fake = IsFakeStage(st_inst.no);
        st_inst.xtra.crafting_allowed = new_stage.crafting_allowed;
        st_inst.xtra.is_decayed = !IsStageNo(st_inst.no);

        st_inst.SetNewStart(hit_t);

        if (!a_allowed_delayer_stages.contains(st_inst.no)) {
            st_inst.RemoveTimeMod(hit_t);
        }

        return true;
    }

    return false;
}

bool Source::UpdateStageInstance(StageInstance& st_inst, const float curr_time) {
    if (st_inst.xtra.is_decayed) return false; // decayed

    const auto curr_delayer = st_inst.GetDelayerFormID(); // curr transformer or delayer

    if (st_inst.xtra.is_transforming) {
        if (!settings.transformers.contains(curr_delayer)) {
            logger::error("Transformer Formid {:x} not found in default settings.", curr_delayer);
            st_inst.RemoveTimeMod(curr_time);
            return false;
        }
        if (const auto hit_t = st_inst.GetTransformHittingTime(settings.transformers.at(curr_delayer).second);
            hit_t <= curr_time) {
            const auto& transformed_stage = transformed_stages.at(curr_delayer);
            st_inst.xtra.form_id = transformed_stage.formid;
            st_inst.SetNewStart(hit_t);
            return true;
        }
        return false;
    }
    if (GetNStages() < 2 && GetFinalStage().formid == st_inst.xtra.form_id) {
        st_inst.SetNewStart(curr_time);
        return false;
    }
    if (!IsStageNo(st_inst.no)) {
        return false;
    }
    if (st_inst.count <= 0) {
        return false;
    }
    if (curr_delayer == 0 && GetStage(st_inst.no).duration > Settings::critical_stage_dur) {
        st_inst.SetNewStart(curr_time);
        return false;
    }

    if (curr_delayer != 0 && !settings.delayers.contains(curr_delayer)) {
        logger::error("Delayer Formid {:x} not found in default settings.", curr_delayer);
        st_inst.RemoveTimeMod(curr_time);
        return false;
    }

    bool updated = false;
    const std::unordered_set<StageNo> allowed_delayer_stages = curr_delayer != 0
                                                                   ? settings.delayer_allowed_stages.at(curr_delayer)
                                                                   : std::unordered_set<StageNo>{};
    while (UpdateStageInstanceHelper(st_inst, curr_time, allowed_delayer_stages)) {
        updated = true;
    }

    return updated;
}

size_t Source::GetNStages() const {
    std::set<StageNo> temp_stage_nos;
    for (const auto& stage_no : stages | std::views::keys) {
        temp_stage_nos.insert(stage_no);
    }
    for (const auto& fake_no : fake_stages) {
        temp_stage_nos.insert(fake_no);
    }
    return temp_stage_nos.size();
}

Stage Source::GetFinalStage() const {
    Stage dcyd_st;
    dcyd_st.formid = settings.decayed_id;
    dcyd_st.duration = 0.1f; // just to avoid error in checkintegrity
    dcyd_st.crafting_allowed = false;
    return dcyd_st;
}

Stage Source::GetTransformedStage(const FormID key_formid) const {
    Stage trnsf_st;
    if (!settings.transformers.contains(key_formid)) {
        logger::error("Transformer Formid {:x} not found in settings.", key_formid);
        return trnsf_st;
    }
    const auto& [fst, snd] = settings.transformers.at(key_formid);
    trnsf_st.formid = fst;
    trnsf_st.duration = 0.1f; // just to avoid error in checkintegrity
    return trnsf_st;
}

void Source::SetDelayOfInstance(StageInstance& instance, const float curr_time, const FormID inv_owner_base,
                                const InvMap& a_inv) const {
    if (instance.count <= 0) return;
    if (ShouldFreezeEvolution(inv_owner_base)) {
        instance.RemoveTimeMod(curr_time);
        instance.SetDelay(curr_time, 0, 0); // freeze
        return;
    }

    if (const auto transformer_best =
        GetTransformerInInventory(a_inv, inv_owner_base, instance.no)) {
        SetDelayOfInstance(instance, curr_time, transformer_best);
    } else if (const auto delayer_best =
        GetModulatorInInventory(a_inv, inv_owner_base, instance.no)) {
        SetDelayOfInstance(instance, curr_time, delayer_best);
    } else {
        instance.RemoveTimeMod(curr_time);
    }
}

void Source::SetDelayOfInstance(StageInstance& instance, const float curr_time, RE::TESObjectREFR* a_loc) const {
    if (instance.count <= 0) return;
    const auto a_loc_base = a_loc->GetBaseObject()->GetFormID();
    if (ShouldFreezeEvolution(a_loc_base)) {
        instance.RemoveTimeMod(curr_time);
        instance.SetDelay(curr_time, 0, 0); // freeze
        return;
    }

    if (const auto transformer_best = GetTransformerInWorld(a_loc, instance.no)) {
        SetDelayOfInstance(instance, curr_time, transformer_best);
    } else if (const auto delayer_best = GetModulatorInWorld(a_loc, instance.no)) {
        SetDelayOfInstance(instance, curr_time, delayer_best);
    } else {
        instance.RemoveTimeMod(curr_time);
    }
}

void Source::SetDelayOfInstance(StageInstance& instance, const float a_time, const FormID a_modulator) const {
    if (settings.transformers.contains(a_modulator)) {
        instance.SetTransform(a_time, a_modulator);
        return;
    }
    instance.RemoveTransform(a_time);
    const float delay_ = !a_modulator
                             ? 1
                             : settings.delayers.contains(a_modulator)
                             ? settings.delayers.at(a_modulator)
                             : 1;
    instance.SetDelay(a_time, delay_, a_modulator);
}

bool Source::CheckIntegrity() {
    if (init_failed) {
        logger::error("CheckIntegrity: Initialisation failed.");
        return false;
    }

    if (formid == 0 || stages.empty() || qFormType.empty()) {
        logger::error("One of the members is empty.");
        return false;
    }

    if (!GetBoundObject()) {
        logger::error("FormID {} does not exist.", formid);
        return false;
    }

    if (!settings.CheckIntegrity()) {
        logger::error("Default settings integrity check failed.");
        return false;
    }

    std::set<StageNo> st_numbers_check;
    for (auto& [st_no,stage_tmp] : stages) {
        if (!stage_tmp.CheckIntegrity()) {
            logger::error("Stage no {} integrity check failed. FormID {}", st_no, stage_tmp.formid);
            return false;
        }
        // also need to check if qformtype is the same as source's qformtype
        const auto stage_formid = stage_tmp.formid;
        if (const auto stage_qformtype = Settings::GetQFormType(stage_formid); stage_qformtype != qFormType) {
            logger::error("Stage {} qformtype is not the same as the source qformtype.", st_no);
            return false;
        }

        st_numbers_check.insert(st_no);
    }
    for (auto st_no : fake_stages) {
        st_numbers_check.insert(st_no);
    }

    if (st_numbers_check.empty()) {
        logger::error("No stages found.");
        return false;
    }

    // stages must have keys [0,...,n-1]
    const auto st_numbers_check_vector = std::vector<StageNo>(st_numbers_check.begin(), st_numbers_check.end());
    //std::sort(st_numbers_check_vector.begin(), st_numbers_check_vector.end());
    if (st_numbers_check_vector[0] != 0) {
        logger::error("Stage 0 does not exist.");
        return false;
    }
    for (size_t i = 1; i < st_numbers_check_vector.size(); i++) {
        if (st_numbers_check_vector[i] != st_numbers_check_vector[i - 1] + 1) {
            logger::error("Stages are not incremented by 1:");
            for (auto st_no : st_numbers_check_vector) {
                logger::error("Stage {}", st_no);
            }
            return false;
        }
    }

    for (const auto& stage : stages | std::views::values) {
        if (!stage.CheckIntegrity()) {
            logger::error("Stage integrity check failed for stage no {} and source {} {}", stage.no, formid, editorid);
            return false;
        }
    }

    if (!decayed_stage.CheckIntegrity()) {
        logger::critical("Decayed stage integrity check failed.");
        InitFailed();
        return false;
    }

    return true;
}

float Source::GetDecayTime(const StageInstance& st_inst) {
    if (const auto slope = st_inst.GetDelaySlope(); slope <= 0) return -1.f;
    StageNo curr_stageno = st_inst.no;
    if (!IsStageNo(curr_stageno)) {
        logger::error("Stage {} does not exist.", curr_stageno);
        return -1.f;
    }
    const auto last_stage_no = GetLastStageNo();
    float total_duration = 0;
    while (curr_stageno <= last_stage_no) {
        total_duration += GetStage(curr_stageno).duration;
        curr_stageno += 1;
    }
    return st_inst.GetHittingTime(total_duration);
}

inline void Source::InitFailed() {
    logger::error("Initialisation failed for formid {:x}.", formid);
    Reset();
    init_failed = true;
}

void Source::RegisterStage(const FormID stage_formid, const StageNo stage_no) {
    for (const auto& value : stages | std::views::values) {
        if (stage_formid == value.formid) {
            logger::error("stage_formid {:x} intended for stage {} is already in stage {}.", stage_formid, stage_no,
                          value.no);
            return;
        }
    }
    if (stage_formid == formid && stage_no != 0) {
        // not allowed. if you want to go back to beginning use decayed stage
        logger::error("FormID of non initial stage is equal to source FormID.");
        return;
    }

    if (!stage_formid) {
        logger::error("Could not create copy form for stage {}", stage_no);
        return;
    }

    const auto stage_form = FormReader::GetFormByID(stage_formid);
    if (!stage_form) {
        logger::error("Could not create copy form for stage {}", stage_no);
        return;
    }

    const auto duration = settings.durations[stage_no];
    const StageName& name = settings.stage_names[stage_no];

    // create stage
    Stage stage(stage_formid, duration, stage_no, name, settings.crafting_allowed[stage_no],
                settings.effects[stage_no]);
    if (!stages.insert({stage_no, stage}).second) {
        logger::error("Could not insert stage");
        return;
    }

    if (M) {
        M->IndexStage(stage_formid, formid);
    }

    Lorebox::AddKeyword(stage_form->As<RE::BGSKeywordForm>(), stage_formid);
}

FormID Source::FetchFake(const StageNo st_no) {
    if (!GetBoundObject()) {
        logger::error("Could not get bound object. FormID {:x}", formid);
        return 0;
    }
    if (editorid.empty()) {
        logger::error("EditorID is empty.");
        return 0;
    }
    if (!std::ranges::contains(Settings::fakes_allowedQFORMS, qFormType)) {
        logger::error("Fake not allowed for this form type {}", qFormType);
        return 0;
    }

    FormID new_formid;

    switch (formtype) {
        case RE::FormType::Armor:
            new_formid = FetchFake<RE::TESObjectARMO>(st_no);
            break;
        case RE::FormType::AlchemyItem:
            new_formid = FetchFake<RE::AlchemyItem>(st_no);
            break;
        case RE::FormType::Book:
            new_formid = FetchFake<RE::TESObjectBOOK>(st_no);
            break;
        case RE::FormType::Ingredient:
            new_formid = FetchFake<RE::IngredientItem>(st_no);
            break;
        case RE::FormType::Misc:
            new_formid = FetchFake<RE::TESObjectMISC>(st_no);
            break;
        case RE::FormType::Weapon:
            new_formid = FetchFake<RE::TESObjectWEAP>(st_no);
            break;
        default:
            logger::error("Form type not found.");
            new_formid = 0;
            break;
    }

    if (!new_formid) {
        logger::error("Could not create copy form for source {}", editorid);
        return 0;
    }

    return new_formid;
}

StageNo Source::GetLastStageNo() {
    std::set<StageNo> stage_numbers;
    for (const auto& key : stages | std::views::keys) {
        stage_numbers.insert(key);
    }
    for (const auto& stage_no : fake_stages) {
        stage_numbers.insert(stage_no);
    }
    if (stage_numbers.empty()) {
        logger::error("No stages found.");
        InitFailed();
        return 0;
    }
    // return maximum of the set
    return *stage_numbers.rbegin();
}


namespace {
    RE::bhkRigidBody* GetRigidBody(const RE::TESObjectREFR* a_refr) {
        if (const auto a_3d = a_refr->GetCurrent3D()) {
            if (const auto a_collobj = a_3d->GetCollisionObject()) {
                return a_collobj->GetRigidBody();
            }
        }
        return nullptr;
    }

    bool AreClose(const DirectX::BoundingOrientedBox& obb1_in, const DirectX::BoundingOrientedBox& obb2_in,
                  const float threshold) {
        DirectX::BoundingOrientedBox obb1 = obb1_in;

        obb1.Extents.x += threshold;
        obb1.Extents.y += threshold;
        obb1.Extents.z += threshold;

        return obb1.Intersects(obb2_in);
    }

    bool SearchModulatorInCell_Sub(const RE::TESObjectREFR* a_origin, const RE::TESObjectREFR* ref,
                                   const float proximity = Settings::proximity_range) {
        DirectX::BoundingOrientedBox obb1{};
        DirectX::BoundingOrientedBox obb2{};

        if (const auto a_rigidbody1 = GetRigidBody(a_origin)) {
            BoundingBox::GetOBB(a_rigidbody1, obb1);
        } else {
            BoundingBox::GetOBB(a_origin, obb1);
        }
        if (const auto a_rigidbody2 = GetRigidBody(ref)) {
            BoundingBox::GetOBB(a_rigidbody2, obb2);
        } else {
            BoundingBox::GetOBB(ref, obb2);
        }

        #ifndef NDEBUG
        if (UI::draw_debug) {
        }
        #endif

        if (!AreClose(obb1, obb2, proximity)) {
            return false;
        }

        #ifndef NDEBUG
        if (UI::draw_debug) {
            DebugAPI_IMPL::DrawDebug::DrawOBB(obb2);
            DebugAPI_IMPL::DrawDebug::DrawOBB(obb1);

            DebugAPI_IMPL::DrawDebug::draw_line(WorldObject::GetPosition(ref),
                                                WorldObject::GetPosition(RE::PlayerCharacter::GetSingleton()), 3.f,
                                                RE::NiColorA(0.f, 0.f, 1.f, 1.f));

            const RE::NiPoint3 c1{obb1.Center.x, obb1.Center.y, obb1.Center.z};
            const RE::NiPoint3 c2{obb2.Center.x, obb2.Center.y, obb2.Center.z};
            // yellow debug line between centers
            DebugAPI_IMPL::DrawDebug::draw_line(c1, c2, 2, RE::NiColorA(1.f, 1.f, 0.f, 1.f));
        }
        #endif

        return true;
    }
};

FormID Source::SearchNearbyModulatorsCached(const RE::TESObjectREFR* a_obj, const std::vector<FormID>& candidates) {
    if (!a_obj || candidates.empty()) {
        return 0;
    }

    const auto cache = CellScanner::GetSingleton()->GetCache();
    if (!cache || cache->byBase.empty()) {
        return 0;
    }

    const auto originPos = Utils::WorldObject::GetPosition(a_obj);

    const float r = Settings::search_radius;
    const float r2 = (r > 0.0f) ? (r * r) : std::numeric_limits<float>::infinity();

    // Respect candidate ordering (unlike your current unordered_set path).
    for (const auto baseID : candidates) {
        const auto it = cache->byBase.find(baseID);
        if (it == cache->byBase.end()) {
            continue;
        }

        // For this baseID, try the closest refs first (without sorting):
        // we scan all within r2 and keep the best hit that passes the OBB check.
        float bestD2 = r2;
        bool found = false;

        for (const auto& e : it->second) {
            const float dx = e.pos.x - originPos.x;
            const float dy = e.pos.y - originPos.y;
            const float dz = e.pos.z - originPos.z;
            const float d2 = dx * dx + dy * dy + dz * dz;

            if (d2 > bestD2) {
                continue;
            }

            const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(e.refid);
            if (!ref || ref->IsDisabled() || ref->IsDeleted() || ref->IsMarkedForDeletion()) {
                continue;
            }

            if (SearchModulatorInCell_Sub(a_obj, ref)) {
                bestD2 = d2;
                found = true;
            }
        }

        if (found) {
            return baseID;
        }
    }

    return 0;
}