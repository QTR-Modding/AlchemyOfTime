#include "Manager.h"
#include <unordered_set>

void Manager::WoUpdateLoop(const std::vector<RefID>& refs)
{
    for (auto& refid : refs) {
		if (const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid)) {
			{
                std::unique_lock lock(queueMutex_);
				if (!_ref_stops_.contains(refid)) {
					continue;
				}
				auto& val = _ref_stops_.at(refid);
				PreDeleteRefStop(val, ref->Get3D());
			    _ref_stops_.erase(refid);
			}
			Update(ref);
		}
    }
}

void Manager::PreDeleteRefStop(RefStop& a_ref_stop, RE::NiAVObject* a_obj)
{
    if (a_obj) a_ref_stop.RemoveTint(a_obj);
	a_ref_stop.RemoveArtObject();
	a_ref_stop.RemoveShader();
	a_ref_stop.RemoveSound();
}

void Manager::UpdateLoop()
{
	//std::unique_lock lock(queueMutex_);
    if (!Settings::world_objects_evolve.load()) {
	    std::unique_lock lock(queueMutex_);
        for (auto& [key,val] : _ref_stops_) {
            const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(key);
            PreDeleteRefStop(val, ref ? ref->Get3D() : nullptr);
        }
        _ref_stops_.clear();
    }
	else if (std::unique_lock lock(queueMutex_);
        !queue_delete_.empty() || !Settings::placed_objects_evolve.load()) {
	    for (auto it = _ref_stops_.begin(); it != _ref_stops_.end();) {
            if (const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(it->first); 
                queue_delete_.contains(it->first) ||
                ref && !Settings::placed_objects_evolve && WorldObject::IsPlacedObject(ref)) {
                PreDeleteRefStop(it->second,ref ? ref->Get3D() : nullptr);
	            it = _ref_stops_.erase(it);
            }
            else ++it;
	    }
		queue_delete_.clear();
    }


    if (_ref_stops_.empty()) {
        Stop();
	    std::unique_lock lock(queueMutex_);
        queue_delete_.clear();
        return;
    }

    if (const auto ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) return;

	std::vector<RefID> ref_stops_copy;
    for (
        auto lock = std::shared_lock(queueMutex_);
        const auto& key : _ref_stops_ | std::views::keys) {
        ref_stops_copy.push_back(key);
    } 

	// new mechanic: WO can also be affected by time modulators
	// Update _ref_stops_ with the new times
    for (const auto& key : ref_stops_copy) {
        if (const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(key)) {
            Update(ref);
        }
    }

    if (const auto cal = RE::Calendar::GetSingleton()) {
        // make copy with only stops
        const auto curr_time = cal->GetHoursPassed();
		std::vector<RefID> ref_stops_copy2;
		for (
            auto lock = std::shared_lock(queueMutex_);
            const auto& key : ref_stops_copy) {
			if (!_ref_stops_.contains(key)) continue;
            if (auto& val = _ref_stops_.at(key); val.IsDue(curr_time)) {
		        ref_stops_copy2.push_back(key);
            }
			else if (const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(key)) {
				if (const auto obj3d = ref->Get3D()) {
					val.ApplyTint(obj3d);
				}
				val.ApplyArtObject(ref);
				val.ApplyShader(ref);
				val.ApplySound();
			}
		}
        WoUpdateLoop(ref_stops_copy2);
    }
    Start();
}

void Manager::QueueWOUpdate(const RefStop& a_refstop)
{
    if (!Settings::world_objects_evolve.load()) return;
	const auto refid = a_refstop.ref_id;
    std::unique_lock lock(queueMutex_);
    if (_ref_stops_.contains(refid)) _ref_stops_.at(refid).Update(a_refstop);
    else _ref_stops_[refid] = a_refstop;
    Start();
}

void Manager::UpdateRefStop(Source& src, const StageInstance& wo_inst, RefStop& a_ref_stop, const float stop_t) {
	const auto wo_inst_delayer = wo_inst.GetDelayerFormID();
    // color
    const auto color = wo_inst.xtra.is_transforming ? src.settings.transformer_colors[wo_inst_delayer] : wo_inst_delayer  ? src.settings.delayer_colors[wo_inst_delayer ] : src.settings.colors[wo_inst.no];
	a_ref_stop.tint_color.id = color;
	// art object
	const auto art_object = wo_inst.xtra.is_transforming ? src.settings.transformer_artobjects[wo_inst_delayer] : wo_inst_delayer  ? src.settings.delayer_artobjects[wo_inst_delayer] : src.settings.artobjects[wo_inst.no];
	a_ref_stop.art_object.id = art_object;

	// effect shader
	const auto effect_shader = wo_inst.xtra.is_transforming ? src.settings.transformer_effect_shaders[wo_inst_delayer] : wo_inst_delayer ? src.settings.delayer_effect_shaders[wo_inst_delayer] : src.settings.effect_shaders[wo_inst.no];
	a_ref_stop.effect_shader.id = effect_shader;

	// sound
	const auto sound = wo_inst.xtra.is_transforming ? src.settings.transformer_sounds[wo_inst_delayer] : wo_inst_delayer ? src.settings.delayer_sounds[wo_inst_delayer] : src.settings.sounds[wo_inst.no];
	a_ref_stop.sound.id = sound;

    a_ref_stop.stop_time = stop_t;

}


unsigned int Manager::GetNInstances() {
    unsigned int n = 0;
    for (auto& src : sources) {
        for (const auto& loc : src.data | std::views::keys) {
            n += static_cast<unsigned int>(src.data[loc].size());
        }
    }
    return n;
}

Source* Manager::MakeSource(const FormID source_formid, const DefaultSettings* settings)
{
    if (!source_formid) return nullptr;
    if (IsDynamicFormID(source_formid)) return nullptr;
    // Source new_source(source_formid, "", empty_mgeff, settings);
    const Source new_source(source_formid, "", settings);
    if (!new_source.IsHealthy()) return nullptr;
    sources.push_back(new_source);
    return &sources.back();
}

