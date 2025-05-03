#include "Settings.h"
#include <future>
#include <utility>
#include "SimpleIni.h"
#include "Threading.h"


using QFormChecker = bool(*)(const RE::TESForm*);

static const std::unordered_map<std::string, QFormChecker> qformCheckers = {
    // POPULATE THIS
	{"FOOD", [](const auto form) {return IsFoodItem(form); } },
	{"INGR", [](const auto form) {return FormIsOfType(form, RE::IngredientItem::FORMTYPE); } },
	{"MEDC", [](const auto form) {return IsMedicineItem(form); } },
	{"POSN", [](const auto form) {return IsPoisonItem(form); } },
	{"ARMO", [](const auto form) {return FormIsOfType(form, RE::TESObjectARMO::FORMTYPE); } },
	{"WEAP", [](const auto form) {return FormIsOfType(form, RE::TESObjectWEAP::FORMTYPE); } },
	{"SCRL", [](const auto form) {return FormIsOfType(form, RE::ScrollItem::FORMTYPE); } },
	{"BOOK", [](const auto form) {return FormIsOfType(form, RE::TESObjectBOOK::FORMTYPE); } },
	{"SLGM", [](const auto form) {return FormIsOfType(form, RE::TESSoulGem::FORMTYPE); } },
	{"MISC", [](const auto form) {return FormIsOfType(form, RE::TESObjectMISC::FORMTYPE); } },
	//{"NPC", [](const auto form) {return FormIsOfType(form, RE::TESNPC::FORMTYPE); } }
};

bool Settings::IsQFormType(const FormID formid, const std::string& qformtype) {
    // POPULATE THIS
    /*const auto* form = GetFormByID(formid);
    if (qformtype == "FOOD") return IsFoodItem(form);
    if (qformtype == "INGR") return FormIsOfType(form, RE::IngredientItem::FORMTYPE);
    if (qformtype == "MEDC") return IsMedicineItem(form);
    if (qformtype == "POSN") return IsPoisonItem(form);
    if (qformtype == "ARMO") return FormIsOfType(form,RE::TESObjectARMO::FORMTYPE);
    if (qformtype == "WEAP") return FormIsOfType(form,RE::TESObjectWEAP::FORMTYPE);
    if (qformtype == "SCRL") return FormIsOfType(form, RE::ScrollItem::FORMTYPE);
	if (qformtype == "BOOK") return FormIsOfType(form,RE::TESObjectBOOK::FORMTYPE);
    if (qformtype == "SLGM") return FormIsOfType(form, RE::TESSoulGem::FORMTYPE);
	if (qformtype == "MISC") return FormIsOfType(form,RE::TESObjectMISC::FORMTYPE);
	if (qformtype == "NPC") return FormIsOfType(form,RE::TESNPC::FORMTYPE);
    return false;*/

    const auto* form = GetFormByID(formid);
    const auto it = qformCheckers.find(qformtype);
    return (it != qformCheckers.end()) ? it->second(form) : false;
}

std::string Settings::GetQFormType(const FormID formid)
{
    for (const auto& q_ftype : QFORMS) {
        if (IsQFormType(formid,q_ftype)) return q_ftype;
	}
	return "";
}

bool Settings::IsSpecialQForm(RE::TESObjectREFR* ref)
{
	const auto base = ref->GetBaseObject();
	if (!base) return false;
	const auto qform_type = GetQFormType(base->GetFormID());
	if (qform_type.empty()) return false;
	return std::ranges::any_of(sQFORMS, [qform_type](const std::string& qformtype) { return qformtype == qform_type; });
}

bool Settings::IsInExclude(const FormID formid, std::string type) {
    const auto form = GetFormByID(formid);
    if (!form) {
        logger::warn("Form not found.");
        return false;
    }
        
    if (type.empty()) type = GetQFormType(formid);
    if (type.empty()) {
		return false;
	}
    if (!Settings::exclude_list.contains(type)) {
		logger::critical("Type not found in exclude list. for formid: {}", formid);
        return false;
    }

    const auto form_string = std::string(form->GetName());

    if (const std::string form_editorid = clib_util::editorID::get_editorID(form); !form_editorid.empty() && String::includesWord(form_editorid, Settings::exclude_list[type])) {
		return true;
	}

    /*const auto exlude_list = LoadExcludeList(postfix);*/
    if (String::includesWord(form_string, Settings::exclude_list[type])) {
        return true;
    }
    return false;
}

void Settings::AddToExclude(const std::string& entry_name, const std::string& type, const std::string& filename)
{
	if (entry_name.empty() || type.empty() || filename.empty()) {
		logger::warn("Empty entry_name, type or filename.");
		return;
	}
	// check if type is a valid qformtype
	if (!std::ranges::any_of(Settings::QFORMS, [type](const std::string& qformtype) { return qformtype == type; })) {
		logger::warn("AddToExclude: Invalid type: {}", type);
		return;
	}

	const auto folder_path = "Data/SKSE/Plugins/AlchemyOfTime/" + type + "/exclude";
	std::filesystem::create_directories(folder_path);
	const auto file_path = folder_path + "/" + filename + ".txt";
	// check if the entry is already in the list
	if (std::ranges::find(Settings::exclude_list[type], entry_name) != Settings::exclude_list[type].end()) {
		logger::warn("Entry already in exclude list: {}", entry_name);
		return;
	}
	std::ofstream file(file_path, std::ios::app);
	file << entry_name << '\n';
	file.close();
	Settings::exclude_list[type].push_back(entry_name);
}

bool Settings::IsItem(const FormID formid, const std::string& type, const bool check_exclude) {
    if (!formid) return false;
    if (check_exclude && Settings::IsInExclude(formid, type)) return false;
    if (type.empty()) return !GetQFormType(formid).empty();
	return IsQFormType(formid, type);
}

