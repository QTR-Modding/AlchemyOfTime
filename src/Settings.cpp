#include "Settings.h"
#include "SimpleIni.h"
#include "Threading.h"
#include "CLibUtilsQTR/PresetHelpers/PresetHelpersTXT.hpp"
#include "CLibUtilsQTR/PresetHelpers/PresetHelpersYAML.hpp"
#include "CLibUtilsQTR/StringHelpers.hpp"
#include "Lorebox.h"
#include "rapidjson/stringbuffer.h"
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/writer.h>

using QFormChecker = bool(*)(const RE::TESForm*);

static const std::unordered_map<std::string, QFormChecker> qformCheckers = {
    // POPULATE THIS
    {"FOOD", [](const auto form) { return IsFoodItem(form); }},
    {"INGR", [](const auto form) { return form->Is(RE::IngredientItem::FORMTYPE); }},
    {"MEDC", [](const auto form) { return IsMedicineItem(form); }},
    {"POSN", [](const auto form) { return IsPoisonItem(form); }},
    {"ARMO", [](const auto form) { return form->Is(RE::TESObjectARMO::FORMTYPE); }},
    {"WEAP", [](const auto form) { return form->Is(RE::TESObjectWEAP::FORMTYPE); }},
    {"SCRL", [](const auto form) { return form->Is(RE::ScrollItem::FORMTYPE); }},
    {"BOOK", [](const auto form) { return form->Is(RE::TESObjectBOOK::FORMTYPE); }},
    {"SLGM", [](const auto form) { return form->Is(RE::TESSoulGem::FORMTYPE); }},
    {"MISC", [](const auto form) { return form->Is(RE::TESObjectMISC::FORMTYPE); }},
    //{"NPC", [](const auto form) {return FormIsOfType(form, RE::TESNPC::FORMTYPE); } }
};

bool Settings::IsQFormType(const FormID formid, const std::string& qformtype) {
    const auto* form = FormReader::GetFormByID(formid);
    if (!form) {
        logger::warn("IsQFormType: Form not found.");
        return false;
    }
    const auto it = qformCheckers.find(qformtype);
    return it != qformCheckers.end() ? it->second(form) : false;
}

std::string Settings::GetQFormType(const FormID formid) {
    for (const auto& q_ftype : QFORMS) {
        if (IsQFormType(formid, q_ftype)) return q_ftype;
    }
    return "";
}

bool Settings::IsSpecialQForm(RE::TESObjectREFR* ref) {
    const auto base = ref->GetBaseObject();
    if (!base) return false;
    const auto qform_type = GetQFormType(base->GetFormID());
    if (qform_type.empty()) return false;
    return std::ranges::any_of(sQFORMS, [qform_type](const std::string& qformtype) { return qformtype == qform_type; });
}

bool Settings::IsInExclude(const FormID formid, std::string type) {
    const auto form = FormReader::GetFormByID(formid);
    if (!form) {
        logger::warn("Form not found.");
        return false;
    }

    if (type.empty()) type = GetQFormType(formid);
    if (type.empty()) {
        return false;
    }
    if (!exclude_list.contains(type)) {
        logger::critical("Type not found in exclude list. for formid: {}", formid);
        return false;
    }

    const auto form_string = std::string(form->GetName());

    if (const std::string form_editorid = clib_util::editorID::get_editorID(form);
        !form_editorid.empty() && StringHelpers::includesWord(form_editorid, exclude_list[type])) {
        return true;
    }

    /*const auto exlude_list = LoadExcludeList(postfix);*/
    if (StringHelpers::includesWord(form_string, exclude_list[type])) {
        return true;
    }
    return false;
}

void Settings::AddToExclude(const std::string& entry_name, const std::string& type, const std::string& filename) {
    if (entry_name.empty() || type.empty() || filename.empty()) {
        logger::warn("Empty entry_name, type or filename.");
        return;
    }
    // check if type is a valid qformtype
    if (!std::ranges::any_of(QFORMS, [type](const std::string& qformtype) { return qformtype == type; })) {
        logger::warn("AddToExclude: Invalid type: {}", type);
        return;
    }

    const auto folder_path = "Data/SKSE/Plugins/AlchemyOfTime/" + type + "/exclude";
    std::filesystem::create_directories(folder_path);
    const auto file_path = folder_path + "/" + filename + ".txt";
    // check if the entry is already in the list
    if (std::ranges::find(exclude_list[type], entry_name) != exclude_list[type].end()) {
        logger::warn("Entry already in exclude list: {}", entry_name);
        return;
    }
    std::ofstream file(file_path, std::ios::app);
    file << entry_name << '\n';
    file.close();
    exclude_list[type].push_back(entry_name);
}