void Manager::CleanUpSourceData(Source* src)
{
    if (!src) return;
    src->CleanUpData();
}

Source* Manager::GetSource(const FormID some_formid)
{
    // maybe it already exists
    for (auto& src : sources) {
        if (!src.IsHealthy()) continue;
        if (src.IsStage(some_formid)) return &src;
    }

    return nullptr;
}

Source* Manager::ForceGetSource(const FormID some_formid)
{
    if (!some_formid) return nullptr;

	if (const auto src = GetSource(some_formid)) return src;

    const auto some_form = GetFormByID(some_formid);
    if (!some_form) {
        logger::warn("Form not found.");
        return nullptr;
    }
    
    if (const auto* customSetting = Settings::GetCustomSetting(some_form)) {
        return MakeSource(some_formid, customSetting);
    }
	if (const auto* defaultSetting = Settings::GetDefaultSetting(some_formid)) {
        if (defaultSetting->durations.at(0)<=10000.f || Settings::GetAddOnSettings(some_form)) {
            return MakeSource(some_formid, defaultSetting);
        }
	}

    // stage item olarak dusunulduyse, custom a baslangic itemi olarak koymali
    return nullptr;

}

bool Manager::IsSource(const FormID some_formid)
{
    if (!some_formid) return false;
    const auto some_form = GetFormByID(some_formid);
    if (!some_form) {
        logger::warn("Form not found.");
        return false;
    }
	if (Settings::GetCustomSetting(some_form)) return true;
	if (Settings::GetDefaultSetting(some_formid)) return true;
    return false;
}

StageInstance* Manager::GetWOStageInstance(const RE::TESObjectREFR* wo_ref)
{
    if (sources.empty()) return nullptr;
	const auto wo_refid = wo_ref->GetFormID();
    for (auto& src : sources) {
        if (!src.data.contains(wo_refid)) continue;
        auto& instances = src.data.at(wo_refid);
        if (instances.size() == 1)
            return instances.data();
        if (instances.empty()) {
            logger::error("Stage instance found but empty.");
        } else if (instances.size() > 1) {
            logger::error("Multiple stage instances found.");
        }
    }
    logger::error("Stage instance not found.");
    return nullptr;
}

inline void Manager::ApplyStageInWorld_Fake(RE::TESObjectREFR* wo_ref, const char* xname)
{
    if (!xname) {
        logger::error("ExtraTextDisplayData is null.");
        return;
    }
    wo_ref->extraList.RemoveByType(RE::ExtraDataType::kTextDisplayData);
    const auto xText = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
    xText->SetName(xname);
    wo_ref->extraList.Add(xText);
}

void Manager::ApplyStageInWorld(RE::TESObjectREFR* wo_ref, const Stage& stage, RE::TESBoundObject* source_bound)
{
    if (!source_bound) {
        WorldObject::SwapObjects(wo_ref, stage.GetBound());
        wo_ref->extraList.RemoveByType(RE::ExtraDataType::kTextDisplayData);
    }
    else {
        WorldObject::SwapObjects(wo_ref, source_bound);
        ApplyStageInWorld_Fake(wo_ref, stage.GetExtraText());
    }
	//SKSE::GetTaskInterface()->AddTask([wo_ref]() {
	//	if (auto a_obj = wo_ref->Get3D()) {
	//		a_obj->art
	//	}
	//});
}

inline void Manager::ApplyEvolutionInInventoryX(RE::TESObjectREFR* inventory_owner, Count update_count, FormID old_item, FormID new_item)
{
    auto* old_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(old_item);
    if (!old_bound) {
        logger::error("Old item is null.");
        return;
    }

    const auto inventory = inventory_owner->GetInventory();
    const auto entry = inventory.find(old_bound);
    if (entry == inventory.end()) {
        logger::error("Item not found in inventory.");
        return;
    }
    if (!entry->second.second) {
        logger::error("Item data is null.");
        return;
    }
    if (entry->second.second->IsQuestObject()) {
        logger::warn("Item is a quest object.");
        return;
    }

    const bool has_xList = Inventory::EntryHasXData(entry->second.second.get());

    const auto inv_count = std::min(update_count, entry->second.first);

    const auto ref_handle = WorldObject::DropObjectIntoTheWorld(RE::TESForm::LookupByID<RE::TESBoundObject>(new_item), inv_count);
    if (has_xList) {
        if (!xData::UpdateExtras(entry->second.second->extraLists->front(), &ref_handle->extraList)) {
            logger::info("ExtraDataList not updated.");
        }
    } else logger::info("original ExtraDataList is null.");

    if (!WorldObject::PlayerPickUpObject(ref_handle, inv_count)) {
        logger::error("Item not picked up.");
        return;
    }

    RemoveItem(inventory_owner, old_item, inv_count);
}

inline void Manager::ApplyEvolutionInInventory_(RE::TESObjectREFR* inventory_owner, Count update_count, FormID old_item, FormID new_item)
{
    if (update_count <= 0) {
        logger::error("Update count is 0 or less {}.", update_count);
        return;
    }

    auto* old_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(old_item);
    if (!old_bound) {
        logger::error("Old item is null.");
        return;
    }
    const auto inventory = inventory_owner->GetInventory();
    const auto entry = inventory.find(old_bound);
    if (entry == inventory.end()) {
        logger::error("Item not found in inventory.");
        return;
    }
    const auto inv_count = entry->second.first;
    if (inv_count <= 0) {
        logger::warn("Item count in inventory is 0 or less {}.", inv_count);
        return;
    }
    if (!entry->second.second) {
        logger::error("Item data is null.");
        return;
    }
    if (entry->second.second->IsQuestObject()) {
        logger::warn("Item is a quest object.");
        return;
    }
    RemoveItem(inventory_owner, old_item, std::min(update_count, inv_count));
    AddItem(inventory_owner, nullptr, new_item, update_count);
}