bool Settings::IsItem(const RE::TESObjectREFR* ref, const std::string& type) {
    const auto base = ref->GetBaseObject();
    if (!base) return false;
    return IsItem(base->GetFormID(), type);
}

DefaultSettings* Settings::GetDefaultSetting(const FormID form_id)
{
    const auto qform_type = Settings::GetQFormType(form_id);

    if (!IsItem(form_id, qform_type, true)) {
        return nullptr;
    }
    if (!defaultsettings.contains(qform_type)) {
        return nullptr;
    }
    if (!defaultsettings[qform_type].IsHealthy()) {
        return nullptr;
    }

	return &defaultsettings[qform_type];
}

DefaultSettings* Settings::GetCustomSetting(const RE::TESForm* form)
{
	const auto form_id = form->GetFormID();
    const auto qform_type = GetQFormType(form_id);
    if (!qform_type.empty() && custom_settings.contains(qform_type)) {
        for (auto& customSetting = custom_settings[qform_type]; auto& [names, sttng] : customSetting) {
            if (!sttng.IsHealthy()) continue;
            for (auto& name : names) {
                if (const FormID temp_cstm_formid = GetFormEditorIDFromString(name); 
                    temp_cstm_formid > 0) {
                    if (const auto temp_cstm_form = GetFormByID(temp_cstm_formid, name); 
                        temp_cstm_form && temp_cstm_form->GetFormID() == form_id) {
                        return &sttng;
                    }
                }
            }
            if (String::includesWord(form->GetName(), names)) {
                return &sttng;
            }
        }
    }
	return nullptr;
}

AddOnSettings* Settings::GetAddOnSettings(const RE::TESForm* form)
{

    const auto form_id = form->GetFormID();
    const auto qform_type = GetQFormType(form_id);
	if (qform_type.empty()) return nullptr;
	auto& temp = addon_settings[qform_type];
	if (!temp.contains(form_id)) return nullptr;
	if (auto& addon = temp.at(form_id); addon.CheckIntegrity()) {
		return &addon;
	}
	return nullptr;
}

void SaveSettings()
{
	using namespace rapidjson;
    Document doc;
    doc.SetObject();

    Document::AllocatorType& allocator = doc.GetAllocator();

    Value version(rapidjson::kObjectType);
    version.AddMember("major", plugin_version.major(), allocator);
    version.AddMember("minor", plugin_version.minor(), allocator);
    version.AddMember("patch", plugin_version.patch(), allocator);
    version.AddMember("build", plugin_version.build(), allocator);

    doc.AddMember("plugin_version", version, allocator);
    doc.AddMember("ticker", Settings::Ticker::to_json(allocator), allocator);

    // Convert JSON document to string
    StringBuffer buffer;
    Writer writer(buffer);
    doc.Accept(writer);

    // Write JSON to file
    std::string filename = Settings::json_path;
    create_directories(std::filesystem::path(filename).parent_path());
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        logger::error("Failed to open file for writing: {}", filename);
        return;
    }
    ofs << buffer.GetString() << '\n';
    ofs.close();
}

std::vector<std::string> LoadExcludeList(const std::string& postfix)
 {
    const auto folder_path = "Data/SKSE/Plugins/AlchemyOfTime/" + postfix + "/exclude";

    // Create folder if it doesn't exist
    std::filesystem::create_directories(folder_path);
    std::unordered_set<std::string> strings;

    // Iterate over files in the folder
    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
        // Check if the entry is a regular file and ends with ".txt"
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            std::ifstream file(entry.path());
            std::string line;
            while (std::getline(file, line)) {
                if (!line.empty()) strings.insert(line);
            }
        }
    }

    std::vector result(strings.begin(), strings.end());
    return result;
}

