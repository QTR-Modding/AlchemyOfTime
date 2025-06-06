#include "Data.h"
#include "DrawDebug.h"

void Source::Init(const DefaultSettings* defaultsettings) {

	if (!defaultsettings) {
		logger::error("Default settings is null.");
		InitFailed();
		return;
	}

    const RE::TESForm* form = GetFormByID(formid, editorid);
    if (const auto bound_ = GetBoundObject(); !form || !bound_) {
        logger::error("Form not found.");
        InitFailed();
        return;
    }

    formid = form->GetFormID();
    editorid = clib_util::editorID::get_editorID(form);
        
    if (!formid || editorid.empty()) {
		logger::error("Editorid is empty.");
		InitFailed();
		return;
	}

    if (!Settings::IsItem(formid, "",true)) {
        logger::error("Form is not a suitable item.");
        InitFailed();
        return;
    }

    qFormType = Settings::GetQFormType(formid);
    if (qFormType.empty()) {
        logger::error("Formtype is not one of the predefined types.");
        InitFailed();
        return;
    }

	// get settings
	settings = *defaultsettings;
    // put addons
	if (auto* addon = Settings::GetAddOnSettings(form); addon && addon->IsHealthy()) {
		settings.Add(*addon);
	}

    if (!settings.CheckIntegrity()) {
        logger::critical("Default settings integrity check failed.");
		InitFailed();
		return;
	}

    formtype = form->GetFormType();

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
    }
    else if (qFormType == "INGR") GatherStages<RE::IngredientItem>();
    else if (qFormType == "MEDC" || qFormType == "POSN") GatherStages<RE::AlchemyItem>();
	else if (qFormType == "ARMO") GatherStages<RE::TESObjectARMO>();
	else if (qFormType == "WEAP") GatherStages<RE::TESObjectWEAP>();
	else if (qFormType == "SCRL") GatherStages<RE::ScrollItem>();
	else if (qFormType == "BOOK") GatherStages<RE::TESObjectBOOK>();
	else if (qFormType == "SLGM") GatherStages<RE::TESSoulGem>();
	else if (qFormType == "MISC") GatherStages<RE::TESObjectMISC>();
	else if (qFormType == "NPC") GatherStages<RE::TESNPC>();
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
        if (!temp_stage.CheckIntegrity()) {
            logger::critical("Transformed stage integrity check failed.");
            InitFailed();
            return;
        }
        transformed_stages[key] = temp_stage;
    }

    if (!CheckIntegrity()) {
        logger::critical("CheckIntegrity failed");
        InitFailed();
		return;
    }
}

std::string_view Source::GetName() const {
    if (const auto form = GetFormByID(formid, editorid)) return form->GetName();
    return "";
}

void Source::UpdateAddons()
{
	const auto* form = GetFormByID(formid, editorid);
	if (!form) {
		logger::error("UpdateAddons: Form not found.");
		return;
	}
	if (auto* addon = Settings::GetAddOnSettings(form); addon && addon->IsHealthy()) {
		settings.Add(*addon);
	}

    if (!settings.CheckIntegrity()) {
        logger::critical("Default settings integrity check failed.");
		InitFailed();
		return;
	}
}