void Manager::ApplyEvolutionInInventory(const std::string& _qformtype_, RE::TESObjectREFR* inventory_owner, const Count update_count, const FormID old_item, const FormID new_item)
{
    if (!inventory_owner){
		logger::error("Inventory owner is null.");
		return;
    }
    if (!old_item || !new_item){
		logger::error("Item is null.");
		return;
    }
    if (!update_count) {
        logger::warn("Update count is 0.");
        return;
    }
    if (old_item == new_item) {
        return;
    }

    bool is_faved = false;
    bool is_equipped = false;
    if (inventory_owner->IsPlayerRef()) {
        is_faved = IsPlayerFavorited(RE::TESForm::LookupByID<RE::TESBoundObject>(old_item));
        is_equipped = IsEquipped(RE::TESForm::LookupByID<RE::TESBoundObject>(old_item));
    }
    if (is_faved || is_equipped || Vector::HasElement<std::string>(Settings::xQFORMS, _qformtype_)) {
        ApplyEvolutionInInventoryX(inventory_owner, update_count, old_item, new_item);
    } else {
        ApplyEvolutionInInventory_(inventory_owner, update_count, old_item, new_item);
    }

    if (is_faved) FavoriteItem(RE::TESForm::LookupByID<RE::TESBoundObject>(new_item), inventory_owner);
    if (is_equipped) {
		listen_equip.store(false);
        EquipItem(RE::TESForm::LookupByID<RE::TESBoundObject>(new_item));
        listen_equip.store(true);
    }
}