AddOnSettings parseAddOns_(const YAML::Node& config)
{
    AddOnSettings settings;
    constexpr int warnings_limit = 5;

    // containers
    if (config["containers"] && !config["containers"].IsNull()) {
		const auto temp_containers = config["containers"].IsScalar() ? std::vector{config["containers"].as<std::string>()} : config["containers"].as<std::vector<std::string>>();
		for (const auto& container : temp_containers) {
			if (const FormID temp_formid = GetFormEditorIDFromString(container)) {
				settings.containers.insert(temp_formid);
			}
		}
    } 

    // delayers
    int n_warnings = 0;
    for (const auto& modulator : config["timeModulators"]) {
        const auto temp_formeditorid = modulator["FormEditorID"] && !modulator["FormEditorID"].IsNull()
                                            ? modulator["FormEditorID"].as<std::string>()
                                            : "";
        const FormID temp_formid = GetFormEditorIDFromString(temp_formeditorid);
		if (!temp_formid) {
			if (n_warnings < warnings_limit) {
			    logger::warn("timeModulators: Formid is 0 for {}", temp_formeditorid);
            }
			n_warnings++;
			continue;
		}
        settings.delayers[temp_formid] = !modulator["magnitude"].IsNull() ? modulator["magnitude"].as<float>() : 1;
		settings.delayers_order.push_back(temp_formid);

        // colors
		if (modulator["color"] && !modulator["color"].IsNull()) {
			settings.delayer_colors[temp_formid] = std::stoul(modulator["color"].as<std::string>(), nullptr, 16);
        }

		// sounds
		if (modulator["sound"] && !modulator["sound"].IsNull()) {
			const auto sound_formid = GetFormEditorIDFromString(modulator["sound"].as<std::string>());
			settings.delayer_sounds[temp_formid] = sound_formid;
        }

		// art_objects
		if (modulator["art_object"] && !modulator["art_object"].IsNull()) {
			const auto art_formid = GetFormEditorIDFromString(modulator["art_object"].as<std::string>());
			settings.delayer_artobjects[temp_formid] = art_formid;
        }

		// effect_shaders
		if (modulator["effect_shader"] && !modulator["effect_shader"].IsNull()) {
			const auto shader_formid = GetFormEditorIDFromString(modulator["effect_shader"].as<std::string>());
			settings.delayer_effect_shaders[temp_formid] = shader_formid;
        }

		// containers
		if (modulator["containers"] && !modulator["containers"].IsNull()) {
			const auto temp_containers = modulator["containers"].IsScalar() ? std::vector{ modulator["containers"].as<std::string>() } : modulator["containers"].as<std::vector<std::string>>();
			for (const auto& container : temp_containers) {
				if (const FormID container_formid = GetFormEditorIDFromString(container)) {
					settings.delayer_containers[temp_formid].insert(container_formid);
				}
            }
        }
    }

	// transformers
	n_warnings = 0;
    for (const auto& transformer : config["transformers"]) {
        const auto temp_formeditorid = transformer["FormEditorID"] &&
                                        !transformer["FormEditorID"].IsNull() ? transformer["FormEditorID"].as<std::string>() : "";
        const FormID temp_formid = GetFormEditorIDFromString(temp_formeditorid);
		if (!temp_formid) {
			if (n_warnings < warnings_limit) {
				logger::warn("transformers: Formid is 0 for {}", temp_formeditorid);
			}
			n_warnings++;
			continue;
		}
        const auto temp_finalFormEditorID =
            transformer["finalFormEditorID"] &&
            !transformer["finalFormEditorID"].IsNull() ? transformer["finalFormEditorID"].as<std::string>() : "";
        const FormID temp_formid2 = GetFormEditorIDFromString(temp_finalFormEditorID);
        if (!transformer["duration"] || transformer["duration"].IsNull()) {
			logger::warn("Duration is missing.");
			continue;
		}
        const auto temp_duration = transformer["duration"].as<float>();
        std::vector<StageNo> allowed_stages;
        if (!transformer["allowed_stages"]) {
            // default is all stages
            //for (const auto& key : settings.items | std::views::keys) {
            //    allowed_stages.push_back(key);
            //}
        } 
        else if (transformer["allowed_stages"].IsScalar()) {
            allowed_stages.push_back(transformer["allowed_stages"].as<StageNo>());
        } 
        else allowed_stages = transformer["allowed_stages"].as<std::vector<StageNo>>();

        const auto temp_tuple = std::make_tuple(temp_formid2, temp_duration, allowed_stages);
        settings.transformers[temp_formid] = temp_tuple;
		settings.transformers_order.push_back(temp_formid);

		// colors
		if (transformer["color"] && !transformer["color"].IsNull()) {
			settings.transformer_colors[temp_formid] = std::stoul(transformer["color"].as<std::string>(), nullptr, 16);
        }

		// sounds
		if (transformer["sound"] && !transformer["sound"].IsNull()) {
			const auto sound_formid = GetFormEditorIDFromString(transformer["sound"].as<std::string>());
			settings.transformer_sounds[temp_formid] = sound_formid;
        }

		// art_objects
		if (transformer["art_object"] && !transformer["art_object"].IsNull()) {
			const auto art_formid = GetFormEditorIDFromString(transformer["art_object"].as<std::string>());
			settings.transformer_artobjects[temp_formid] = art_formid;
        }

		// effect_shaders
		if (transformer["effect_shader"] && !transformer["effect_shader"].IsNull()) {
			const auto shader_formid = GetFormEditorIDFromString(transformer["effect_shader"].as<std::string>());
			settings.transformer_effect_shaders[temp_formid] = shader_formid;
        }

		// containers
		if (transformer["containers"] && !transformer["containers"].IsNull()) {
			const auto temp_containers = transformer["containers"].IsScalar() ? std::vector{ transformer["containers"].as<std::string>() } : transformer["containers"].as<std::vector<std::string>>();
			for (const auto& container : temp_containers) {
				if (const FormID container_formid = GetFormEditorIDFromString(container)) {
					settings.transformer_containers[temp_formid].insert(container_formid);
				}
			}
        }
    }
        
    if (!settings.CheckIntegrity()) logger::critical("Settings integrity check failed.");

    return settings;
}

std::unordered_map<FormID, AddOnSettings> parseAddOns(const std::string& _type)
{
    const auto folder_path = "Data/SKSE/Plugins/AlchemyOfTime/" + _type + "/addon";
    std::filesystem::create_directories(folder_path);

	std::unordered_map<FormID, AddOnSettings> _addon_settings;

    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".yml") continue;
        const auto filename = entry.path().string();
        processAddOnFile(filename,_addon_settings);
    }
    return _addon_settings;
}