std::unordered_map<RefID, std::vector<StageUpdate>> Source::UpdateAllStages(
    const std::vector<RefID>& filter, const float time) {
    if (init_failed) {
        logger::critical("UpdateAllStages: Initialisation failed.");
        return {};
    }
    // save the updated instances
    std::unordered_map<RefID, std::vector<StageUpdate>> updated_instances;
    if (data.empty()) {
		logger::warn("No data found for source {}", editorid);
		return updated_instances;
	}

    for (auto& reffid : filter) {
        if (!data.contains(reffid)) {
			logger::warn("Refid {} not found in data.", reffid);
			continue;
		}
        for (auto& instances = data.at(reffid); auto& instance : instances) {
			const Stage* old_stage = IsStageNo(instance.no) ? &GetStage(instance.no) : nullptr;
            if (UpdateStageInstance(instance, time)) {
                Stage* new_stage = nullptr;
                if (instance.xtra.is_transforming) {
                    instance.xtra.is_decayed = true;
                    instance.xtra.is_fake = false;
                    const auto temp_formid = instance.GetDelayerFormID();
                    if (!transformed_stages.contains(temp_formid)) {
						logger::error("Transformed stage not found.");
						continue;
					}
                    new_stage = &transformed_stages[temp_formid];
					instance.xtra.is_transforming = false;
                }
                else if (instance.xtra.is_decayed || !IsStageNo(instance.no)) {
                    new_stage = &decayed_stage;
                }
                auto is_fake_ = IsFakeStage(instance.no);
                if (!new_stage) {
                    updated_instances[reffid].emplace_back(old_stage, &GetStage(instance.no), instance.count,
                                                            instance.start_time, is_fake_);
                }
                else {
                    updated_instances[reffid].emplace_back(old_stage, new_stage, instance.count, instance.start_time ,
                                                           is_fake_);
                }
            }
        }
    }
    return updated_instances;
}