bool Settings::IsItem(const FormID formid, const std::string& type, const bool check_exclude) {
    if (!formid) return false;
    if (check_exclude && IsInExclude(formid, type)) return false;
    if (type.empty()) return !GetQFormType(formid).empty();
    return IsQFormType(formid, type);
}

bool Settings::IsItem(const RE::TESObjectREFR* ref, const std::string& type) {
    const auto base = ref->GetBaseObject();
    if (!base) return false;
    return IsItem(base->GetFormID(), type);
}

DefaultSettings* Settings::GetDefaultSetting(const FormID form_id) {
    const auto qform_type = GetQFormType(form_id);

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

DefaultSettings* Settings::GetCustomSetting(const RE::TESForm* form) {
    const auto form_id = form->GetFormID();
    const auto qform_type = GetQFormType(form_id);
    if (!qform_type.empty() && custom_settings.contains(qform_type)) {
        for (auto& customSetting = custom_settings[qform_type]; auto& [names, sttng] : customSetting) {
            if (!sttng.IsHealthy()) continue;
            for (auto& name : names) {
                if (const FormID temp_cstm_formid = FormReader::GetFormEditorIDFromString(name);
                    temp_cstm_formid > 0) {
                    if (const auto temp_cstm_form = FormReader::GetFormByID(temp_cstm_formid, name);
                        temp_cstm_form && temp_cstm_form->GetFormID() == form_id) {
                        return &sttng;
                    }
                }
            }
            if (StringHelpers::includesWord(form->GetName(), names)) {
                return &sttng;
            }
        }
    }
    return nullptr;
}

AddOnSettings* Settings::GetAddOnSettings(const RE::TESForm* form) {
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

void PresetParse::SaveSettings() {
    using namespace rapidjson;
    Document doc;
    doc.SetObject();

    Document::AllocatorType& allocator = doc.GetAllocator();

    Value version(kObjectType);
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


std::vector<std::string> PresetParse::LoadExcludeList(const std::string& postfix) // NOLINT(misc-use-internal-linkage)
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

namespace {
    constexpr int warnings_limit = 5;

    void mergeCustomSettings(CustomSettings& dest, const CustomSettings& src) {
        for (const auto& [owners, settings] : src) {
            dest[owners] = settings;
        }
    }

    void processCustomFile(const std::string& filename, CustomSettings& combinedSettings) {
        logger::info("Parsing file: {}", filename);
        // Create a temporary result for this file
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

        for (const auto& Node_ : config["ownerLists"]) {
            if (!Node_["owners"]) {
                logger::warn("Owners not found in {}", filename);
                return;
            }
            // we have list of owners at each node or a scalar owner
            if (auto temp_settings = PresetParse::parseDefaults_(Node_); temp_settings.CheckIntegrity()) {
                std::vector<std::string> owners = PresetHelpers::YAML_Helpers::CollectFrom<
                    std::string>(Node_, "owners");
                fileResult[owners] = temp_settings;
            }
        }

        if (!fileResult.empty()) {
            std::lock_guard lock(PresetParse::g_settingsMutex);
            mergeCustomSettings(combinedSettings, fileResult);
        }
    }

    CustomSettings parseCustomsParallel(const std::string& _type) {
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

    void mergeAddOnSettings(std::unordered_map<FormID, AddOnSettings>& dest,
                            const std::unordered_map<FormID, AddOnSettings>& src) {
        for (const auto& [formID, settings] : src) {
            dest[formID] = settings;
        }
    }

    void processAddOnFile(const std::string& filename, std::unordered_map<FormID, AddOnSettings>& combinedSettings) {
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

        for (const auto& Node_ : config["formsLists"]) {
            if (!Node_["forms"] || Node_["forms"].IsNull()) {
                logger::warn("Forms not found in {}", filename);
                return;
            }
            // we have list of owners at each node or a scalar owner
            if (auto temp_settings = PresetParse::parseAddOns_(Node_); temp_settings.CheckIntegrity()) {
                for (const auto owner : PresetHelpers::YAML_Helpers::CollectFrom<FormID, std::string>(Node_, "forms")) {
                    fileResult[owner] = temp_settings;
                }
            } else {
                logger::error("Settings integrity check failed for forms starting with {}",
                              Node_["forms"].IsScalar()
                                  ? Node_["forms"].as<std::string>()
                                  : Node_["forms"].begin()->as<std::string>());
            }
        }

        if (!fileResult.empty()) {
            std::lock_guard lock(PresetParse::g_settingsMutex);
            mergeAddOnSettings(combinedSettings, fileResult);
        }
    }

    std::unordered_map<FormID, AddOnSettings> parseAddOnsParallel(const std::string& _type) {
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

    auto parse_color = [](const YAML::Node& node, const char* key) -> std::optional<uint32_t> {
        if (node[key] && !node[key].IsNull())
            return std::stoul(node[key].as<std::string>(), nullptr, 16);
        return std::nullopt;
    };

    auto parse_formid = [](const YAML::Node& node, const char* key) -> std::optional<FormID> {
        if (node[key] && !node[key].IsNull()) {
            const auto formid_str = node[key].as<std::string>();
            if (const FormID formid = FormReader::GetFormEditorIDFromString(formid_str); formid > 0) {
                return formid;
            }
            logger::warn("parse_formid: Invalid FormID for key '{}': {}", key, formid_str);
        }
        return std::nullopt;
    };

    auto parse_formid_vec = [](const YAML::Node& node, const char* key) -> std::vector<FormID> {
        if (node[key] && !node[key].IsNull()) {
            return PresetHelpers::YAML_Helpers::CollectFrom<FormID, std::string>(node, key);
        }
        return {};
    };

    template <typename T>
    std::optional<T> parse_type(const YAML::Node& node, const char* key) {
        if (node[key] && !node[key].IsNull()) {
            return node[key].as<T>();
        }
        return std::nullopt;
    }
};

AddOnSettings PresetParse::parseAddOns_(const YAML::Node& config) {
    AddOnSettings settings;

    // containers
    if (config["containers"] && !config["containers"].IsNull()) {
        auto containers = parse_formid_vec(config, "containers");
        settings.containers.insert(containers.begin(), containers.end());
    }

    // delayers
    int n_warnings = 0;
    if (config["timeModulators"] && !config["timeModulators"].IsNull()) {
        for (const auto& modulator : config["timeModulators"]) {
            if (!modulator["FormEditorID"] || modulator["FormEditorID"].IsNull()) {
                if (n_warnings < warnings_limit) {
                    logger::warn("timeModulators: FormEditorID field has error.");
                    n_warnings++;
                }
                continue;
            }
            // allowed_stages
            std::vector<StageNo> a_asv;
            if (modulator["allowed_stages"] && !modulator["allowed_stages"].IsNull()) {
                a_asv = PresetHelpers::YAML_Helpers::CollectFrom<StageNo>(modulator, "allowed_stages");
            }
            // delayer magnitude
            const auto delayer_magnitude = !modulator["magnitude"].IsNull() ? modulator["magnitude"].as<float>() : 1.f;
            // colors
            auto a_color = parse_color(modulator, "color");
            // sounds
            auto a_sound = parse_formid(modulator, "sound");
            // art_objects
            auto a_art_object = parse_formid(modulator, "art_object");
            // effect_shaders
            auto a_effect_shader = parse_formid(modulator, "effect_shader");
            // containers
            auto containers = parse_formid_vec(modulator, "containers");

            for (auto a_formid : parse_formid_vec(modulator, "FormEditorID")) {
                settings.delayer_allowed_stages[a_formid] = std::unordered_set(a_asv.begin(), a_asv.end());
                settings.delayers[a_formid] = delayer_magnitude;
                // delayer (order)
                settings.delayers_order.insert(a_formid);
                if (a_color) settings.delayer_colors[a_formid] = *a_color;
                if (a_sound) settings.delayer_sounds[a_formid] = *a_sound;
                if (a_art_object) settings.delayer_artobjects[a_formid] = *a_art_object;
                if (a_effect_shader) settings.delayer_effect_shaders[a_formid] = *a_effect_shader;
                settings.delayer_containers[a_formid].insert(containers.begin(), containers.end());
            }
        }
    }

    // transformers
    n_warnings = 0;
    if (config["transformers"] && !config["transformers"].IsNull()) {
        for (const auto& transformer : config["transformers"]) {
            FormID a_formid2;

            if (auto temp_finalFormEditorID = parse_formid(transformer, "finalFormEditorID"); !temp_finalFormEditorID) {
                if (n_warnings < warnings_limit) {
                    logger::warn("transformers: Final FormEditorID is missing.");
                }
                n_warnings++;
                continue;
            } else {
                a_formid2 = *temp_finalFormEditorID;
            }

            if (!transformer["FormEditorID"] || transformer["FormEditorID"].IsNull()) {
                if (n_warnings < warnings_limit) {
                    logger::warn("transformers: FormEditorID field has error.");
                    n_warnings++;
                }
                continue;
            }

            // duration
            float a_duration;
            if (auto temp_duration = parse_type<float>(transformer, "duration"); !temp_duration) {
                if (n_warnings < warnings_limit) {
                    logger::warn("transformers: Duration is missing");
                    n_warnings++;
                }
                continue;
            } else {
                a_duration = *temp_duration;
            }
            // allowed_stages
            std::vector<StageNo> a_asv;
            if (transformer["allowed_stages"] && !transformer["allowed_stages"].IsNull()) {
                a_asv = PresetHelpers::YAML_Helpers::CollectFrom<StageNo>(transformer, "allowed_stages");
            }
            // colors
            auto a_color = parse_color(transformer, "color");
            // sounds
            auto a_sound = parse_formid(transformer, "sound");
            // art_objects
            auto a_art_object = parse_formid(transformer, "art_object");
            // effect_shaders
            auto a_effect_shader = parse_formid(transformer, "effect_shader");
            // containers
            auto containers = parse_formid_vec(transformer, "containers");

            for (auto a_formid : parse_formid_vec(transformer, "FormEditorID")) {
                settings.transformers[a_formid] = {a_formid2, a_duration};
                // transformers (order)
                settings.transformers_order.insert(a_formid);
                settings.transformer_allowed_stages[a_formid] = std::unordered_set(a_asv.begin(), a_asv.end());
                if (a_color) settings.transformer_colors[a_formid] = *a_color;
                if (a_sound) settings.transformer_sounds[a_formid] = *a_sound;
                if (a_art_object) settings.transformer_artobjects[a_formid] = *a_art_object;
                if (a_effect_shader) settings.transformer_effect_shaders[a_formid] = *a_effect_shader;
                settings.transformer_containers[a_formid].insert(containers.begin(), containers.end());
            }
        }
    }

    if (!settings.CheckIntegrity()) logger::critical("Settings integrity check failed.");

    return settings;
}

DefaultSettings PresetParse::parseDefaults_(const YAML::Node& config) {
    DefaultSettings settings = {};

    if (!config["stages"] || config["stages"].size() == 0) {
        logger::error("Stages are empty.");
        return settings;
    }

    for (const auto& stageNode : config["stages"]) {
        if (!stageNode["no"] || stageNode["no"].IsNull()) {
            logger::error("No is missing for stage.");
            return settings;
        }
        const auto a_stage_no = stageNode["no"].as<StageNo>();

        // add to numbers
        settings.numbers.push_back(a_stage_no);
        const auto temp_formeditorid = stageNode["FormEditorID"] && !stageNode["FormEditorID"].IsNull()
                                           ? stageNode["FormEditorID"].as<std::string>()
                                           : "";
        const FormID temp_formid = temp_formeditorid.empty()
                                       ? 0
                                       : FormReader::GetFormEditorIDFromString(temp_formeditorid);
        if (!temp_formid && !temp_formeditorid.empty()) {
            logger::error("Formid could not be obtained for {}", temp_formid, temp_formeditorid);
            return {};
        }
        // add to items
        settings.items[a_stage_no] = temp_formid;
        // add to durations
        settings.durations[a_stage_no] = stageNode["duration"].as<Duration>();
        // add to stage_names
        if (!stageNode["name"].IsNull()) {
            // if it is empty, or just whitespace, set it to empty
            if (const auto temp_name = stageNode["name"].as<StageName>();
                temp_name.empty() || std::ranges::all_of(temp_name, isspace)) {
                settings.stage_names[a_stage_no] = "";
            } else settings.stage_names[a_stage_no] = temp_name;
        } else settings.stage_names[a_stage_no] = "";
        // add to costoverrides
        if (auto a_value = parse_type<int>(stageNode, "value"); a_value) {
            settings.costoverrides[a_stage_no] = *a_value;
        } else settings.costoverrides[a_stage_no] = -1;
        // add to weightoverrides
        if (auto a_weight = parse_type<float>(stageNode, "weight"); a_weight) {
            settings.weightoverrides[a_stage_no] = *a_weight;
        } else settings.weightoverrides[a_stage_no] = -1.0f;

        // add to crafting_allowed
        if (auto crafting_allowed = parse_type<bool>(stageNode, "crafting_allowed"); crafting_allowed) {
            settings.crafting_allowed[a_stage_no] = *crafting_allowed;
        } else settings.crafting_allowed[a_stage_no] = false;

        // add to effects
        std::vector<StageEffect> effects;
        if (!stageNode["mgeffect"] || stageNode["mgeffect"].size() == 0) {
        } else {
            for (const auto& effectNode : stageNode["mgeffect"]) {
                const auto temp_effect_formeditorid =
                    effectNode["FormEditorID"] && !effectNode["FormEditorID"].IsNull()
                        ? effectNode["FormEditorID"].as<std::string>()
                        : "";
                const FormID temp_effect_formid =
                    temp_effect_formeditorid.empty()
                        ? 0
                        : FormReader::GetFormEditorIDFromString(temp_effect_formeditorid);
                if (temp_effect_formid > 0) {
                    const auto temp_magnitude = effectNode["magnitude"].as<float>();
                    const auto temp_duration = effectNode["duration"].as<DurationMGEFF>();
                    effects.emplace_back(temp_effect_formid, temp_magnitude, temp_duration);
                } else effects.emplace_back(temp_effect_formid, 0.f, 0);
                // currently only one allowed
                break;
            }
        }
        settings.effects[a_stage_no] = effects;

        // add to colors
        if (auto a_color = parse_color(stageNode, "color"); a_color) {
            settings.colors[a_stage_no] = *a_color;
        } else settings.colors[a_stage_no] = 0;
        // add to sounds
        if (auto a_sound = parse_formid(stageNode, "sound"); a_sound) {
            settings.sounds[a_stage_no] = *a_sound;
        }
        // add to art_objects
        if (auto a_art_object = parse_formid(stageNode, "art_object"); a_art_object) {
            settings.artobjects[a_stage_no] = *a_art_object;
        }
        // add to effect_shaders
        if (auto a_effect_shader = parse_formid(stageNode, "effect_shader"); a_effect_shader) {
            settings.effect_shaders[a_stage_no] = *a_effect_shader;
        }
    }
    // final formid
    const FormID temp_decayed_id =
        config["finalFormEditorID"] && !config["finalFormEditorID"].IsNull()
            ? FormReader::GetFormEditorIDFromString(config["finalFormEditorID"].as<std::string>())
            : 0;
    if (!temp_decayed_id) {
        logger::error("Decayed id is 0.");
        return {};
    }
    settings.decayed_id = temp_decayed_id;

    const auto addons = parseAddOns_(config);
    // containers
    settings.containers = addons.containers;
    // delayers
    settings.delayers = addons.delayers;
    settings.delayer_allowed_stages = addons.delayer_allowed_stages;
    settings.delayer_colors = addons.delayer_colors;
    settings.delayer_sounds = addons.delayer_sounds;
    settings.delayer_artobjects = addons.delayer_artobjects;
    settings.delayer_effect_shaders = addons.delayer_effect_shaders;
    settings.delayer_containers = addons.delayer_containers;
    // delayers_order
    settings.delayers_order = addons.delayers_order;
    // transformers
    settings.transformers = addons.transformers;
    settings.transformer_allowed_stages = addons.transformer_allowed_stages;
    settings.transformer_colors = addons.transformer_colors;
    settings.transformer_sounds = addons.transformer_sounds;
    settings.transformer_artobjects = addons.transformer_artobjects;
    settings.transformer_effect_shaders = addons.transformer_effect_shaders;
    settings.transformer_containers = addons.transformer_containers;
    // transformers_order
    settings.transformers_order = addons.transformers_order;

    // loop delayers
    for (const auto& a_formid : settings.delayers_order) {
        if (settings.delayer_allowed_stages.contains(a_formid)) {
            if (settings.delayer_allowed_stages.at(a_formid).empty()) {
                settings.delayer_allowed_stages.at(a_formid) = std::unordered_set(
                    settings.numbers.begin(), settings.numbers.end());
            }
        } else {
            settings.delayer_allowed_stages[a_formid] = std::unordered_set(
                settings.numbers.begin(), settings.numbers.end());
        }
    }

    // loop transformers
    for (const auto& a_formid : settings.transformers_order) {
        if (settings.transformer_allowed_stages.contains(a_formid)) {
            if (settings.transformer_allowed_stages.at(a_formid).empty()) {
                settings.transformer_allowed_stages.at(a_formid) = std::unordered_set(
                    settings.numbers.begin(), settings.numbers.end());
            }
        } else {
            settings.transformer_allowed_stages[a_formid] = std::unordered_set(
                settings.numbers.begin(), settings.numbers.end());
        }
    }

    if (!settings.CheckIntegrity()) logger::critical("Settings integrity check failed.");

    return settings;
}

DefaultSettings PresetParse::parseDefaults(const std::string& _type) {
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


void PresetParse::LoadINISettings() {
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
    Settings::world_objects_evolve = ini.GetBoolValue("Other Settings", "WorldObjectsEvolve",
                                                      Settings::world_objects_evolve);
    Settings::placed_objects_evolve = ini.GetBoolValue("Other Settings", "PlacedObjectsEvolve",
                                                       Settings::placed_objects_evolve);
    Settings::unowned_objects_evolve = ini.GetBoolValue("Other Settings", "UnOwnedObjectsEvolve",
                                                        Settings::unowned_objects_evolve);

    // LoreBox settings (defaults true, except ShowModulatorName and ShowMultiplier)
    const bool lb_title = ini.GetBoolValue("LoreBox", "ShowTitle", true);
    const bool lb_pct = ini.GetBoolValue("LoreBox", "ShowPercentage", true);
    const bool lb_mod = ini.GetBoolValue("LoreBox", "ShowModulatorName", false);
    const bool lb_col = ini.GetBoolValue("LoreBox", "ColorizeRows", true);
    const bool lb_mul = ini.GetBoolValue("LoreBox", "ShowMultiplier", false);
    Lorebox::show_title.store(lb_title);
    Lorebox::show_percentage.store(lb_pct);
    Lorebox::show_modulator_name.store(lb_mod);
    Lorebox::colorize_rows.store(lb_col);
    Lorebox::show_multiplier.store(lb_mul);

    // Colors
    auto readHex = [&](const char* key, const uint32_t def) {
        const char* s = ini.GetValue("LoreBox", key, nullptr);
        if (!s) return def;
        try {
            return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
        } catch (...) {
            return def;
        }
    };

    Lorebox::color_title.store(readHex("ColorTitle", Lorebox::color_title.load()));
    Lorebox::color_neutral.store(readHex("ColorNeutral", Lorebox::color_neutral.load()));
    Lorebox::color_slow.store(readHex("ColorSlow", Lorebox::color_slow.load()));
    Lorebox::color_fast.store(readHex("ColorFast", Lorebox::color_fast.load()));
    Lorebox::color_transform.store(readHex("ColorTransform", Lorebox::color_transform.load()));
    Lorebox::color_separator.store(readHex("ColorSeparator", Lorebox::color_separator.load()));

    // Symbols (allow raw ASCII, HTML entities, or backslash-escapes)
    if (const char* sep = ini.GetValue("LoreBox", "SeparatorSymbol", nullptr)) {
        if (const auto ws = String::DecodeEscapesFromAscii(sep); !ws.empty()) Lorebox::separator_symbol = ws;
    }
    if (const char* ar = ini.GetValue("LoreBox", "ArrowRight", nullptr)) {
        if (const auto ws = String::DecodeEscapesFromAscii(ar); !ws.empty()) Lorebox::arrow_right = ws;
    }
    if (const char* al = ini.GetValue("LoreBox", "ArrowLeft", nullptr)) {
        if (const auto ws = String::DecodeEscapesFromAscii(al); !ws.empty()) Lorebox::arrow_left = ws;
    }

    // Ensure keys exist with defaults if they were missing
    ini.SetBoolValue("LoreBox", "ShowTitle", lb_title);
    ini.SetBoolValue("LoreBox", "ShowPercentage", lb_pct);
    ini.SetBoolValue("LoreBox", "ShowModulatorName", lb_mod);
    ini.SetBoolValue("LoreBox", "ColorizeRows", lb_col);
    ini.SetBoolValue("LoreBox", "ShowMultiplier", lb_mul);

    auto writeHex = [&](const char* key, uint32_t val) {
        ini.SetValue("LoreBox", key, std::format("{:06X}", val).c_str());
    };
    writeHex("ColorTitle", Lorebox::color_title.load());
    writeHex("ColorNeutral", Lorebox::color_neutral.load());
    writeHex("ColorSlow", Lorebox::color_slow.load());
    writeHex("ColorFast", Lorebox::color_fast.load());
    writeHex("ColorTransform", Lorebox::color_transform.load());
    writeHex("ColorSeparator", Lorebox::color_separator.load());

    // Write symbol defaults only if missing; preserve user's raw codes/entities (use escaped ASCII)
    if (!ini.KeyExists("LoreBox", "SeparatorSymbol"))
        ini.SetValue("LoreBox", "SeparatorSymbol",
                     String::EncodeEscapesToAscii(
                         Lorebox::separator_symbol).c_str());
    if (!ini.KeyExists("LoreBox", "ArrowRight"))
        ini.SetValue("LoreBox", "ArrowRight",
                     String::EncodeEscapesToAscii(Lorebox::arrow_right).c_str());
    if (!ini.KeyExists("LoreBox", "ArrowLeft"))
        ini.SetValue("LoreBox", "ArrowLeft",
                     String::EncodeEscapesToAscii(Lorebox::arrow_left).c_str());

    ini.SaveFile(Settings::INI_path);
}

void PresetParse::LoadJSONSettings() {
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

void PresetParse::LoadFormGroups() {
    const auto folder_path = std::format("Data/SKSE/Plugins/{}", mod_name) + "/formGroups";
    PresetHelpers::TXT_Helpers::GatherForms(folder_path);
}

void PresetParse::LoadSettingsParallel() {
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
    for (const auto& [key,val] : Settings::INI_settings["Modules"]) {
        if (val) Settings::QFORMS.push_back(key);
    }

    std::vector<std::future<void>> typeFutures;
    typeFutures.reserve(Settings::QFORMS.size());
    ThreadPool typePool(numThreads);

    std::mutex defaultsettingsMutex;
    std::mutex customsettingsMutex;
    std::mutex excludeListMutex;
    std::mutex addonsettingsMutex;

    logger::info("Loading form groups");
    LoadFormGroups();

    for (const auto& _qftype : Settings::QFORMS) {
        typeFutures.push_back(typePool.enqueue(
                [_qftype,&defaultsettingsMutex,&customsettingsMutex,&excludeListMutex,&addonsettingsMutex]() {
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
                        if (const auto temp_custom_settings = parseCustomsParallel(_qftype); !temp_custom_settings.
                            empty()) {
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
                    } catch (const std::exception& ex) {
                        logger::critical("Failed to load addons for {}: {}", _qftype, ex.what());
                        Settings::failed_to_load = true;
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
    } catch (const std::exception& ex) {
        logger::critical("Failed to load json settings: {}", ex.what());
        Settings::failed_to_load = true;
    }
}


rapidjson::Value Settings::Ticker::to_json(rapidjson::Document::AllocatorType& a) // NOLINT(misc-use-internal-linkage)
{
    using namespace rapidjson;
    Value ticker(kObjectType);
    const std::string speed_str = to_string(ticker_speed);
    Value speed_value(speed_str.c_str(), a);
    ticker.AddMember("speed", speed_value, a);
    return ticker;
}