DefaultSettings parseDefaults_(const YAML::Node& config)
 {
#ifndef NDEBUG
    logger::trace("Parsing settings.");
#endif
    DefaultSettings settings;

    //we have:stages, decayedFormEditorID and delayers
    if (!config["stages"] || config["stages"].size() == 0) {
        logger::error("Stages are empty.");
        return settings;
    }
    for (const auto& stageNode : config["stages"]) {
        if (!stageNode["no"] || stageNode["no"].IsNull()) {
			logger::error("No is missing for stage.");
            return settings;
		}
        const auto temp_no = stageNode["no"].as<StageNo>();
        // add to numbers
#ifndef NDEBUG
        logger::trace("Stage no: {}", temp_no);
#endif
        settings.numbers.push_back(temp_no);
        const auto temp_formeditorid = stageNode["FormEditorID"] && !stageNode["FormEditorID"].IsNull()
                                            ? stageNode["FormEditorID"].as<std::string>()
                                            : "";
        const FormID temp_formid = temp_formeditorid.empty()
                                        ? 0
                                        : GetFormEditorIDFromString(temp_formeditorid);
        if (!temp_formid && !temp_formeditorid.empty()) {
            logger::error("Formid could not be obtained for {}", temp_formid, temp_formeditorid);
            return {};
        }
        // add to items
#ifndef NDEBUG
        logger::trace("Formid");
#endif
        settings.items[temp_no] = temp_formid;
        // add to durations
#ifndef NDEBUG
        logger::trace("Duration");
#endif
        settings.durations[temp_no] = stageNode["duration"].as<Duration>();
        // add to stage_names
#ifndef NDEBUG
        logger::trace("Name");
#endif
        if (!stageNode["name"].IsNull()) {
            const auto temp_name = stageNode["name"].as<StageName>();
            // if it is empty, or just whitespace, set it to empty
            if (temp_name.empty() || std::ranges::all_of(temp_name, isspace))
				settings.stage_names[temp_no] = "";
			else settings.stage_names[temp_no] = stageNode["name"].as<StageName>();
        } else settings.stage_names[temp_no] = "";
        // add to costoverrides
#ifndef NDEBUG
        logger::trace("Cost");
#endif
        if (stageNode["value"] && !stageNode["value"].IsNull()) {
            settings.costoverrides[temp_no] = stageNode["value"].as<int>();
        } else settings.costoverrides[temp_no] = -1;
        // add to weightoverrides
#ifndef NDEBUG
        logger::trace("Weight");
#endif
        if (stageNode["weight"] && !stageNode["weight"].IsNull()) {
            settings.weightoverrides[temp_no] = stageNode["weight"].as<float>();
        } else settings.weightoverrides[temp_no] = -1.0f;
            
        // add to crafting_allowed
#ifndef NDEBUG
        logger::trace("Crafting");
#endif
        if (stageNode["crafting_allowed"] && !stageNode["crafting_allowed"].IsNull()){
            settings.crafting_allowed[temp_no] = stageNode["crafting_allowed"].as<bool>();
        } else settings.crafting_allowed[temp_no] = false;


        // add to effects
#ifndef NDEBUG
        logger::trace("Effects");
#endif
        std::vector<StageEffect> effects;
        if (!stageNode["mgeffect"] || stageNode["mgeffect"].size() == 0) {
#ifndef NDEBUG
			logger::trace("Effects are empty. Skipping.");
#endif
        } else {
            for (const auto& effectNode : stageNode["mgeffect"]) {
                const auto temp_effect_formeditorid =
                    effectNode["FormEditorID"] && !effectNode["FormEditorID"].IsNull() ? effectNode["FormEditorID"].as<std::string>() : "";
                const FormID temp_effect_formid =
                    temp_effect_formeditorid.empty()
                        ? 0
                        : GetFormEditorIDFromString(temp_effect_formeditorid);
                if (temp_effect_formid>0){
                    const auto temp_magnitude = effectNode["magnitude"].as<float>();
                    const auto temp_duration = effectNode["duration"].as<DurationMGEFF>();
                    effects.emplace_back(temp_effect_formid, temp_magnitude, temp_duration);
                } else effects.emplace_back(temp_effect_formid, 0.f, 0);
                // currently only one allowed
                break;
            }
        }
        settings.effects[temp_no] = effects;

		// add to colors
#ifndef NDEBUG
		logger::trace("Color");
#endif
		if (stageNode["color"] && !stageNode["color"].IsNull()) {
			settings.colors[temp_no] = std::stoul(stageNode["color"].as<std::string>(), nullptr, 16);
		}
		else settings.colors[temp_no] = 0;

		// add to sounds
#ifndef NDEBUG
		logger::trace("Sound");
#endif
		if (stageNode["sound"] && !stageNode["sound"].IsNull()) {
			const auto sound_formid = GetFormEditorIDFromString(stageNode["sound"].as<std::string>());
			settings.sounds[temp_no] = sound_formid;
        }

		// add to art_objects
#ifndef NDEBUG
		logger::trace("ArtObject");
#endif
		if (stageNode["art_object"] && !stageNode["art_object"].IsNull()) {
			const auto art_formid = GetFormEditorIDFromString(stageNode["art_object"].as<std::string>());
			settings.artobjects[temp_no] = art_formid;
        }

		// add to effect_shaders
#ifndef NDEBUG
		logger::trace("EffectShader");
#endif
		if (stageNode["effect_shader"] && !stageNode["effect_shader"].IsNull()) {
			const auto shader_formid = GetFormEditorIDFromString(stageNode["effect_shader"].as<std::string>());
			settings.effect_shaders[temp_no] = shader_formid;
        }
    }
    // final formid
#ifndef NDEBUG
    logger::trace("terminal item");
#endif
    const FormID temp_decayed_id =
        config["finalFormEditorID"] && !config["finalFormEditorID"].IsNull()
			? GetFormEditorIDFromString(config["finalFormEditorID"].as<std::string>())
			: 0;
    if (!temp_decayed_id) {
        logger::error("Decayed id is 0.");
        return {};
    }
#ifndef NDEBUG
    logger::trace("Decayed id: {}", temp_decayed_id);
#endif
    settings.decayed_id = temp_decayed_id;

    // containers
#ifndef NDEBUG
    logger::trace("containers");
#endif
    if (config["containers"] && !config["containers"].IsNull()) {
		const auto temp_containers = config["containers"].IsScalar() ? std::vector{config["containers"].as<std::string>()} : config["containers"].as<std::vector<std::string>>();
		for (const auto& container : temp_containers) {
			if (const FormID temp_formid = GetFormEditorIDFromString(container)) {
				settings.containers.insert(temp_formid);
			}
		}
    } 

    // delayers
	constexpr int warnings_limit = 10;
    int n_warnings = 0;
#ifndef NDEBUG
    logger::trace("timeModulators");
#endif
    for (const auto& modulator : config["timeModulators"]) {
        const auto temp_formeditorid = modulator["FormEditorID"] && !modulator["FormEditorID"].IsNull()
                                            ? modulator["FormEditorID"].as<std::string>()
                                            : "";
        const FormID temp_formid = GetFormEditorIDFromString(temp_formeditorid);
		if (!temp_formid) {
			if (n_warnings < warnings_limit) {
				logger::warn("timeModulators: Formid is 0 for {}", temp_formeditorid);
			}
			n_warnings++;
			continue;
		}
        settings.delayers[temp_formid] = !modulator["magnitude"].IsNull() ? modulator["magnitude"].as<float>() : 1;
		settings.delayers_order.push_back(temp_formid);

        // colors
		if (modulator["color"] && !modulator["color"].IsNull()) {
			settings.delayer_colors[temp_formid] = std::stoul(modulator["color"].as<std::string>(), nullptr, 16);
        }

		// sounds
		if (modulator["sound"] && !modulator["sound"].IsNull()) {
			const auto sound_formid = GetFormEditorIDFromString(modulator["sound"].as<std::string>());
			settings.delayer_sounds[temp_formid] = sound_formid;
        }

		// art_objects
		if (modulator["art_object"] && !modulator["art_object"].IsNull()) {
			const auto art_formid = GetFormEditorIDFromString(modulator["art_object"].as<std::string>());
			settings.delayer_artobjects[temp_formid] = art_formid;
        }

		// effect_shaders
		if (modulator["effect_shader"] && !modulator["effect_shader"].IsNull()) {
			const auto shader_formid = GetFormEditorIDFromString(modulator["effect_shader"].as<std::string>());
			settings.delayer_effect_shaders[temp_formid] = shader_formid;
        }

		// containers
		if (modulator["containers"] && !modulator["containers"].IsNull()) {
			const auto temp_containers = modulator["containers"].IsScalar() ? std::vector{ modulator["containers"].as<std::string>() } : modulator["containers"].as<std::vector<std::string>>();
			for (const auto& container : temp_containers) {
				if (const FormID container_formid = GetFormEditorIDFromString(container)) {
					settings.delayer_containers[temp_formid].insert(container_formid);
				}
			}
        }

    }

	// transformers
	n_warnings = 0;
    for (const auto& transformer : config["transformers"]) {
        const auto temp_formeditorid = transformer["FormEditorID"] &&
                                        !transformer["FormEditorID"].IsNull() ? transformer["FormEditorID"].as<std::string>() : "";
        const FormID temp_formid = GetFormEditorIDFromString(temp_formeditorid);
		if (!temp_formid) {
			if (n_warnings < warnings_limit) {
				logger::warn("transformers: Formid is 0 for {}", temp_formeditorid);
			}
			n_warnings++;
			continue;
		}
        const auto temp_finalFormEditorID =
            transformer["finalFormEditorID"] &&
            !transformer["finalFormEditorID"].IsNull() ? transformer["finalFormEditorID"].as<std::string>() : "";
        const FormID temp_formid2 = GetFormEditorIDFromString(temp_finalFormEditorID);
        if (!transformer["duration"] || transformer["duration"].IsNull()) {
			logger::warn("Duration is missing.");
			continue;
		}
        const auto temp_duration = transformer["duration"].as<float>();
        std::vector<StageNo> allowed_stages;
        if (!transformer["allowed_stages"]) {
            // default is all stages
            for (const auto& key : settings.items | std::views::keys) {
                allowed_stages.push_back(key);
            }
        } 
        else if (transformer["allowed_stages"].IsScalar()) {
            allowed_stages.push_back(transformer["allowed_stages"].as<StageNo>());
        } 
        else allowed_stages = transformer["allowed_stages"].as<std::vector<StageNo>>();
        if (allowed_stages.empty()) {
            for (const auto& key : settings.items | std::views::keys) {
				allowed_stages.push_back(key);
			}
        }

        auto temp_tuple = std::make_tuple(temp_formid2, temp_duration, allowed_stages);
        settings.transformers[temp_formid] = temp_tuple;
		settings.transformers_order.push_back(temp_formid);

		// colors
		if (transformer["color"] && !transformer["color"].IsNull()) {
			settings.transformer_colors[temp_formid] = std::stoul(transformer["color"].as<std::string>(), nullptr, 16);
        }

		// sounds
		if (transformer["sound"] && !transformer["sound"].IsNull()) {
			const auto sound_formid = GetFormEditorIDFromString(transformer["sound"].as<std::string>());
			settings.transformer_sounds[temp_formid] = sound_formid;
        }

		// art_objects
		if (transformer["art_object"] && !transformer["art_object"].IsNull()) {
			const auto art_formid = GetFormEditorIDFromString(transformer["art_object"].as<std::string>());
			settings.transformer_artobjects[temp_formid] = art_formid;
        }

		// effect_shaders
		if (transformer["effect_shader"] && !transformer["effect_shader"].IsNull()) {
			const auto shader_formid = GetFormEditorIDFromString(transformer["effect_shader"].as<std::string>());
			settings.transformer_effect_shaders[temp_formid] = shader_formid;
        }

        // containers
		if (transformer["containers"] && !transformer["containers"].IsNull()) {
			const auto temp_containers = transformer["containers"].IsScalar() ? std::vector{ transformer["containers"].as<std::string>() } : transformer["containers"].as<std::vector<std::string>>();
			for (const auto& container : temp_containers) {
				if (const FormID container_formid = GetFormEditorIDFromString(container)) {
					settings.transformer_containers[temp_formid].insert(container_formid);
				}
            }
        }
        
    }
        
    if (!settings.CheckIntegrity()) logger::critical("Settings integrity check failed.");

    return settings;
}