inline void Manager::RemoveItem(RE::TESObjectREFR* moveFrom, const FormID item_id, const Count count)
{
	if (!moveFrom) {
		logger::warn("RemoveItem: moveFrom is null.");
		return;
	}
	if (count <= 0) {
		logger::warn("RemoveItem: Count is 0 or less.");
		return;
	}

	const auto inventory = moveFrom->GetInventory();
	if (const auto item = inventory.find(RE::TESForm::LookupByID<RE::TESBoundObject>(item_id)); item != inventory.end()) {
		if (item->second.second->IsQuestObject()) {
			logger::warn("Item is a quest object.");
			return;
		}
		if (item->second.first < count) {
			logger::warn("Item count is less than the count to remove.");
		}

		auto* bound = item->first;
		moveFrom->RemoveItem(bound, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
	}
}

void Manager::AddItem(RE::TESObjectREFR* addTo, RE::TESObjectREFR* addFrom, const FormID item_id, const Count count)
{
    if (!addTo) {
        logger::critical("add to is null!");
        return;
    }
    if (count <= 0) {
        logger::error("Count is 0 or less.");
        return;
    }
    if (addFrom) {
        if (addTo->GetFormID() == addFrom->GetFormID()) {
            logger::warn("Add to and add from are the same.");
            return;
        }
        if (!addFrom->HasContainer()) {
            logger::warn("Add from does not have a container.");
            return;
        }
    }

	if (auto* bound = RE::TESForm::LookupByID<RE::TESBoundObject>(item_id)) {
		addTo->AddObjectToContainer(bound, nullptr, count, addFrom);
	}
	else logger::critical("Bound is null.");
}


void Manager::Init()
{
    if (Settings::INI_settings.contains("Other Settings")) {
        if (Settings::INI_settings["Other Settings"].contains("bReset")) {
            should_reset = Settings::INI_settings["Other Settings"]["bReset"];
        } else logger::warn("bReset not found.");
    } else logger::critical("Other Settings not found.");

    _instance_limit = Settings::nMaxInstances;

    logger::info("Manager initialized with instance limit {}", _instance_limit);
}

std::set<float> Manager::GetUpdateTimes(const RE::TESObjectREFR* inventory_owner) {

    std::set<float> queued_updates;

	const auto inventory_owner_refid = inventory_owner->GetFormID();

    for (auto& src : sources) {
        if (!src.IsHealthy()) {
            logger::error("_UpdateTimeModulators: Source is not healthy.");
			continue;
        }
        if (!src.data.contains(inventory_owner_refid)) continue;

        for (auto& st_inst : src.data.at(inventory_owner_refid)) {
            if (st_inst.xtra.is_decayed || !src.IsStageNo(st_inst.no)) continue;
            if (const auto hitting_time = src.GetNextUpdateTime(&st_inst); hitting_time > 0) queued_updates.insert(hitting_time);
        }
    }

	return queued_updates;
}

bool Manager::UpdateInventory(RE::TESObjectREFR* ref, const float t)
{
    bool update_took_place = false;
    const auto refid = ref->GetFormID();

    for (size_t i = 0; i < sources.size(); ++i) {
        auto& src = sources[i];
        if (!src.IsHealthy()) continue;
        if (src.data.empty()) continue;
        if (!src.data.contains(refid)) continue;
        if (src.data.at(refid).empty()) continue;
        const auto updated_stages = src.UpdateAllStages({refid}, t);
		const auto& updates = updated_stages.contains(refid) ? updated_stages.at(refid) : std::vector<StageUpdate>();
		if (!update_took_place && !updates.empty()) update_took_place = true;
		CleanUpSourceData(&src);
#ifndef NDEBUG
		if (updates.empty()) {
			logger::trace("UpdateInventory: No updates for source formid {} editorid {}", src.formid, src.editorid);
        }
#endif // !NDEBUG
		for (const auto& update : updates) {
			ApplyEvolutionInInventory(src.qFormType, ref, update.count, update.oldstage->formid, update.newstage->formid);
			if (src.IsDecayedItem(update.newstage->formid)) {
                Register(update.newstage->formid, update.count, refid, t);
			}
		}
    }

    for (auto& src : sources) src.UpdateTimeModulationInInventory(ref, t);

    return update_took_place;
}

void Manager::UpdateInventory(RE::TESObjectREFR* ref)
{
    listen_container_change.store(false);

	SyncWithInventory(ref);
    
    // if there are time modulators which can also evolve, they need to be updated first
	const auto curr_time = RE::Calendar::GetSingleton()->GetHoursPassed();
    while (true){
		const auto times = GetUpdateTimes(ref);
		if (times.empty()) break;
        if (const auto t = *times.begin() + 0.000028f; t >= curr_time) break;
		else if(!UpdateInventory(ref, t)) {
			logger::warn("UpdateInventory: No updates for the time {}", t);
		    break;
		}
    }

	UpdateInventory(ref, curr_time);

	listen_container_change.store(true);
}

void Manager::SyncWithInventory(RE::TESObjectREFR* ref)
{

	const auto loc_refid = ref->GetFormID();
    const bool needHandling = locs_to_be_handled.contains(loc_refid);

    // handle discrepancies in inventory vs registries
    std::unordered_map<FormID, std::vector<StageInstance*>> formid_instances_map = {};
    std::unordered_map<FormID, Count> total_registry_counts = {};

    const auto loc_inventory = ref->GetInventory();

    formid_instances_map.reserve(loc_inventory.size());
	total_registry_counts.reserve(loc_inventory.size());

    for (auto& src : sources) {
        if (!src.data.contains(loc_refid)) continue;
        for (auto& st_inst : src.data.at(loc_refid)) {  // bu liste onceski savele ayni deil cunku source.datayi
                                                        // _registeratreceivedata deistirdi
            if (!st_inst.xtra.is_decayed && st_inst.count > 0) {
                auto& vecRef = formid_instances_map[st_inst.xtra.form_id];
                vecRef.push_back(&st_inst);
                auto& countRef = total_registry_counts[st_inst.xtra.form_id];
                countRef += st_inst.count;
            }
        }
    }


    if (needHandling) {
        for (const auto& [bound, entry] : loc_inventory) {
            if (bound->IsDynamicForm()) {
                const auto a_formID = bound->GetFormID();
                auto* name = bound->GetName();
                const auto nameLen = (name != nullptr) ? std::strlen(name) : 0;
                if (nameLen == 0) {
                    RemoveItem(ref, a_formID, std::max(1, entry.first));
                }
            }
        }
    }

    // for every formid, handle the discrepancies

	const auto current_time = RE::Calendar::GetSingleton()->GetHoursPassed();

    for (const auto& [bound, entry] : loc_inventory) {
		const auto formid = bound->GetFormID();
		const auto inventory_count = entry.first;
		if (!formid_instances_map.contains(formid)) {
			if (inventory_count > 0) {
				Register(formid, inventory_count, loc_refid, current_time);
			}
		}
        else {
            const auto total_registry_count = total_registry_counts[formid];
            if (auto diff = total_registry_count - inventory_count; diff < 0) {
			    Register(formid, -diff, loc_refid, current_time);
            }
            else if (diff > 0) {
                for (auto* instance : formid_instances_map.at(formid)) {
                    if (instance->xtra.is_fake && needHandling) {
                        AddItem(ref, nullptr, formid, diff);
                        break;
                    }
                    if (diff <= instance->count) {
                        instance->count -= diff;
                        break;
                    }
                    diff -= instance->count;
                    instance->count = 0;
                }
            }
        }
		formid_instances_map.erase(formid);
    }

    for (const auto& [formid,instances]:formid_instances_map) {
		for (auto* instance : instances) {
			if (instance->xtra.is_fake && needHandling) {
				AddItem(ref, nullptr, formid, instance->count);
			}
			else {
				instance->count = 0;
			}
		}
    }

	locs_to_be_handled.erase(loc_refid);
}

void Manager::UpdateWO(RE::TESObjectREFR* ref)
{
	HandleDynamicWO(ref);
    if (!Settings::world_objects_evolve.load()) return;
	if (ref->IsDeleted() || ref->IsDisabled() || ref->IsMarkedForDeletion()) return;
    if (ref->IsActivationBlocked()) return;
    if (!Settings::unowned_objects_evolve.load() && RE::PlayerCharacter::GetSingleton()->WouldBeStealing(ref)) return;

    const RefID refid = ref->GetFormID();
	const auto curr_time = RE::Calendar::GetSingleton()->GetHoursPassed();
	bool not_found = true;

    sources.reserve(sources.size()+1);
    for (size_t i = 0; i < sources.size(); ++i) {  // NOLINT(modernize-loop-convert)
        auto& src = sources[i];
        if (!src.IsHealthy()) continue;
        if (src.data.empty()) continue;
        if (!src.data.contains(refid)) continue;
        if (src.data.at(refid).empty()) continue;
        HandleWOBaseChange(ref);
        if (src.data.at(refid).data()->count <= 0) continue;

        not_found = false;

        if (const auto updated_stages = src.UpdateAllStages({refid}, curr_time); updated_stages.contains(refid)) {
			if (updated_stages.size() > 1) {
				logger::error("UpdateWO: Multiple updates for the same ref.");
			}
		    const auto& update = updated_stages.at(refid).front();
            const auto bound = src.IsFakeStage(update.newstage->no) ? src.GetBoundObject() : nullptr;
            ApplyStageInWorld(ref, *update.newstage, bound);
            if (src.IsDecayedItem(update.newstage->formid)) {
		        Register(update.newstage->formid, update.count, refid, update.update_time);
            }
        }

		//src = sources[i];
		if (!src.data.contains(refid)) logger::error("UpdateWO: Refid {} not found in source data.", refid);
        auto& wo_inst = src.data.at(refid).front();
        if (wo_inst.xtra.is_fake) ApplyStageInWorld(ref, src.GetStage(wo_inst.no), src.GetBoundObject());
        src.UpdateTimeModulationInWorld(ref,wo_inst,curr_time);
        if (const auto next_update = src.GetNextUpdateTime(&wo_inst); next_update > curr_time) {
			RefStop a_ref_stop(refid);
			UpdateRefStop(src, wo_inst, a_ref_stop, next_update);
            QueueWOUpdate(a_ref_stop);
        }
		CleanUpSourceData(&src);
		break;
    }

    sources.shrink_to_fit();
    if (not_found) Register(ref->GetBaseObject()->GetFormID(), ref->extraList.GetCount(), refid);
}

void Manager::UpdateRef(RE::TESObjectREFR* loc)
{
    if (loc->HasContainer()) {
        UpdateInventory(loc);
    }
	else UpdateWO(loc);

 //   for (auto& src : sources) {
	//	if (src.data.empty()) continue;
	//	CleanUpSourceData(&src);
	//}
}

RefStop* Manager::GetRefStop(const RefID refid)
{
	if (!_ref_stops_.contains(refid)) return nullptr;
	return &_ref_stops_.at(refid);
}

bool Manager::RefIsRegistered(const RefID refid) {
    if (!refid) {
        logger::warn("Refid is null.");
        return false;
    }
    std::shared_lock lock(sourceMutex_);
    if (sources.empty()) {
        logger::warn("Sources is empty.");
        return false;
    }
    for (auto& src : sources) {
        if (src.data.contains(refid) && !src.data.at(refid).empty()) return true;
    }
    return false;
}

void Manager::Register(const FormID some_formid, const Count count, const RefID location_refid, Duration register_time)
{
    if (do_not_register.contains(some_formid)) {
        logger::trace("Formid is in do not register list.");
        return;
    }
    if (!some_formid) {
        logger::warn("Formid is null.");
        return;
    }
    if (!count) {
        logger::warn("Count is 0.");
        return;
    }
    if (!location_refid) {
        logger::warn("Location refid is null.");
        return;
    }
    const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(location_refid);
    if (!ref) {
        logger::warn("Location ref is null. FormID: {:x}",some_formid);
        return;
	}
	if (Inventory::IsQuestItem(some_formid, ref)) {
		logger::trace("Formid is a quest item.");
		return;
	}
	if (!Settings::IsItem(some_formid, "", true)) {
		//logger::warn("Formid is an item.");
		return;
	}

    if (GetNInstances() > _instance_limit) {
        logger::warn("Instance limit reached.");
        MsgBoxesNotifs::InGame::CustomMsg(
            std::format("The mod is tracking over {} instances. It is advised to check your memory usage and "
                        "skse co-save sizes.",
                        _instance_limit));
    }
    
    if (register_time < EPSILON) register_time = RE::Calendar::GetSingleton()->GetHoursPassed();

    logger::trace("Registering new instance. Formid {} , Count {} , Location refid {}, register_time {}",
                    some_formid, count, location_refid, register_time);

    // make new registry
    Source* const src = ForceGetSource(some_formid);  // also saves it to sources if it was created new
    if (!src) {
        logger::trace("Register: Source is null.");
        do_not_register.insert(some_formid);
        return;
    }
    if (!src->IsStage(some_formid)) {
        logger::critical("Register: some_formid is not a stage.");
        do_not_register.insert(some_formid);
        return;
    }

    const auto stage_no = src->formid == some_formid ? 0 : src->GetStageNo(some_formid);

    if (ref->HasContainer()) {
        logger::trace("Registering inventory.");
        if (!src->InitInsertInstanceInventory(stage_no, count, ref, register_time)) {
            logger::error("Register: InsertNewInstance failed 1.");
            return;
        }
        
        // is this necessary?
        const auto stage_formid = src->GetStage(stage_no).formid;
        // to change from the source form to the stage form
        ApplyEvolutionInInventory(src->qFormType, ref, count, some_formid, stage_formid);
    } 
    else {
        logger::trace("Registering world object.");
        if (auto* inserted_instance = src->InitInsertInstanceWO(stage_no, count, location_refid, register_time); !inserted_instance) {
            logger::error("Register: InsertNewInstance failed 2.");
        }
        else {
            const auto bound = src->IsFakeStage(stage_no) ? src->GetBoundObject() : nullptr;
            ApplyStageInWorld(ref, src->GetStage(stage_no), bound);
		    // add to the queue
		    const auto hitting_time = src->GetNextUpdateTime(inserted_instance);
            RefStop a_ref_stop(location_refid);
            UpdateRefStop(*src,*inserted_instance,a_ref_stop, hitting_time);
			QueueWOUpdate(a_ref_stop);
        }
    }
}

void Manager::HandleCraftingEnter(unsigned int bench_type)
 {
    logger::trace("HandleCraftingEnter. bench_type: {}", bench_type);


    if (!handle_crafting_instances.empty()) {
        logger::warn("HandleCraftingEnter: Crafting instances already exist.");
        return;
    }

    if (!Settings::qform_bench_map.contains(bench_type)) {
        logger::warn("HandleCraftingEnter: Bench type not found.");
        return;
    }

    Update(player_ref);
	listen_container_change.store(false);

    const auto& q_form_types = Settings::qform_bench_map.at(bench_type);

    // trusting that the player will leave the crafting menu at some point and everything will be reverted


    std::map<FormID, int> to_remove;
    const auto player_inventory = player_ref->GetInventory();

    for (
        std::shared_lock lock(sourceMutex_);
        auto& src : sources) {
		if (!src.IsHealthy()) continue;
        if (!src.data.contains(player_refid)) continue;
        
        if (!Vector::HasElement<std::string>(q_form_types, src.qFormType)) {
            logger::trace("HandleCraftingEnter: qFormType mismatch: {} , {}", src.qFormType, bench_type);
            continue;
        }

        for (const auto& st_inst : src.data.at(player_refid)) {
            const auto stage_formid = st_inst.xtra.form_id;
            if (!stage_formid) {
                logger::error("HandleCraftingEnter: Stage formid is null!!!");
                continue;
            }

			if (st_inst.count <= 0 || st_inst.xtra.is_decayed) continue;
            if (Inventory::IsQuestItem(stage_formid, player_ref)) continue;
            if (stage_formid != src.formid && !st_inst.xtra.crafting_allowed) continue;

            if (const Types::FormFormID temp = {src.formid, stage_formid}; !handle_crafting_instances.contains(temp)) {
                const auto it = player_inventory.find(src.GetBoundObject());
                const auto count_src = it != player_inventory.end() ? it->second.first : 0;
                handle_crafting_instances[temp] = {st_inst.count, count_src};
            } else handle_crafting_instances.at(temp).first += st_inst.count;

            if (!faves_list.contains(stage_formid)) faves_list[stage_formid] = IsFavorited(stage_formid, player_refid);
            else if (!faves_list.at(stage_formid)) faves_list.at(stage_formid) = IsFavorited(stage_formid, player_refid);

            if (!equipped_list.contains(stage_formid)) equipped_list[stage_formid] = IsEquipped(stage_formid);
            else if (!equipped_list.at(stage_formid)) equipped_list.at(stage_formid) = IsEquipped(stage_formid);
        }
    }

    for (const auto& [formids, counts] : handle_crafting_instances) {
        if (formids.form_id1 == formids.form_id2) continue;
        RemoveItem(player_ref, formids.form_id2, counts.first);
        AddItem(player_ref, nullptr, formids.form_id1, counts.first);
        logger::trace("Crafting item updated in inventory.");
        logger::trace("HandleCraftingEnter: Formid1: {} , Formid2: {} , Count1: {} , Count2: {}", formids.form_id1,
                        formids.form_id2, counts.first, counts.second);
    }

	listen_container_change.store(true);

}

void Manager::HandleCraftingExit()
 {
    logger::trace("HandleCraftingExit");

    if (handle_crafting_instances.empty()) {
        logger::info("HandleCraftingExit: No instances found.");
        faves_list.clear();
        equipped_list.clear();
        return;
    }

    logger::trace("Crafting menu closed");

	listen_container_change.store(false);

    // need to figure out how many items were used up in crafting and how many were left
    const auto player_inventory = player_ref->GetInventory();
    for (auto& [formids, counts] : handle_crafting_instances) {
        if (formids.form_id1 == formids.form_id2) continue;

        logger::trace("HandleCraftingExit: Formid1: {} , Formid2: {} , Count1: {} , Count2: {}", formids.form_id1,
                        formids.form_id2, counts.first, counts.second);

        const auto it_src = player_inventory.find(GetFormByID<RE::TESBoundObject>(formids.form_id1));
        const auto actual_count_src = it_src != player_inventory.end() ? it_src->second.first : 0;

        if (const auto to_be_taken_back = actual_count_src - counts.second; to_be_taken_back > 0) {
            RemoveItem(player_ref, formids.form_id1, to_be_taken_back);
            AddItem(player_ref, nullptr, formids.form_id2, to_be_taken_back);
            if (faves_list[formids.form_id2]) FavoriteItem(formids.form_id2, player_refid);
            if (equipped_list[formids.form_id2]) EquipItem(formids.form_id2, false);
        }
    }

    handle_crafting_instances.clear();
    faves_list.clear();
    equipped_list.clear();

	listen_container_change.store(true);

	Update(player_ref);
}

void Manager::Update(RE::TESObjectREFR* from, RE::TESObjectREFR* to, const RE::TESForm* what, Count count)
{

    const bool to_is_world_object = to && !to->HasContainer();
    if (to_is_world_object) count = to->extraList.GetCount();

    if (from && to && !from->HasContainer()) {
        if (const auto temp_refid = from->GetFormID(); _ref_stops_.contains(temp_refid)) {
		    std::unique_lock lock(queueMutex_);
            queue_delete_.insert(temp_refid);
        }
    }

    if (RE::UI::GetSingleton()->IsMenuOpen(RE::BarterMenu::MENU_NAME)){
		if (from && from->IsPlayerRef()) to = nullptr;
		else if (to && to->IsPlayerRef()) from = nullptr;
    }

    if (!to && what && what->Is(RE::FormType::AlchemyItem)) count = 0;

    if (what && count > 0) {
		std::unique_lock lock(sourceMutex_);
        if (const auto src = GetSource(what->GetFormID())) {
			logger::trace("Update: Source found for {}.", what->GetName());
	        const auto from_refid = from ? from->GetFormID() : 0;
	        const auto to_refid = to ? to->GetFormID() : 0;
			auto what_formid = what->GetFormID();

	        if (src->data.contains(from_refid)) {
				logger::trace("Update: Moving {} instances from {} to {}.", count, from_refid, to_refid);
		        count = src->MoveInstances(from_refid, to_refid, what_formid, count, true);
			}

	        if (count > 0) Register(what_formid, count, to_refid);
	        CleanUpSourceData(src);

            if (to_is_world_object && src->data.contains(to_refid)) {
		        // need to break down the count of the item out in the world into the counts of the instances
		        bool handled_first = false;
				const bool is_player_owned = from ? from->IsPlayerRef() : false;
                for (auto& st_inst : src->data.at(to_refid)) {
			        const auto temp_count = st_inst.count;
                    if (!handled_first) {
                        if (to->extraList.GetCount() != temp_count) to->extraList.SetCount(static_cast<uint16_t>(temp_count));
                        if (is_player_owned) to->extraList.SetOwner(RE::TESForm::LookupByID(0x07));
				        handled_first = true;
                    }
                    else if (const auto new_ref = WorldObject::DropObjectIntoTheWorld(st_inst.GetBound(), temp_count, is_player_owned)) {
                        if (!src->MoveInstance(to_refid, new_ref->GetFormID(), &st_inst)){
							logger::error("Update: MoveInstance failed for form {} and loc {}.", what_formid, to_refid);
						}
                        else {
                            logger::trace("Update: Moved instance to new ref.");
							UpdateRef(new_ref);
                        }
                    }
                    else logger::error("Update: New ref is null.");
		        }
            }
		}
	}

    if (to) {
		std::unique_lock lock(sourceMutex_);
        UpdateRef(to);
    }
    if (from && (from->HasContainer() || !to)) {
		std::unique_lock lock(sourceMutex_);
		UpdateRef(from);
	}
}

void Manager::SwapWithStage(RE::TESObjectREFR* wo_ref)
{
    // registered olduunu varsayiyoruz
    logger::trace("SwapWithStage");
    if (!wo_ref) {
        logger::critical("Ref is null.");
        return;
    }
	std::shared_lock lock(sourceMutex_);
    const auto* st_inst = GetWOStageInstance(wo_ref);
	lock.unlock();
    if (!st_inst) {
        logger::warn("SwapWithStage: Source not found.");
        return;
    }
    WorldObject::SwapObjects(wo_ref, st_inst->GetBound(), false);
}

void Manager::Reset()
{
    logger::info("Resetting manager...");
	Stop();
	ClearWOUpdateQueue();
    {
	    std::unique_lock lock(sourceMutex_);
        for (auto& src : sources) src.Reset();
        sources.clear();
    }
    // external_favs.clear();         // we will update this in ReceiveData
    handle_crafting_instances.clear();
    faves_list.clear();
    equipped_list.clear();
    locs_to_be_handled.clear();
    Clear();
	listen_container_change.store(true);
	isUninstalled.store(false);
    
    logger::info("Manager reset.");
}

void Manager::HandleFormDelete(const FormID a_refid)
{
	std::unique_lock lock(sourceMutex_);
    for (auto& src : sources) {
        if (src.data.contains(a_refid)) {
            logger::warn("HandleFormDelete: Formid {}", a_refid);
            for (auto& st_inst : src.data.at(a_refid)) {
                st_inst.count = 0;
            }
        }
    }
}

void Manager::SendData()
{
    logger::info("--------Sending data---------");
    Print();
    Clear();

    int n_instances = 0;
	std::shared_lock lock(sourceMutex_);
    for (const auto& src : sources) {
		if (src.GetStageDuration(0) >= 10000.f) {
            if (src.settings.transformers_order.size()==0 && src.settings.delayers_order.size()==0) {
			    continue;
            }
		}
        for (auto& [loc, instances] : src.data) {
            if (instances.empty()) continue;
            const SaveDataLHS lhs{{src.formid, src.editorid}, loc};
            SaveDataRHS rhs;
            for (auto& st_inst : instances) {
                auto plain = st_inst.GetPlain();
                if (plain.is_fake) {
                    plain.is_faved = IsPlayerFavorited(st_inst.GetBound());
                    plain.is_equipped = IsEquipped(st_inst.GetBound());
                }
                rhs.push_back(plain);
                n_instances++;
            }
            if (!rhs.empty()) SetData(lhs, rhs);
        }
    }
    logger::info("Data sent. Number of instances: {}", n_instances);
};

void Manager::HandleLoc(RE::TESObjectREFR* loc_ref)
{
    if (!loc_ref) {
        logger::error("Loc ref is null.");
        return;
    }
    const auto loc_refid = loc_ref->GetFormID();

    if (!locs_to_be_handled.contains(loc_refid)) {
        logger::trace("Loc ref not in locs_to_be_handled.");
        return;
    }

    if (!loc_ref->HasContainer()) {
        logger::trace("Does not have container");
        // remove the loc refid key from locs_to_be_handled map
        if (const auto it = locs_to_be_handled.find(loc_refid); it != locs_to_be_handled.end()) {
            locs_to_be_handled.erase(it);
        }
        return;
    }

    for (const auto loc_inventory_temp = loc_ref->GetInventory(); const auto& [bound, entry] : loc_inventory_temp) {
        if (bound && IsDynamicFormID(bound->GetFormID()) &&
            std::strlen(bound->GetName()) == 0) {
            RemoveItem(loc_ref, bound->GetFormID(), std::max(1, entry.first));
        }
    }

    SyncWithInventory(loc_ref);

    if (const auto it = locs_to_be_handled.find(loc_refid); it != locs_to_be_handled.end()) {
        locs_to_be_handled.erase(it);
    }

    logger::trace("HandleLoc: synced with loc {}.", loc_refid);
}

StageInstance* Manager::RegisterAtReceiveData(const FormID source_formid, const RefID loc, const StageInstancePlain& st_plain)
{
     {
        if (!source_formid) {
            logger::warn("Formid is null.");
            return nullptr;
        }

        const auto count = st_plain.count;
        if (!count) {
            logger::warn("Count is 0.");
            return nullptr;
        }
        if (!loc) {
            logger::warn("loc is 0.");
            return nullptr;
        }

        if (GetNInstances() > _instance_limit) {
            logger::warn("Instance limit reached.");
            MsgBoxesNotifs::InGame::CustomMsg(
                std::format("The mod is tracking over {} instances. Maybe it is not bad to check your memory usage and "
                            "skse co-save sizes.",
                            _instance_limit));
        }

        logger::trace("Registering new instance.Formid {:x} , Count {} , Location refid {:x}", source_formid, count, loc);
        // make new registry

        auto* src = ForceGetSource(source_formid);
        if (!src) {
            logger::warn("Source could not be obtained.");
            return nullptr;
        }

        //src->UpdateAddons();
		if (!src->IsHealthy()) {
			logger::warn("RegisterAtReceiveData: Source is not healthy.");
			return nullptr;
		}

        const auto stage_no = st_plain.no;
        if (!src->IsStageNo(stage_no)) {
            logger::warn("Stage not found.");
            return nullptr;
        }

        StageInstance new_instance(st_plain.start_time, stage_no, st_plain.count);
        const auto& stage_temp = src->GetStage(stage_no);
        new_instance.xtra.form_id = stage_temp.formid;
        new_instance.xtra.editor_id = clib_util::editorID::get_editorID(stage_temp.GetBound());
        new_instance.xtra.crafting_allowed = stage_temp.crafting_allowed;
        if (src->IsFakeStage(stage_no)) new_instance.xtra.is_fake = true;

        new_instance.SetDelay(st_plain);
        new_instance.xtra.is_transforming = st_plain.is_transforming;

        if (!src->InsertNewInstance(new_instance, loc)) {
            logger::warn("RegisterAtReceiveData: InsertNewInstance failed.");
            return nullptr;
        }
        logger::trace("New instance registered at load game.");
        return &src->data[loc].back();
    }
}

void Manager::ReceiveData()
{
    logger::info("-------- Receiving data (Manager) ---------");


    if (m_Data.empty()) {
        logger::warn("ReceiveData: No data to receive.");
        return;
    }
    if (should_reset) {
        logger::info("ReceiveData: User wants to reset.");
        Reset();
        MsgBoxesNotifs::InGame::CustomMsg(
            "The mod has been reset. Please save and close the game. Do not forget to set bReset back to false in "
            "the INI before loading your save.");
        return;
    }

    // I need to deal with the fake forms from last session
    // trying to make sure that the fake forms in bank will be used when needed
	auto* DFT = DynamicFormTracker::GetSingleton();
    for (const auto source_forms = DFT->GetSourceForms(); const auto& [source_formid, source_editorid] : source_forms) {
        if (IsSource(source_formid)) {
            for (const auto dynamic_formid : DFT->GetFormSet(source_formid, source_editorid)) {
                DFT->Reserve(source_formid, source_editorid,dynamic_formid);
            }
		}
	}

    DFT->ApplyMissingActiveEffects();

    /////////////////////////////////

    int n_instances = 0;
    for (const auto& [lhs, rhs] : m_Data) {
        const auto& [form_id, editor_id] = lhs.first;
        auto source_formid = form_id;
        const auto& source_editorid = editor_id;
        const auto loc = lhs.second;
        if (!source_formid) {
            logger::error("ReceiveData: Formid is null.");
            continue;
        }
        if (source_editorid.empty()) {
            logger::error("ReceiveData: Editorid is empty.");
            continue;
        }
        const auto source_form = GetFormByID(0, source_editorid);
        if (!source_form) {
            logger::critical("ReceiveData: Source form not found. Saved formid: {}, editorid: {}", source_formid,
                                source_editorid);
            continue;
        }
        if (source_form->GetFormID() != source_formid) {
            logger::warn("ReceiveData: Source formid does not match. Saved formid: {}, editorid: {}", source_formid,
                            source_editorid);
            source_formid = source_form->GetFormID();
        }
        for (const auto& st_plain : rhs) {
            if (st_plain.is_fake) locs_to_be_handled[loc].push_back(st_plain.form_id);
            if (const auto* inserted_instance = RegisterAtReceiveData(source_formid, loc, st_plain); !inserted_instance) {
                logger::warn("ReceiveData: could not insert instance: formid: {}, loc: {}", source_formid, loc);
                continue;
            }
            n_instances++;
        }
    }

    logger::trace("Deleting unused fake forms from bank.");
    listen_container_change.store(false);
    DFT->DeleteInactives();
    listen_container_change.store(true);
    if (DFT->GetNDeleted() > 0) {
        logger::warn("ReceiveData: Deleted forms exist. User is required to restart.");
        MsgBoxesNotifs::InGame::CustomMsg(
            "It seems the configuration has changed from your previous session"
            " that requires you to restart the game."
            "DO NOT IGNORE THIS:"
            "1. Save your game."
            "2. Exit the game."
            "3. Restart the game."
            "4. Load the saved game."
            "JUST DO IT! NOW! BEFORE DOING ANYTHING ELSE!");
    } else {
        HandleLoc(player_ref);
        if (const auto it = locs_to_be_handled.find(player_refid); it != locs_to_be_handled.end()) {
            locs_to_be_handled.erase(it);
        }
        Print();
    }

    logger::info("--------Data received. Number of instances: {}---------", n_instances);
}

void Manager::Print()
{
    return;
    /*logger::info("Printing sources...Current time: {}", RE::Calendar::GetSingleton()->GetHoursPassed());
    for (auto& src : sources) {
        if (src.data.empty()) continue;
        src.PrintData();
    }*/
}

void Manager::HandleDynamicWO(RE::TESObjectREFR* ref)
{
	// if there is an object in the world that is a dynamic base form and comes from this mod, swap it back to the main stage form
	if (!ref) return;

	if (const auto bound = ref->GetObjectReference()) {
		if (!bound->IsDynamicForm()) return;
		const auto* src = GetSource(bound->GetFormID());
		if (!src) return;
        WorldObject::SwapObjects(ref, src->GetBoundObject(), false);
	}
}

void Manager::HandleWOBaseChange(RE::TESObjectREFR* ref)
{
	if (!ref) return;
	if (const auto bound = ref->GetObjectReference()) {
		if (bound->IsDynamicForm()) return HandleDynamicWO(ref);
        const auto* src = GetSource(bound->GetFormID());
		if (!src || !src->IsHealthy()) return;
        auto* st_inst = GetWOStageInstance(ref);
		if (!st_inst || st_inst->count <= 0) return;
        if (const auto* bound_expected = src->IsFakeStage(st_inst->no) ? src->GetBoundObject() : st_inst->GetBound(); bound_expected->GetFormID() != bound->GetFormID()) {
	        st_inst->count = 0;
			std::unique_lock lock(queueMutex_);
			queue_delete_.insert(ref->GetFormID());
        }
	}
}