bool Source::IsStage(const FormID some_formid) {
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

StageNo Source::GetStageNo(const FormID formid_) {
    for (auto& [key, value] : stages) {
        if (value.formid == formid_) return key;
    }
    return 0;
}

const Stage& Source::GetStage(const StageNo no)
{   
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

const Stage* Source::GetStageSafe(const StageNo no) const
{
	if (stages.contains(no)) return &stages.at(no);
    return nullptr;
}

Duration Source::GetStageDuration(const StageNo no) const {
    if (!stages.contains(no)) return 0;
    return stages.at(no).duration;
}

std::string Source::GetStageName(const StageNo no) const {
	if (stages.contains(no)) return stages.at(no).name;
	return "";
}

StageInstance* Source::InsertNewInstance(const StageInstance& stage_instance, const RefID loc)
{
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
    data[loc].push_back(stage_instance);

    // fillout the xtra of the emplaced instance
    // get the emplaced instance
    /*auto& emplaced_instance = data.back();
    emplaced_instance.xtra.form_id = stages[n].formid;
    emplaced_instance.xtra.editor_id = clib_util::editorID::get_editorID(stages[n].GetBound());
    emplaced_instance.xtra.crafting_allowed = stages[n].crafting_allowed;
    if (IsFakeStage(n)) emplaced_instance.xtra.is_fake = true;*/

	return &data[loc].back();
}

StageInstance* Source::InitInsertInstanceWO(StageNo n, const Count c, const RefID l, const Duration t_0)
{
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

    return InsertNewInstance(new_instance,l);
}

bool Source::InitInsertInstanceInventory(const StageNo n, const Count c, RE::TESObjectREFR* inventory_owner, const Duration t_0) {
    if (!inventory_owner) {
        logger::error("Inventory owner is null.");
        return false;
    }
    const RefID inventory_owner_refid = inventory_owner->GetFormID();
    if (!inventory_owner->HasContainer() && inventory_owner_refid != player_refid) {
        logger::error("Inventory owner is not a container.");
		return false;
    }
        
    // isme takilma
    if (!InitInsertInstanceWO(n, c, inventory_owner_refid, t_0)) {
		logger::error("InitInsertInstance failed.");
		return false;
	}
        
    SetDelayOfInstance(data[inventory_owner_refid].back(), t_0, inventory_owner);
    return true;
}

bool Source::MoveInstance(const RefID from_ref, const RefID to_ref, const StageInstance* st_inst) {
    // Check if the from_ref exists in the data map
    if (!data.contains(from_ref)) {
        return false;
    }

    // Get the vector of instances from the from_ref key
    auto& from_instances = data.at(from_ref);
    const StageInstance new_instance(*st_inst);

    // Find the instance in the from_instances vector
    const auto it = std::ranges::find(from_instances, *st_inst);
    if (it == from_instances.end()) {
        return false;
    }

    // Remove the instance from the from_instances vector
    from_instances.erase(it);

    // Add the instance to the to_ref key vector
    if (to_ref > 0) data[to_ref].push_back(new_instance);

    return true;
}

Count Source::MoveInstances(const RefID from_ref, const RefID to_ref, const FormID instance_formid, Count count, const bool older_first)
{
    // older_first: true to move older instances first
    if (data.empty()) {
		logger::warn("No data found for source {}", editorid);
		return 0;
	}
    if (init_failed) {
		logger::critical("MoveInstances: Initialisation failed.");
		return 0;
	}
    if (count <= 0) {
        logger::error("Count is less than or equal 0.");
        return 0;
    }
    if (!instance_formid) {
		logger::error("Instance formid is 0.");
		return 0;
	}
    if (from_ref == to_ref) {
		logger::error("From and to refs are the same.");
		return 0;
	}

    if (!data.contains(from_ref)) {
		logger::error("From refid not found in data.");
        return count;
	}

    std::vector<size_t> instances_candidates = {};
    size_t index_ = 0;
    for (const auto& st_inst : data.at(from_ref)) {
        if (st_inst.xtra.form_id == instance_formid) {
            instances_candidates.push_back(index_);
        }
        index_++;
    }

    if (instances_candidates.empty()) {
        logger::warn("No instances found for formid {} and location {}", instance_formid, from_ref);
        return 0;
    }

    const auto curr_time = RE::Calendar::GetSingleton()->GetHoursPassed();
    if (older_first) {
        std::ranges::sort(instances_candidates,
                          [this, from_ref, curr_time](const size_t a, const size_t b) {
                              return this->data[from_ref][a].GetElapsed(curr_time) >
                                     this->data[from_ref][b].GetElapsed(curr_time);  // move the older stuff
                          });
    } 
    else {
        std::ranges::sort(instances_candidates,
                          [this, from_ref, curr_time](const size_t a, const size_t b) {
                              return this->data[from_ref][a].GetElapsed(curr_time) <
                                     this->data[from_ref][b].GetElapsed(curr_time);  // move the newer stuff
                          });
	}

    std::vector<size_t> removed_indices;
    for (size_t index: instances_candidates) {
        if (!count) break;
            
        StageInstance* instance;
        if (removed_indices.empty()) {
            instance = &data[from_ref][index];
        } else {
            int shift = 0;
            for (const size_t removed_index : removed_indices) {
                if (index == removed_index) {
                    logger::critical("Index is equal to removed index.");
                    return count;
                }
                if (index > removed_index) shift++;
            }
            instance = &data[from_ref][index - shift];
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
        } 
        else {
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

inline bool Source::IsTimeModulator(const FormID _form_id) const {
    if (!_form_id) return false;
    for (const auto& instances : data | std::views::values) {
		for (const auto& instance : instances) {
			if (instance.GetDelayerFormID() == _form_id) return true;
		}
	}
    return false;
}

bool Source::IsDecayedItem(const FormID _form_id) const {
    // if it is one of the transformations counts as decayed
	if (decayed_stage.formid == _form_id) return true;
	return std::ranges::any_of(settings.transformers | std::views::values,
                               [&](const auto& trns_tpl) {
                                   return std::get<0>(trns_tpl) == _form_id;
                               });
}

inline FormID Source::GetModulatorInInventory(RE::TESObjectREFR* inventory_owner) const {
    const auto inventory_owner_base_id = inventory_owner->GetBaseObject()->GetFormID();
    const auto inventory = inventory_owner->GetInventory();
    for (const auto& dlyr_fid : settings.delayers_order) {
        if (const auto entry = inventory.find(RE::TESForm::LookupByID<RE::TESBoundObject>(dlyr_fid));
            entry != inventory.end() && entry->second.first > 0) {
			if (!settings.delayer_containers.contains(dlyr_fid) ||
                settings.delayer_containers.at(dlyr_fid).empty()) {
				return dlyr_fid;
			}
			if (settings.delayer_containers.at(dlyr_fid).contains(inventory_owner_base_id)) {
				return dlyr_fid;
            }
        }
    }
    return 0;
}

inline FormID Source::GetModulatorInWorld(const RE::TESObjectREFR* wo) const
{
	// idea: scan proximity for the modulators
	const auto& candidates = settings.delayers_order;
    return SearchNearbyModulators(wo,candidates);
}

inline FormID Source::GetTransformerInInventory(RE::TESObjectREFR* inventory_owner) const {
	const auto inventory_owner_base_id = inventory_owner->GetBaseObject()->GetFormID();
    const auto inventory = inventory_owner->GetInventory();
	for (const auto& trns_fid : settings.transformers_order) {
		if (const auto entry = inventory.find(RE::TESForm::LookupByID<RE::TESBoundObject>(trns_fid));
			entry != inventory.end() && entry->second.first > 0) {
			if (!settings.transformer_containers.contains(trns_fid) ||
				settings.transformer_containers.at(trns_fid).empty()) {
				return trns_fid;
			}
			if (settings.transformer_containers.at(trns_fid).contains(inventory_owner_base_id)) {
				return trns_fid;
			}
		}
	}
	return 0;
}

inline FormID Source::GetTransformerInWorld(const RE::TESObjectREFR* wo) const
{
	const auto& candidates = settings.transformers_order;
    return SearchNearbyModulators(wo,candidates);
}

void Source::UpdateTimeModulationInInventory(RE::TESObjectREFR* inventory_owner, const float _time)
{
    if (!inventory_owner) {
        logger::error("Inventory owner is null.");
        return;
    }

    const RefID inventory_owner_refid = inventory_owner->GetFormID();
    if (!inventory_owner_refid) {
        logger::error("Inventory owner refid is 0.");
        return;
    }

    if (!inventory_owner->HasContainer()) {
        logger::error("Inventory owner does not have a container.");
        return;
    }

    if (!data.contains(inventory_owner_refid)) {
		//logger::error("Inventory owner refid not found in data: {} and source {}.", inventory_owner_refid, editorid);
		return;
	}

    if (data.at(inventory_owner_refid).empty()) {
        return;
    }

    SetDelayOfInstances(_time, inventory_owner);
}

void Source::UpdateTimeModulationInWorld(RE::TESObjectREFR* wo, StageInstance& wo_inst, const float _time) const
{
    SetDelayOfInstance(wo_inst, _time, wo, false);
}

float Source::GetNextUpdateTime(StageInstance* st_inst) {
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

    const auto schranke = delay_slope > 0 ? GetStage(st_inst->no).duration : 0.f;

    return st_inst->GetHittingTime(schranke);
}

void Source::CleanUpData()
{
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
	
    const auto curr_time = RE::Calendar::GetSingleton()->GetHoursPassed();
    for (auto& instances : data | std::views::values) {
        if (instances.empty()) continue;
        if (instances.size() > 1) {
            for (auto it = instances.begin(); it+1 != instances.end(); ++it) {
                size_t ind = 1;
                for (auto it2 = it + ind; it2 != instances.end(); it2 = it + ind) {
					++ind;
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
            const bool should_erase = 
                (it->count <= 0) ||
                (it->start_time > curr_time) ||
                (it->xtra.is_decayed || !IsStageNo(it->no));

            const auto decay_time = should_erase ? 0.f : GetDecayTime(*it);
            if (should_erase || 
                decay_time > 0.f && curr_time-decay_time > static_cast<float>(Settings::nForgettingTime)) {
                it = instances.erase(it);
            }
            else ++it;
		}
    }
        
    for (auto it = data.begin(); it != data.end();) {
        if (it->second.empty()) it = data.erase(it);
        else ++it;
    }
}

void Source::PrintData()
{

    if (init_failed) {
		logger::critical("PrintData: Initialisation failed.");
		return;
	}
    int n_print=0;
    logger::info("Printing data for source -{}-", editorid);
	for (auto& [loc,instances] : data) {
        if (data[loc].empty()) continue;
        logger::info("Location: {}", loc);
        for (auto& instance : instances) {
            logger::info("No: {}, Count: {}, Start time: {}, Stage Duration {}, Delay Mag {}, Delayer {}, isfake {}, istransforming {}, isdecayed {}",
                instance.no, instance.count, instance.start_time, GetStage(instance.no).duration, instance.GetDelayMagnitude(), instance.GetDelayerFormID(), instance.xtra.is_fake, instance.xtra.is_transforming, instance.xtra.is_decayed);
            if (n_print>200) {
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

void Source::Reset()
{
    formid = 0;
	editorid = "";
	stages.clear();
	data.clear();
	init_failed = false;
}

bool Source::UpdateStageInstance(StageInstance& st_inst, const float curr_time) {
    if (st_inst.xtra.is_decayed) return false;  // decayed
    if (st_inst.xtra.is_transforming) {
        if (const auto transformer_form_id = st_inst.GetDelayerFormID(); !settings.transformers.contains(transformer_form_id)) {
			logger::error("Transformer Formid {} not found in default settings.", transformer_form_id);
            st_inst.RemoveTransform(curr_time);
		}
        else {
            const auto& transform_properties = settings.transformers[transformer_form_id];
            const auto trnsfrm_duration = std::get<1>(transform_properties);
            if (const auto trnsfrm_elapsed = st_inst.GetTransformElapsed(curr_time); trnsfrm_elapsed >= trnsfrm_duration) {
                const auto& transformed_stage = transformed_stages[transformer_form_id];
                st_inst.xtra.form_id = transformed_stage.formid;
                st_inst.SetNewStart(curr_time, trnsfrm_elapsed - trnsfrm_duration);
                return true;
            }
            return false;
        }

    } 
    else if (GetNStages() < 2 && GetFinalStage().formid == st_inst.xtra.form_id) {
        st_inst.SetNewStart(curr_time, 0);
		return false;
	}
    if (!IsStageNo(st_inst.no)) {
		return false;
	}
    if (st_inst.count <= 0) {
        return false;
    }
    float diff = st_inst.GetElapsed(curr_time);
    bool updated = false;
        
    while (diff < 0) {
        if (st_inst.no > 0) {
            if (!IsStageNo(st_inst.no - 1)) {
                logger::critical("Stage {} does not exist.", st_inst.no - 1);
                return false;
			}
            st_inst.no--;
            diff += GetStage(st_inst.no).duration;
            updated = true;
        } else {
            diff = 0;
            break;
        }
    }
    while (diff >= GetStage(st_inst.no).duration) {
        diff -= GetStage(st_inst.no).duration;
		st_inst.no++;
        updated = true;
        if (!IsStageNo(st_inst.no)) {
            st_inst.xtra.is_decayed= true;
            st_inst.xtra.form_id = decayed_stage.formid;
            st_inst.xtra.editor_id = clib_util::editorID::get_editorID(decayed_stage.GetBound());
            st_inst.xtra.is_fake = false;
            st_inst.xtra.crafting_allowed = false;
            break;
		}
	}
    if (updated) {
        if (st_inst.xtra.is_decayed) {
            st_inst.xtra.form_id = decayed_stage.formid;
            st_inst.xtra.editor_id = clib_util::editorID::get_editorID(decayed_stage.GetBound());
            st_inst.xtra.is_fake = false;
            st_inst.xtra.crafting_allowed = false;
        } 
        else {
            st_inst.xtra.form_id = GetStage(st_inst.no).formid;
            st_inst.xtra.editor_id = clib_util::editorID::get_editorID(GetStage(st_inst.no).GetBound());
            st_inst.xtra.is_fake = IsFakeStage(st_inst.no);
            st_inst.xtra.crafting_allowed = GetStage(st_inst.no).crafting_allowed;
        }
        // as long as the delay start was before the ueberschreitung time this will work,
        // the delay start cant be strictly after the ueberschreitung time bcs we call update when a new delay
        // starts so the delay start will always be before the ueberschreitung time
        st_inst.SetNewStart(curr_time,diff);
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
    return dcyd_st;
}

Stage Source::GetTransformedStage(const FormID key_formid) const {
    Stage trnsf_st;
	if (!settings.transformers.contains(key_formid)) {
		logger::error("Transformer Formid {} not found in settings.", key_formid);
		return trnsf_st;
    }
    const auto& trnsf_props = settings.transformers.at(key_formid);
    trnsf_st.formid = std::get<0>(trnsf_props);
    trnsf_st.duration = 0.1f; // just to avoid error in checkintegrity
    return trnsf_st;
}

void Source::SetDelayOfInstances(const float some_time, RE::TESObjectREFR* inventory_owner)
{
    const RefID loc = inventory_owner->GetFormID();
    if (!data.contains(loc)) {
        logger::error("Location {} does not exist.", loc);
        return;
    }
    if (ShouldFreezeEvolution(inventory_owner->GetBaseObject()->GetFormID())) {
		for (auto& instance : data.at(loc)) {
	        if (instance.count <= 0) continue;
			instance.RemoveTimeMod(some_time);
			instance.SetDelay(some_time, 0, 0); // freeze
        }
        return;
    }

    const auto transformer_best = GetTransformerInInventory(inventory_owner);
    const auto delayer_best = GetModulatorInInventory(inventory_owner);
    std::vector<StageNo> allowed_stages;
	if (transformer_best && settings.transformers.contains(transformer_best)) {
        allowed_stages = std::get<2>(settings.transformers[transformer_best]);
	}

    for (auto& instance : data.at(loc)) {
		if (instance.count <= 0) continue;
		SetDelayOfInstance(instance, some_time, transformer_best, delayer_best, allowed_stages);
	}
}

void Source::SetDelayOfInstance(StageInstance& instance, const float curr_time, RE::TESObjectREFR* a_object, const bool inventory_owner) const {
    if (instance.count <= 0) return;
    if (ShouldFreezeEvolution(a_object->GetBaseObject()->GetFormID())) {
		instance.RemoveTimeMod(curr_time);
		instance.SetDelay(curr_time, 0, 0); // freeze
        return;
    }
	const auto transformer_best = inventory_owner ? GetTransformerInInventory(a_object) : GetTransformerInWorld(a_object);
	const auto delayer_best = inventory_owner ? GetModulatorInInventory(a_object) : GetModulatorInWorld(a_object);
    std::vector<StageNo> allowed_stages;
	if (transformer_best && settings.transformers.contains(transformer_best)) {
        allowed_stages = std::get<2>(settings.transformers.at(transformer_best));
	}
	SetDelayOfInstance(instance, curr_time, transformer_best, delayer_best, allowed_stages);
}

void Source::SetDelayOfInstance(StageInstance& instance, const float a_time, const FormID a_transformer, const FormID a_delayer, const std::vector<
                                StageNo>& allowed_stages) const
{
	if (!a_transformer || !Vector::HasElement<StageNo>(allowed_stages, instance.no)) instance.RemoveTransform(a_time);
	else return instance.SetTransform(a_time, a_transformer);

	const float delay_ = !a_delayer ? 1 : settings.delayers.contains(a_delayer) ? settings.delayers.at(a_delayer) : 1;
    instance.SetDelay(a_time, delay_, a_delayer);
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
		logger::error("Formid {} does not exist.", formid);
		return false;
	}

	if (!settings.CheckIntegrity()) {
        logger::error("Default settings integrity check failed.");
        return false;
    }

    std::set<StageNo> st_numbers_check;
    for (auto& [st_no,stage_tmp]: stages) {
            
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

    for (const auto& stage: stages | std::views::values) {
        if (!stage.CheckIntegrity()) {
			logger::error("Stage integrity check failed for stage no {} and source {} {}", stage.no,formid,editorid);
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
    if (const auto slope = st_inst.GetDelaySlope(); slope <= 0) return -1;
    StageNo curr_stageno = st_inst.no;
    if (!IsStageNo(curr_stageno)) {
        logger::error("Stage {} does not exist.", curr_stageno);
        return true;
    }
    const auto last_stage_no = GetLastStageNo();
    float total_duration = 0;
    while (curr_stageno <= last_stage_no) {
        total_duration += GetStage(curr_stageno).duration;
        curr_stageno+=1;
	}
    return st_inst.GetHittingTime(total_duration);
}

inline void Source::InitFailed()
{
    logger::error("Initialisation failed for formid {}.",formid);
    Reset();
    init_failed = true;
}

void Source::RegisterStage(const FormID stage_formid, const StageNo stage_no)
{
    for (const auto& value : stages | std::views::values) {
        if (stage_formid == value.formid) {
            logger::error("stage_formid is already in the stages.");
            return;
        }
    }
    if (stage_formid == formid && stage_no != 0) {
        // not allowed. if you want to go back to beginning use decayed stage
        logger::error("Formid of non initial stage is equal to source formid.");
        return;
    }

    if (!stage_formid) {
        logger::error("Could not create copy form for stage {}", stage_no);
        return;
    }

    if (const auto stage_form = GetFormByID(stage_formid); !stage_form) {
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
}

FormID Source::FetchFake(const StageNo st_no) {
    if (!GetBoundObject()) {
        logger::error("Could not get bound object", formid);
        return 0;
    }
    if (editorid.empty()) {
        logger::error("Editorid is empty.");
        return 0;
    }
    if (!Vector::HasElement(Settings::fakes_allowedQFORMS, qFormType)) {
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

FormID Source::SearchNearbyModulators(const RE::TESObjectREFR* a_obj, const std::vector<FormID>& candidates) {
    const auto cell = a_obj->GetParentCell();
	if (!cell) {
		logger::error("WO and Player cell are null.");
		return 0;
	}
	FormID result = 0;
	const auto candidates_set = std::unordered_set(candidates.begin(), candidates.end());
    if (cell->IsInteriorCell()) {
        SearchModulatorInCell(result, a_obj, cell, candidates_set, Settings::search_radius);
		return result;
    }
    SearchModulatorInCell(result, a_obj, cell, candidates_set, Settings::search_radius);
	if (result) return result;

	// now search in the adjacent cells
	for (const auto& worldCell : cell->GetRuntimeData().worldSpace->cellMap) {
		if (!worldCell.second) continue;
		if (worldCell.second->IsInteriorCell()) continue;
		if (!AreAdjacentCells(cell,worldCell.second)) continue;
		SearchModulatorInCell(result, a_obj, worldCell.second, candidates_set, Settings::search_radius);
		if (result) return result;
	}

    // now search in skycell
	if (const auto worldspace = a_obj->GetWorldspace()) {
		if (const auto skycell = worldspace->GetSkyCell()) {
			SearchModulatorInCell(result, a_obj, skycell, candidates_set, Settings::search_radius);
		}
	}
	return result;
}

namespace {
    bool SearchModulatorInCell_Sub(const RE::TESObjectREFR* a_origin, const RE::TESObjectREFR* ref, const float proximity=Settings::proximity_range) {
#ifndef NDEBUG
        draw_line(WorldObject::GetPosition(ref), WorldObject::GetPosition(RE::PlayerCharacter::GetSingleton()),3.f, glm::vec4(0.f, 0.f, 1.f, 1.f));
        WorldObject::DrawBoundingBox(ref);
#endif
        if (!WorldObject::AreClose(a_origin, ref, proximity)) {
	        return false;
        }
#ifndef NDEBUG
        logger::info("Found modulator in proximity: {}", clib_util::editorID::get_editorID(ref->GetBaseObject()));
#endif
        return true;
    }
};

void Source::SearchModulatorInCell(FormID& result, const RE::TESObjectREFR* a_origin,
                                   const RE::TESObjectCELL* a_cell, const std::unordered_set<FormID>& modulators, const float range) {

    auto callback = [&a_origin,&result, &modulators](const RE::TESObjectREFR* ref)-> RE::BSContainer::ForEachResult {
        if (!ref || ref->IsDisabled() || ref->IsDeleted() || ref->IsMarkedForDeletion()) return RE::BSContainer::ForEachResult::kContinue;
		const auto a_base = ref->GetObjectReference();
        if (const auto form_id = a_base->GetFormID(); modulators.contains(form_id) && SearchModulatorInCell_Sub(a_origin,ref)) {
			result = form_id;
	        return RE::BSContainer::ForEachResult::kStop;
        }
        return RE::BSContainer::ForEachResult::kContinue;
        };

    if (range>0) {
        a_cell->ForEachReferenceInRange(WorldObject::GetPosition(a_origin), range,callback);
    }
    else {
		a_cell->ForEachReference(callback);
    }
}