DefaultSettings parseDefaults(const std::string& _type)
{ 
    const auto filename = "Data/SKSE/Plugins/AlchemyOfTime/" + _type + "/AoT_default" + _type + ".yml";

	// check if the file exists
	if (!std::filesystem::exists(filename)) {
		logger::warn("File does not exist: {}", filename);
		return {};
	}

	if (FileIsEmpty(filename)) {
		logger::info("File is empty: {}", filename);
		return {};
	}

    logger::info("Filename: {}", filename);
    const YAML::Node config = YAML::LoadFile(filename);
    auto temp_settings = parseDefaults_(config);
    if (!temp_settings.CheckIntegrity()) {
        logger::warn("parseDefaults: Settings integrity check failed for {}", _type);
    }
    return temp_settings;
}
CustomSettings parseCustoms(const std::string& _type)
{
    CustomSettings _custom_settings;
    const auto folder_path = "Data/SKSE/Plugins/AlchemyOfTime/" + _type + "/custom";
    std::filesystem::create_directories(folder_path);
        
    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".yml") {
            const auto filename = entry.path().string();
            processCustomFile(filename,_custom_settings);
        }
    }
    return _custom_settings;
}

CustomSettings parseCustomsParallel(const std::string& _type)
{
    CustomSettings combinedSettings;
    const auto folder_path = "Data/SKSE/Plugins/AlchemyOfTime/" + _type + "/custom";
    std::filesystem::create_directories(folder_path);

    // Gather all .yml filenames in the directory
    std::vector<std::string> filenames;
    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".yml") {
            filenames.push_back(entry.path().string());
        }
    }

    // Reserve space for futures
    std::vector<std::future<void>> futures;
    futures.reserve(filenames.size());

    // Create a thread pool with available hardware threads
    ThreadPool pool(numThreads);

    // Enqueue a task for each file. Each task will parse and merge its file.
    for (const auto& filename : filenames) {
        futures.emplace_back(
            pool.enqueue([filename, &combinedSettings]() {
                processCustomFile(filename, combinedSettings);
            })
        );
    }

    // Wait for all tasks to complete
    for (auto& fut : futures) {
        fut.get();
    }

    // Return the fully merged settings after all threads are done
    return combinedSettings;
}

