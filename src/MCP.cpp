#include "MCP.h"
#include "SimpleIni.h"

void HelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void __stdcall UI::RenderSettings()
{

    for (const auto& [section_name, section_settings] : Settings::INI_settings) {
        if (ImGui::CollapsingHeader(section_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("table_settings", 2, table_flags)) {
                for (const auto& [setting_name, setting] : section_settings) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (setting_name == "DisableWarnings") {
                        IniSettingToggle(Settings::disable_warnings,setting_name,section_name,"Disables in-game warning pop-ups.");
                    }
                    else if (setting_name == "WorldObjectsEvolve") {
						bool temp = Settings::world_objects_evolve.load();
                        IniSettingToggle(temp,setting_name,section_name,"Allows items out in the world to transform.");
						Settings::world_objects_evolve.store(temp);
					}
					else if (setting_name == "PlacedObjectsEvolve") {
						bool temp = Settings::placed_objects_evolve.load();
						IniSettingToggle(temp, setting_name, section_name, "Allows hand-placed objects (vanilla or by mods) to transform.");
						Settings::placed_objects_evolve.store(temp);
					}
					else if (setting_name == "UnOwnedObjectsEvolve") {
						bool temp = Settings::unowned_objects_evolve.load();
						IniSettingToggle(temp, setting_name, section_name, "Allows unowned objects to transform.");
						Settings::unowned_objects_evolve.store(temp);
					}
                    else {
                        // we just want to display the settings in read only mode
                        ImGui::Text(setting_name.c_str());
                    }
                    ImGui::TableNextColumn();
                    const auto temp_setting_val = setting_name == "DisableWarnings" ? Settings::disable_warnings  : setting;
                    const char* value = temp_setting_val ? "Enabled" : "Disabled";
                    const auto color = setting ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
                    ImGui::TextColored(color, value);
                }
                ImGui::EndTable();
            }
        }
    }
}
void __stdcall UI::RenderStatus()
 {
    constexpr auto color_operational = ImVec4(0, 1, 0, 1);
    constexpr auto color_not_operational = ImVec4(1, 0, 0, 1);


	if (!M) {
		ImGui::TextColored(color_not_operational, "Mod is not working! Check log for more info.");
        return;
	}

    if (ImGui::BeginTable("table_status", 3, table_flags)) {
        ImGui::TableSetupColumn("Module");
        ImGui::TableSetupColumn("Default Preset");
        ImGui::TableSetupColumn("Custom Presets");
        ImGui::TableHeadersRow();
        for (const auto& [module_name,module_enabled] : Settings::INI_settings["Modules"]) {
            if (!module_enabled) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text(module_name.c_str());
            ImGui::TableNextColumn();
			const auto loaded_default = Settings::defaultsettings.contains(module_name) ? Settings::defaultsettings.at(module_name).IsHealthy() : false;
            const auto loaded_default_str = loaded_default ? "Loaded" : "Not Loaded";
            const auto color = loaded_default ? color_operational : color_not_operational;
            ImGui::TextColored(color, loaded_default_str);
            ImGui::TableNextColumn();
            const auto& custom_presets = Settings::custom_settings[module_name];
                
			std::string loaded_custom_str = "Not Loaded";
			auto color_custom = color_not_operational;
            if (!custom_presets.empty()) {
                loaded_custom_str = std::format("Loaded ({})", custom_presets.size());
                color_custom = color_operational;
			}
            ImGui::TextColored(color_custom, loaded_custom_str.c_str());
        }
        ImGui::EndTable();
    }

	ExcludeList();
}
void __stdcall UI::RenderInspect()
{

    if (!M) {
        ImGui::Text("Not available");
        return;
    }

	RefreshButton();

    ImGui::Text("Location");
    if (ImGui::BeginCombo("##combo 1", item_current.c_str())) {
        for (const auto& key : locations | std::views::keys) {
            if (filter->PassFilter(key.c_str())) {
                if (const bool is_selected = item_current == key; ImGui::Selectable(key.c_str(), is_selected)) {
                    item_current = key;
					UpdateSubItem();
                }
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::SameLine();
	DrawFilter1();

    is_list_box_focused = ImGui::IsItemHovered(ImGuiHoveredFlags_::ImGuiHoveredFlags_NoNavOverride) || ImGui::IsItemActive();

    if (locations.contains(item_current)) 
    {
        InstanceMap& selectedItem = locations[item_current];
        ImGui::Text("Item");
        if (ImGui::BeginCombo("##combo 2", sub_item_current.c_str())) {
            for (const auto& key : selectedItem | std::views::keys) {
                if (filter2->PassFilter(key.c_str())) {
                    const bool is_selected = (sub_item_current == key);
                    if (ImGui::Selectable(key.c_str(), is_selected)) {
                        sub_item_current = key;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
		DrawFilter2();

        if (selectedItem.contains(sub_item_current)) {
            const auto& selectedInstances = selectedItem[sub_item_current];

            ImGui::Text("Instances");
            if (ImGui::BeginTable("table_inspect", 8, table_flags)) {
                ImGui::TableSetupColumn("Stage");
                ImGui::TableSetupColumn("Count");
                ImGui::TableSetupColumn("Start Time");
                ImGui::TableSetupColumn("Duration");
                ImGui::TableSetupColumn("Time Modulation");
                ImGui::TableSetupColumn("Dynamic Form");
                ImGui::TableSetupColumn("Transforming");
                ImGui::TableSetupColumn("Decayed");
                ImGui::TableHeadersRow();
                for (const auto& item : selectedInstances) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(std::format("{}/{} {}", item.stage_number.first, item.stage_number.second,item.stage_name).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(std::format("{}", item.count).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(std::format("{}", item.start_time).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(std::format("{}", item.duration).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(std::format("{} ({})", item.delay_magnitude, item.delayer).c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.is_fake? "Yes" : "No");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.is_transforming ? "Yes" : "No");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(item.is_decayed ? "Yes" : "No");
                }
                ImGui::EndTable();
            }
        }
    }

}
void __stdcall UI::RenderUpdateQ()
{
	if (!M) {
		ImGui::Text("Not available");
		return;
	}

	RefreshButton();
	ImGui::Text("Update Queue: %s", M->IsTickerActive() ? "Active" : "Paused");
	// need a combo box to select the ticker speed
    ImGui::SetNextItemWidth(180.f);
	const auto ticker_speed_str = Settings::Ticker::to_string(Settings::ticker_speed);
	if (ImGui::BeginCombo("##combo_ticker_speed", ticker_speed_str.c_str())) {
		for (int i = 0; i < Settings::Ticker::enum_size; ++i) {
			const auto speed = static_cast<Settings::Ticker::Intervals>(i);
			const auto speed_str = Settings::Ticker::to_string(speed);
			if (ImGui::Selectable(speed_str.c_str(), Settings::ticker_speed == speed)) {
				Settings::ticker_speed = speed;
				M->UpdateInterval(std::chrono::milliseconds(Settings::Ticker::GetInterval(Settings::ticker_speed)));
                PresetParse::SaveSettings();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    HelpMarker("Choosing faster options reduces the time between updates, making the evolution of items out in the world more responsive. It will take time when switching from slower settings.");

	if (Settings::world_objects_evolve.load()) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "World Objects Evolve: Enabled");
	}
	else {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "World Objects Evolve: Disabled");
	}

	if (ImGui::BeginTable("table_queue", 2, table_flags)) {
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Update Time");
		ImGui::TableHeadersRow();
		for (const auto& [fst, snd] : update_q | std::views::values) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(fst.c_str());
			ImGui::TableNextColumn();
			ImGui::Text(std::format("{}", snd).c_str());
		}
		ImGui::EndTable();
	}

}
void __stdcall UI::RenderStages()
{
	if (!M) {
		ImGui::Text("Not available");
		return;
	}

    RefreshButton();

	if (mcp_sources.empty()) {
		ImGui::Text("No stages available");
		return;
	}


	ImGui::Text("Source List");

	source_current = "Source " + std::to_string(selected_source_index+1) + ": " + mcp_sources[selected_source_index].stages.begin()->item.name;

    if (ImGui::BeginCombo("##SourceList", source_current.c_str())) {
        for (size_t i = 0; i < mcp_sources.size(); ++i) {
			if (filter_module != "None" && mcp_sources[i].type != filter_module) continue;
            std::string label = "Source " + std::to_string(i+1) + ": " + mcp_sources[i].stages.begin()->item.name;
            if (ImGui::Selectable(label.c_str(), selected_source_index == static_cast<int>(i))) {
                selected_source_index = static_cast<int>(i);
				source_current = label;
            }
        }
        ImGui::EndCombo();
    }

	ImGui::SameLine();
    if (const auto module_filter_old = filter_module; DrawFilterModule() && module_filter_old != filter_module) {
        for (size_t i = 0; i < mcp_sources.size(); ++i) {
			if (filter_module != "None" && mcp_sources[i].type != filter_module) continue;
			selected_source_index = static_cast<int>(i);
			source_current = "Source " + std::to_string(i + 1) + ": " + mcp_sources[i].stages.begin()->item.name;
            break;
		}
	}

    ImGui::SameLine();
    if (ImGui::Button("Exclude##addtoexclude")) {
		const auto& temp_selected_source = mcp_sources[selected_source_index];
		const auto temp_form = RE::TESForm::LookupByID(temp_selected_source.stages.begin()->item.formid);
        if (const auto temp_editorid = clib_util::editorID::get_editorID(temp_form); !temp_editorid.empty()) {
			Settings::AddToExclude(temp_editorid, temp_selected_source.type, "MCP");
        }
    }

	if (mcp_sources.empty() || selected_source_index >= mcp_sources.size()) {
		ImGui::Text("No sources available");
		return;
	}

    ImGui::Text("");
    ImGui::Text("Stages");
	const auto& src = mcp_sources[selected_source_index];

	if (ImGui::BeginTable("table_stages", 5, table_flags)) {
		ImGui::TableSetupColumn("Item");
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Duration");
		ImGui::TableSetupColumn("Crafting Allowed");
		ImGui::TableSetupColumn("Is Dynamic Form");
		ImGui::TableHeadersRow();
		for (const auto& stage : src.stages) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text((stage.item.name + std::format(" ({:x})",stage.item.formid)).c_str());
			ImGui::TableNextColumn();
			ImGui::Text(stage.name.c_str());
			ImGui::TableNextColumn();
			ImGui::Text(std::format("{}", stage.duration).c_str());
			ImGui::TableNextColumn();
			ImGui::Text(stage.crafting_allowed ? "Yes" : "No");
			ImGui::TableNextColumn();
			ImGui::Text(stage.is_fake ? "Yes" : "No");

		}
		ImGui::EndTable();
	}

    ImGui::Text("");
    ImGui::Text("Containers");
	if (ImGui::BeginTable("table_containers", 1, table_flags)) {
		ImGui::TableSetupColumn("Container (FormID)");
		ImGui::TableHeadersRow();
		for (const auto& [name, formid] : src.containers) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text((name + std::format(" ({:x})", formid)).c_str());
		}
		ImGui::EndTable();
	}


    ImGui::Text("");
    ImGui::Text("Transformers");
	if (ImGui::BeginTable("table_transformers", 3, table_flags)) {
		ImGui::TableSetupColumn("Item");
		ImGui::TableSetupColumn("Transformed Item");
		ImGui::TableSetupColumn("Duration");
		ImGui::TableHeadersRow();
		for (const auto& [name, formid] : src.transformers) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text((name + std::format(" ({:x})", formid)).c_str());
			//ImGui::TableNextColumn();
			const auto temp_name = src.transformer_enditems.contains(formid) ? src.transformer_enditems.at(formid).name : std::format("{:x}", formid);
			ImGui::TableNextColumn();
			ImGui::Text(temp_name.c_str());
			ImGui::TableNextColumn();
			const auto temp_duration = src.transform_durations.contains(formid) ? std::format("{}", src.transform_durations.at(formid)) : "???";
			ImGui::Text(temp_duration.c_str());
		}
		ImGui::EndTable();
	}


    ImGui::Text("");
	ImGui::Text("Time Modulators");
	if (ImGui::BeginTable("table_time_modulators", 2, table_flags)) {
		ImGui::TableSetupColumn("Item");
        ImGui::TableSetupColumn("Multiplier");
		ImGui::TableHeadersRow();
		for (const auto& [name, formid] : src.time_modulators) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text((name + std::format(" ({:x})", formid)).c_str());
			ImGui::TableNextColumn();
			const auto temp_duration = src.time_modulator_multipliers.contains(formid) ? std::format("{}", src.time_modulator_multipliers.at(formid)) : "???";
			ImGui::Text(temp_duration.c_str());
		}
		ImGui::EndTable();
	}
}
void __stdcall UI::RenderDFT()
{
    RefreshButton();

	ImGui::Text(std::format("Dynamic Forms ({}/{})", dynamic_forms.size(),dft_form_limit).c_str());
	if (dynamic_forms.empty()) {
		ImGui::Text("No dynamic forms found.");
		return;
	}
	// dynamic forms table: FormID, Name, Status
	if (ImGui::BeginTable("table_dynamic_forms", 3, table_flags)) {
		for (const auto& [formid, form] : dynamic_forms) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text(std::format("{:08X}", formid).c_str());
			ImGui::TableNextColumn();
			ImGui::Text(form.first.c_str());
			ImGui::TableNextColumn();
			const auto color = form.second == 2 ? ImVec4(0, 1, 0, 1) : form.second == 1 ? ImVec4(1, 1, 0, 1) : ImVec4(1, 0, 0, 1);
			ImGui::TextColored(color, form.second == 2 ? "Active" : form.second == 1 ? "Protected" : "Inactive");
		}
		ImGui::EndTable();
	}
}
void __stdcall UI::RenderLog()
{
#ifndef NDEBUG
    ImGui::Checkbox("Trace", &LogSettings::log_trace);
#endif
    ImGui::SameLine();
    ImGui::Checkbox("Info", &LogSettings::log_info);
    ImGui::SameLine();
    ImGui::Checkbox("Warning", &LogSettings::log_warning);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &LogSettings::log_error);

    // if "Generate Log" button is pressed, read the log file
    if (ImGui::Button("Generate Log")) logLines = ReadLogFile();

    // Display each line in a new ImGui::Text() element
    for (const auto& line : logLines) {
        if (!LogSettings::log_trace && line.find("trace") != std::string::npos) continue;
        if (!LogSettings::log_info && line.find("info") != std::string::npos) continue;
        if (!LogSettings::log_warning && line.find("warning") != std::string::npos) continue;
        if (!LogSettings::log_error && line.find("error") != std::string::npos) continue;
        ImGui::Text(line.c_str());
    }
}
void UI::Register(Manager* manager)
{

    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    } 

    filter = new ImGuiTextFilter();
    filter2 = new ImGuiTextFilter();

    SKSEMenuFramework::SetSection(mod_name);
    SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
    SKSEMenuFramework::AddSectionItem("Status", RenderStatus);
    SKSEMenuFramework::AddSectionItem("Inspect", RenderInspect);
	SKSEMenuFramework::AddSectionItem("Update Queue", RenderUpdateQ);
	SKSEMenuFramework::AddSectionItem("Stages", RenderStages);
	SKSEMenuFramework::AddSectionItem("Dynamic Forms", RenderDFT);
    SKSEMenuFramework::AddSectionItem("Log", RenderLog);
    M = manager;
}
void UI::ExcludeList()
{
    ImGui::Text("");
    ImGui::Text("Exclusions per Module:");

    for (const auto& [qform, excludes] : Settings::exclude_list) {
        if (ImGui::CollapsingHeader(qform.c_str())) {
            if (ImGui::BeginTable(("#exclude_"+qform).c_str(), 1, table_flags)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
				for (const auto& exclude : excludes) {
					ImGui::Text(exclude.c_str());
				}

                ImGui::EndTable();
            }
        }
    }
}
void UI::IniSettingToggle(bool& setting, const std::string& setting_name, const std::string& section_name, const char* desc)
{
    const bool previous_state = setting;
    ImGui::Checkbox(setting_name.c_str(), &setting);
    if (Settings::disable_warnings != previous_state) {
        // save to INI
        Settings::INI_settings[section_name][setting_name] = setting;
        CSimpleIniA ini;
        ini.SetUnicode();
        ini.LoadFile(Settings::INI_path);
        ini.SetBoolValue(section_name.c_str(), setting_name.c_str(), setting);
        ini.SaveFile(Settings::INI_path);
    }
    ImGui::SameLine();
    if (desc) HelpMarker(desc);
}
void UI::DrawFilter1()
{   
    if (filter->Draw("Location filter",200)) {
        if (!filter->PassFilter(item_current.c_str())){
		    for (const auto& key : locations | std::views::keys) {
                if (filter->PassFilter(key.c_str())) {
                    item_current = key;
					UpdateSubItem();
                    break;
                }
            }
        }
    }
}
void UI::DrawFilter2()
{   
	if (filter2->Draw("Item filter",200)) {
		if (!filter2->PassFilter(sub_item_current.c_str())) UpdateSubItem();
	}
}
bool UI::DrawFilterModule()
{
	ImGui::Text("Module Filter:");
	ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
	if (ImGui::BeginCombo("##combo 3", filter_module.c_str())) {
		if (ImGui::Selectable("None", filter_module == "None")) {
			filter_module = "None";
		}
		for (const auto& [module_name, module_enabled] : Settings::INI_settings["Modules"]) {
			if (!module_enabled) continue;
			if (ImGui::Selectable(module_name.c_str(), filter_module == module_name)) {
				filter_module = module_name;
			}
		}
		ImGui::EndCombo();
		return true;
	}
	return false;
}
void UI::UpdateSubItem()
{
    for (const auto& key2 : locations[item_current] | std::views::keys) {
		if (filter2->PassFilter(key2.c_str())) {
			sub_item_current = key2;
			break;
		}
	}
}
void UI::UpdateLocationMap(const std::vector<Source>& sources)
{
    locations.clear();

    for (const auto& source : sources) {
        for (const auto& [location, instances] : source.data) {
            const auto* locationReference = RE::TESForm::LookupByID<RE::TESObjectREFR>(location);
            std::string locationName_temp;
            if (locationReference) {
				locationName_temp = locationReference->GetName();
                if (locationReference->HasContainer()) locationName_temp += std::format(" ({:x})", location);
			}
			else {
				locationName_temp = std::format("{:x}", location);
			}
            const char* locationName = locationName_temp.c_str();
            for (auto& stageInstance : instances) {
                const auto* delayerForm = RE::TESForm::LookupByID(stageInstance.GetDelayerFormID());
                auto delayer_name = delayerForm ? delayerForm->GetName() : std::format("{:x}", stageInstance.GetDelayerFormID());
                if (delayer_name == "0") delayer_name = "None";
                int max_stage_no = 0;
                while (source.IsStageNo(max_stage_no+1)) max_stage_no++;
                    
                const auto temp_stage_no = std::make_pair(stageInstance.no, max_stage_no);
                auto temp_stagename = source.GetStageName(stageInstance.no);
                temp_stagename = temp_stagename.empty() ? "" : std::format("({})", temp_stagename);
                Instance instance(
                    temp_stage_no, 
                    temp_stagename,
                    stageInstance.count,
                    stageInstance.start_time,
                    source.GetStageDuration(stageInstance.no),
                    stageInstance.GetDelayMagnitude(),
                    delayer_name,
                    stageInstance.xtra.is_fake,
                    stageInstance.xtra.is_transforming,
                    stageInstance.xtra.is_decayed
                );

                const auto item = RE::TESForm::LookupByID(source.formid);
                locations[std::string(locationName) + "##location"][(item ? item->GetName() : source.editorid)+ "##item"]
                    .push_back(
                    instance);
            }
        }
    }

    if (const auto current = locations.find(item_current); current == locations.end()){
        item_current = "##current";
        sub_item_current = "##item"; 
    } else if (const auto item = current->second; item.find(sub_item_current) == item.end()) {
        sub_item_current = "##item";
    }
}

void UI::UpdateStages(const std::vector<Source>& sources)
{
    mcp_sources.clear();

    for (const auto& source : sources) {
        if (!source.IsHealthy()) continue;
        StageNo max_stage_no = 0;
		std::set<Stage> temp_stages;
		while (source.IsStageNo(max_stage_no)) {
            if (const auto* stage = source.GetStageSafe(max_stage_no)) {
				const auto* temp_form = RE::TESForm::LookupByID(stage->formid);
				if (!temp_form) continue;
                const GameObject item = {temp_form->GetName(),stage->formid};
				temp_stages.insert(Stage(item, stage->name, stage->duration, source.IsFakeStage(max_stage_no), stage->crafting_allowed,max_stage_no));
			}
			max_stage_no++;
		}
		const auto& stage = source.GetDecayedStage();
        if (const auto* temp_form = RE::TESForm::LookupByID(stage.formid)) {
			const GameObject item = { .name= temp_form->GetName(),.formid= stage.formid };
			temp_stages.insert(Stage(item, "Final", 0.f, source.IsFakeStage(max_stage_no), stage.crafting_allowed, max_stage_no));
		}
        std::set<GameObject> containers_;
		for (const auto& container : source.settings.containers) {
			const auto temp_formid = container;
			const auto temp_name = GetName(temp_formid);
			containers_.insert(GameObject{ temp_name,temp_formid });
		}

        std::set<GameObject> transformers_;
		std::map<FormID,GameObject> transformer_enditems_;
		std::map<FormID,Duration> transform_durations_;
		for (const auto& [fst, snd] : source.settings.transformers) {
			auto temp_formid = fst;
			const auto temp_name = GetName(temp_formid);
			transformers_.insert(GameObject{ temp_name,temp_formid });
			const auto temp_formid2 = std::get<0>(snd);
			auto temp_name2 = GetName(temp_formid2);
			transformer_enditems_[temp_formid] = GameObject{ temp_name2,temp_formid2 };
			transform_durations_[temp_formid] = std::get<1>(snd);
		}
		std::set<GameObject> time_modulators_;
		std::map<FormID,float> time_modulator_multipliers_;
		for (const auto& [fst, snd] : source.settings.delayers) {
			auto temp_formid = fst;
			const auto temp_form = RE::TESForm::LookupByID(temp_formid);
			const auto temp_name = temp_form ? temp_form->GetName() : std::format("{:x}", temp_formid);
			time_modulators_.insert(GameObject{ temp_name,temp_formid });
			time_modulator_multipliers_[temp_formid] = snd;
		}

		const auto qform_type = Settings::GetQFormType(source.formid);
		mcp_sources.push_back(MCPSource{ temp_stages,containers_,transformers_,transformer_enditems_,transform_durations_,time_modulators_,time_modulator_multipliers_,qform_type});
	}
}

void UI::RefreshButton()
{
    FontAwesome::PushSolid();

    if (ImGui::Button((FontAwesome::UnicodeToUtf8(0xf021) + " Refresh").c_str()) || last_generated.empty()) {
		Refresh();
    }
    FontAwesome::Pop();

    ImGui::SameLine();
    ImGui::Text(("Last Generated: " + last_generated).c_str());
}

void UI::Refresh()
{
    last_generated = std::format("{} (in-game hours)", RE::Calendar::GetSingleton()->GetHoursPassed());
	dynamic_forms.clear();
    for (const auto DFT = DynamicFormTracker::GetSingleton(); const auto& df : DFT->GetDynamicForms()) {
		if (const auto form = RE::TESForm::LookupByID(df); form) {
			auto status = DFT->IsActive(df) ? 2 : DFT->IsProtected(df) ? 1 : 0;
			dynamic_forms[df] = { form->GetName(), status };
		}
    }

    const auto sources = M->GetSources();
    UpdateLocationMap(sources);
	UpdateStages(sources);

	update_q.clear();
    for (const auto [refid, stop_time] : M->GetUpdateQueue()) {
		if (const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid)) {
			std::string temp_name = std::format("{} ({:x})", ref->GetName(), refid);
			update_q[refid] = std::make_pair(temp_name, stop_time);
		}
        else {
			update_q[refid] = std::make_pair(std::format("{:x}", refid), stop_time);
		}
    }
}

std::string UI::GetName(FormID formid)
{
    const auto temp_form = RE::TESForm::LookupByID(formid);
    auto temp_name = temp_form ? temp_form->GetName() : std::format("{:x}", formid);
	if (temp_name.empty()) temp_name = clib_util::editorID::get_editorID(temp_form);
	if (temp_name.empty()) temp_name = "???";
	return temp_name;
}