std::unordered_map<FormID, AddOnSettings> parseAddOnsParallel(const std::string& _type)
{
	std::unordered_map<FormID, AddOnSettings> combinedSettings;
	const auto folder_path = "Data/SKSE/Plugins/AlchemyOfTime/" + _type + "/addon";
	std::filesystem::create_directories(folder_path);
	// Gather all .yml filenames in the directory
	std::vector<std::string> filenames;
	for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
		if (entry.is_regular_file() && entry.path().extension() == ".yml") {
			filenames.push_back(entry.path().string());
		}
	}
	std::vector<std::future<void>> futures;
	futures.reserve(filenames.size());
	ThreadPool pool(numThreads);
	for (const auto& filename : filenames) {
		futures.emplace_back(
			pool.enqueue([filename, &combinedSettings]() {
				processAddOnFile(filename, combinedSettings);
				})
		);
	}
	// Wait for all tasks to complete
	for (auto& fut : futures) {
		fut.get();
	}
	return combinedSettings;
}


void processCustomFile(const std::string& filename, CustomSettings& combinedSettings)
{
	CustomSettings fileResult;
    if (FileIsEmpty(filename)) {
		logger::info("File is empty: {}", filename);
		return;
    }

    YAML::Node config = YAML::LoadFile(filename);

    if (!config["ownerLists"]) {
		logger::warn("OwnerLists not found in {}", filename);
		return;
	}

    for (const auto& Node_ : config["ownerLists"]){
		if (!Node_["owners"]) {
			logger::warn("Owners not found in {}", filename);
			return;
		}
        // we have list of owners at each node or a scalar owner
        if (Node_["owners"].IsScalar()) {
            const auto ownerName = Node_["owners"].as<std::string>();
            if (auto temp_settings = parseDefaults_(Node_); temp_settings.CheckIntegrity()) {
                fileResult[std::vector{ownerName}] = temp_settings;
            }
		} 
        else {
			std::vector<std::string> owners;
            for (const auto& owner : Node_["owners"]) {
				owners.push_back(owner.as<std::string>());
			}

            if (auto temp_settings = parseDefaults_(Node_); temp_settings.CheckIntegrity()) {
                fileResult[owners] = temp_settings;
            }
		}
    }

    if (!fileResult.empty()) {
        std::lock_guard lock(g_settingsMutex);
		mergeCustomSettings(combinedSettings, fileResult);
    }
}

void processAddOnFile(const std::string& filename, std::unordered_map<FormID, AddOnSettings>& combinedSettings)
{
    logger::info("Parsing file: {}", filename);

	std::unordered_map<FormID, AddOnSettings> fileResult;

    if (FileIsEmpty(filename)) {
		logger::info("File is empty: {}", filename);
		return;
    }

    YAML::Node config = YAML::LoadFile(filename);

    if (!config["formsLists"] || config["formsLists"].IsNull()) {
		logger::warn("formsLists not found in {}", filename);
		return;
	}
	if (config["formsLists"].size() == 0) {
		logger::warn("formsLists is empty in {}", filename);
		return;
    }

    for (const auto& Node_ : config["formsLists"]){
		if (!Node_["forms"]) {
			logger::warn("Forms not found in {}", filename);
			return;
		}
        // we have list of owners at each node or a scalar owner
        if (Node_["forms"].IsScalar()) {
            const auto ownerName = Node_["forms"].as<std::string>();
            if (auto temp_settings = parseAddOns_(Node_); temp_settings.CheckIntegrity()) {
				if (const auto formID = GetFormEditorIDFromString(ownerName)) {
					fileResult[formID] = temp_settings;
				}
				else {
                    logger::error("Formid could not be obtained for {}", ownerName);
                }
            }
            else {
				logger::error("Settings integrity check failed for {}", ownerName);
            }
        } 
        else {
			std::set<FormID> owners;
            for (const auto& owner : Node_["forms"]) {
                if (const auto formID = GetFormEditorIDFromString(owner.as<std::string>())) {
                    logger::error("Formid: {}", formID);
                    owners.insert(formID);
                }
				else logger::error("Formid could not be obtained for {}", owner.as<std::string>());
			}

            if (auto temp_settings = parseAddOns_(Node_); temp_settings.CheckIntegrity()) {
				for (const auto owner : owners) {
					fileResult[owner] = temp_settings;
				}
			}
			else {
				logger::error("Settings integrity check failed for forms starting with {}", *owners.begin());
            }
		}
    }

	if (!fileResult.empty()) {
		std::lock_guard lock(g_settingsMutex);
		mergeAddOnSettings(combinedSettings, fileResult);
    }
}

void mergeCustomSettings(CustomSettings& dest, const CustomSettings& src) {
    for (const auto& [owners, settings] : src) {
        dest[owners] = settings;
    }
}

void mergeAddOnSettings(std::unordered_map<FormID, AddOnSettings>& dest, const std::unordered_map<FormID, AddOnSettings>& src)
{
	for (const auto& [formID, settings] : src) {
		dest[formID] = settings;
	}
}

void LoadINISettings()
{
    logger::info("Loading ini settings");


    CSimpleIniA ini;

    ini.SetUnicode();
    ini.LoadFile(Settings::INI_path);

    // if the section does not exist populate with default values
    for (const auto& [section, defaults] : Settings::InISections) {
        if (!ini.GetSection(section)) {
			for (const auto& [key, val] : defaults) {
				ini.SetBoolValue(section, key, val);
                Settings::INI_settings[section][key] = val;
			}
		}
        // it exist now check if we have values for all keys
        else {
			for (const auto& [key, val] : defaults) {
                if (const auto temp_bool = ini.GetBoolValue(section, key, val); temp_bool == val) {
					ini.SetBoolValue(section, key, val);
					Settings::INI_settings[section][key] = val;
				} else Settings::INI_settings[section][key] = temp_bool;
			}
        }
	}
    if (!ini.KeyExists("Other Settings", "nMaxInstancesInThousands")) {
        ini.SetLongValue("Other Settings", "nMaxInstancesInThousands", 200);
        Settings::nMaxInstances = 200000;
    } else Settings::nMaxInstances = 1000 * ini.GetLongValue("Other Settings", "nMaxInstancesInThousands", 200);

    Settings::nMaxInstances = std::min(Settings::nMaxInstances, 2000000);

    if (!ini.KeyExists("Other Settings", "nTimeToForgetInDays")) {
        ini.SetLongValue("Other Settings", "nTimeToForgetInDays", 90);
        Settings::nForgettingTime = 2160;
    } else Settings::nForgettingTime = 24 * ini.GetLongValue("Other Settings", "nTimeToForgetInDays", 90);

    Settings::nForgettingTime = std::min(Settings::nForgettingTime, 4320);

    Settings::disable_warnings = ini.GetBoolValue("Other Settings", "DisableWarnings", Settings::disable_warnings);
    Settings::world_objects_evolve = ini.GetBoolValue("Other Settings", "WorldObjectsEvolve", Settings::world_objects_evolve);
	Settings::placed_objects_evolve = ini.GetBoolValue("Other Settings", "PlacedObjectsEvolve", Settings::placed_objects_evolve);
	Settings::unowned_objects_evolve = ini.GetBoolValue("Other Settings", "UnOwnedObjectsEvolve", Settings::unowned_objects_evolve);
		
    ini.SaveFile(Settings::INI_path);
}

void LoadJSONSettings()
{
	logger::info("Loading json settings.");
	std::ifstream ifs(Settings::json_path);
	if (!ifs.is_open()) {
		logger::info("Failed to open file for reading: {}", Settings::json_path);
        SaveSettings();
		return;
	}
	rapidjson::IStreamWrapper isw(ifs);
	rapidjson::Document doc;
	doc.ParseStream(isw);
	if (doc.HasParseError()) {
		logger::error("Failed to parse json file: {}", Settings::json_path);
		return;
	}
	if (!doc.HasMember("ticker")) {
		logger::error("Ticker not found in json file: {}", Settings::json_path);
		return;
	}
	const auto& ticker = doc["ticker"];
	if (!ticker.HasMember("speed")) {
		logger::error("Speed not found in ticker.");
		return;
	}
	Settings::ticker_speed = Settings::Ticker::from_string(ticker["speed"].GetString());
}

void LoadSettings() {
    logger::info("Loading settings.");
    try {
        LoadINISettings();
    } catch (const std::exception& ex) {
		logger::critical("Failed to load ini settings: {}", ex.what());
		Settings::failed_to_load = true;
		return;
	}
    if (!Settings::INI_settings.contains("Modules")) {
        logger::critical("Modules section not found in ini settings.");
		Settings::failed_to_load = true;
		return;
	}
    for (const auto& [key,val]: Settings::INI_settings["Modules"]) {
        if (val) Settings::QFORMS.push_back(key);
	}

    for (const auto& _qftype: Settings::QFORMS) {
        try {
            logger::info("Loading defaultsettings for {}", _qftype);
			if (auto temp_default_settings = parseDefaults(_qftype); !temp_default_settings.IsEmpty()) {
                Settings::defaultsettings[_qftype] = temp_default_settings;
			}
        } catch (const std::exception& ex) {
            logger::critical("Failed to load default settings for {}: {}", _qftype, ex.what());
            Settings::failed_to_load = true;
            return;
        }
        try {
            logger::info("Loading custom settings for {}", _qftype);
			if (auto temp_custom_settings = parseCustoms(_qftype); !temp_custom_settings.empty()) {
                Settings::custom_settings[_qftype] = temp_custom_settings;
			}
        } catch (const std::exception& ex) {
			logger::critical("Failed to load custom settings for {}: {}", _qftype, ex.what());
			Settings::failed_to_load = true;
            return;
        }
        try {
			logger::info("Loading exclude list for {}", _qftype);
            Settings::exclude_list[_qftype] = LoadExcludeList(_qftype);
        } catch (const std::exception& ex) {
			logger::critical("Failed to load exclude list for {}: {}", _qftype, ex.what());
            Settings::failed_to_load = true;
            return;
        }
		try {
			logger::info("Loading addons for {}", _qftype);
		    Settings::addon_settings[_qftype] = parseAddOns(_qftype);
		}
		catch (const std::exception& ex) {
			logger::critical("Failed to load addons for {}: {}", _qftype, ex.what());
			Settings::failed_to_load = true;
			return;
		}
	}

	try {
		LoadJSONSettings();
	}
	catch (const std::exception& ex) {
		logger::critical("Failed to load json settings: {}", ex.what());
		Settings::failed_to_load = true;
		return;
	}
}

void LoadSettingsParallel()
{
    logger::info("Loading settings.");
    try {
        LoadINISettings();
    } catch (const std::exception& ex) {
		logger::critical("Failed to load ini settings: {}", ex.what());
		Settings::failed_to_load = true;
		return;
	}
    if (!Settings::INI_settings.contains("Modules")) {
        logger::critical("Modules section not found in ini settings.");
		Settings::failed_to_load = true;
		return;
	}
    for (const auto& [key,val]: Settings::INI_settings["Modules"]) {
        if (val) Settings::QFORMS.push_back(key);
	}

    std::vector<std::future<void>> typeFutures;
	typeFutures.reserve(Settings::QFORMS.size());
	ThreadPool typePool(numThreads);

    std::mutex defaultsettingsMutex;
	std::mutex customsettingsMutex;
	std::mutex excludeListMutex;
	std::mutex addonsettingsMutex;


    for (const auto& _qftype: Settings::QFORMS) {
        typeFutures.push_back(typePool.enqueue([_qftype,&defaultsettingsMutex,&customsettingsMutex,&excludeListMutex,&addonsettingsMutex]() {
                    try {
                        logger::info("Loading defaultsettings for {}", _qftype);
			            if (auto temp_default_settings = parseDefaults(_qftype); !temp_default_settings.IsEmpty()) {
							std::lock_guard lock(defaultsettingsMutex);
                            Settings::defaultsettings[_qftype] = temp_default_settings;
			            }
                    } catch (const std::exception& ex) {
                        logger::critical("Failed to load default settings for {}: {}", _qftype, ex.what());
                        Settings::failed_to_load = true;
                        return;
                    }
                    try {
                        logger::info("Loading custom settings for {}", _qftype);
			            if (const auto temp_custom_settings = parseCustomsParallel(_qftype); !temp_custom_settings.empty()) {
							std::lock_guard lock(customsettingsMutex);
                            Settings::custom_settings[_qftype] = temp_custom_settings;
			            }
                    } catch (const std::exception& ex) {
			            logger::critical("Failed to load custom settings for {}: {}", _qftype, ex.what());
			            Settings::failed_to_load = true;
                        return;
                    }
                    try {
			            logger::info("Loading exclude list for {}", _qftype);
						std::lock_guard lock(excludeListMutex);
                        Settings::exclude_list[_qftype] = LoadExcludeList(_qftype);
                    } catch (const std::exception& ex) {
			            logger::critical("Failed to load exclude list for {}: {}", _qftype, ex.what());
                        Settings::failed_to_load = true;
                        return;
                    }
		            try {
			            logger::info("Loading addons for {}", _qftype);
						std::lock_guard lock(addonsettingsMutex);
		                Settings::addon_settings[_qftype] = parseAddOnsParallel(_qftype);
		            }
		            catch (const std::exception& ex) {
			            logger::critical("Failed to load addons for {}: {}", _qftype, ex.what());
			            Settings::failed_to_load = true;
			            return;
		            }
			    }
            )
		);
	}

	for (auto& fut : typeFutures) {
		fut.get();
	}


	try {
		LoadJSONSettings();
	}
	catch (const std::exception& ex) {
		logger::critical("Failed to load json settings: {}", ex.what());
		Settings::failed_to_load = true;
		return;
	}

}


rapidjson::Value Settings::Ticker::to_json(rapidjson::Document::AllocatorType& a)
{
	using namespace rapidjson;
    Value ticker(kObjectType);
    const std::string speed_str = Ticker::to_string(ticker_speed);
    Value speed_value(speed_str.c_str(), a);
    ticker.AddMember("speed", speed_value, a); 
    return ticker;
}
