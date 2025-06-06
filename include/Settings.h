#pragma once
#include "CustomObjects.h"
#include <yaml-cpp/yaml.h>



namespace Settings {

    constexpr std::uint32_t kSerializationVersion = 627;
    constexpr std::uint32_t kDataKey = 'QAOT';
    constexpr std::uint32_t kDFDataKey = 'DAOT';
    
    inline bool failed_to_load = false;
    constexpr auto INI_path = L"Data/SKSE/Plugins/AlchemyOfTime.ini";
    const std::string json_path = std::format("Data/SKSE/Plugins/{}/Settings.json", mod_name);
	// POPULATE THIS
    const std::map<const char*, bool> moduleskeyvals = {{"FOOD",false},
														{"INGR",false},
                                                        {"MEDC",false},
                                                        {"POSN",false},
                                                        {"ARMO",false},
														{"WEAP",false},
														{"SCRL",false},
														{"BOOK",false},
														{"SLGM",false},
														{"MISC",false},
														{"NPC",false}
                                                        };
    const std::map<const char*, bool> otherkeysvals = {{"PlacedObjectsEvolve", false},{"UnOwnedObjectsEvolve", false},{"WorldObjectsEvolve", false}, {"bReset", false}, {"DisableWarnings",false}};
    const std::map<const char*, std::map<const char*, bool>> InISections = 
                   {{"Modules", moduleskeyvals}, {"Other Settings", otherkeysvals}};
    inline int nMaxInstances = 200000;
    inline int nForgettingTime = 2160;  // in hours
    inline bool disable_warnings = false;
    inline std::atomic world_objects_evolve = false;
	inline std::atomic placed_objects_evolve = false;
	inline std::atomic unowned_objects_evolve = false;
    inline float proximity_range = 40.f;

    inline float search_radius = 1000.f;
    inline float max_modulator_strength = 500000.f;
    inline float search_scaling = 0.5f; // for IsNextTo
    namespace Ticker {
        enum Intervals {
            kSlower,
            kSlow,
            kNormal,
            kFast,
			kFaster,
            kVeryFast,
            kExtreme,
			kTotal
        };

        inline std::string to_string(const Intervals e) {
            switch (e) {
			case kSlower:
				return "Slower";
			case kSlow:
				return "Slow";
            case kNormal:
                return "Normal";
            case kFast:
                return "Fast";
            case kFaster:
                return "Faster";
            case kVeryFast:
                return "VeryFast";
            case kExtreme:
                return "Extreme";
            default:
                return "Unknown";
            }
        }

		inline Intervals from_string(const std::string& str) {
			if (str == "Slower") return kSlower;
			if (str == "Slow") return kSlow;
			if (str == "Normal") return kNormal;
			if (str == "Fast") return kFast;
			if (str == "Faster") return kFaster;
			if (str == "VeryFast") return kVeryFast;
			if (str == "Extreme") return kExtreme;
			return kNormal;
		}

        inline std::map<Intervals, int> intervals = {
		    {kSlower, 10000 },
			{kSlow, 5000},
			{kNormal, 3000},
			{kFast, 1000},
			{kFaster, 500},
			{kVeryFast, 250},
			{kExtreme, 100}
		};

		inline int GetInterval(const Intervals e) {return intervals.at(e);}

        constexpr int enum_size = kTotal;

        rapidjson::Value to_json(rapidjson::Document::AllocatorType& a);
		void from_json(const rapidjson::Value& j);

	};

    inline Ticker::Intervals ticker_speed = Ticker::kNormal;


    const std::vector<std::string> fakes_allowedQFORMS = {"FOOD", "MISC"};
    const std::vector<std::string> xQFORMS = {"ARMO", "WEAP", "SLGM", "MEDC", "POSN"};  // xdata is carried over in item transitions
    const std::vector<std::string> mgeffs_allowedQFORMS = {"FOOD"};
    [[maybe_unused]] const std::vector<std::string> consumableQFORMS = {"FOOD", "INGR", "MEDC", "POSN", "SCRL", "BOOK", "SLGM", "MISC"};
    [[maybe_unused]] const std::vector<std::string> updateonequipQFORMS = {"ARMO", "WEAP"};
    const std::vector<std::string> sQFORMS = {"NPC"};  // forms that are world objects with inventory and cant be taken into inventory
    const std::map<unsigned int, std::vector<std::string>> qform_bench_map = {
        {1, {"FOOD"}}
    };

    inline std::map<std::string,std::map<std::string, bool>> INI_settings;
    inline std::vector<std::string> QFORMS;
    inline std::map<std::string, DefaultSettings> defaultsettings;
    inline std::map<std::string, CustomSettings> custom_settings;
    inline std::unordered_map<std::string,std::vector<std::string>> exclude_list;
    inline std::map<std::string,std::unordered_map<FormID, AddOnSettings>> addon_settings;

    [[nodiscard]] bool IsQFormType(FormID formid, const std::string& qformtype);

    inline std::string GetQFormType(FormID formid);

    bool IsSpecialQForm(RE::TESObjectREFR* ref);

	[[nodiscard]] bool IsInExclude(FormID formid, std::string type = "");

	void AddToExclude(const std::string& entry_name, const std::string& type, const std::string& filename);

    [[nodiscard]] bool IsItem(FormID formid, const std::string& type = "", bool check_exclude = false);

    [[nodiscard]] bool IsItem(const RE::TESObjectREFR* ref, const std::string& type = "");


	DefaultSettings* GetDefaultSetting(FormID form_id);
	DefaultSettings* GetCustomSetting(const RE::TESForm* form);
	AddOnSettings* GetAddOnSettings(const RE::TESForm* form);

    // 0x99 - ExtraTextDisplayData 
    // 0x3C - ExtraSavedHavokData
    // 0x0B - ExtraPersistentCell
    // 0x48 - ExtraStartingWorldOrCell

    // 0x21 - ExtraOwnership
    // 0x24 - ExtraCount
    // 0x0E - ExtraStartingPosition //crahes when removed
    
    // 0x70 - ExtraEncounterZone 112
    // 0x7E - ExtraReservedMarkers 126
    // 0x88 - ExtraAliasInstanceArray 136
    // 0x8C - ExtraPromotedRef 140 NOT OK
    // 0x1C - ExtraReferenceHandle 28 NOT OK (npc muhabbeti)
    const std::vector<int> xRemove = {
        //0x99, 
        //0x3C, 0x0B, 0x48,
         //0x21, 
         // 
        //0x24,
                                //0x70, 0x7E, 0x88, 
        //0x8C, 0x1C
    };
};

inline std::mutex g_settingsMutex;

std::vector<std::string> LoadExcludeList(const std::string& postfix);
AddOnSettings parseAddOns_(const YAML::Node& config);
std::unordered_map<FormID, AddOnSettings> parseAddOns(const std::string& _type);
DefaultSettings parseDefaults_(const YAML::Node& config);
DefaultSettings parseDefaults(const std::string& _type);
CustomSettings parseCustoms(const std::string& _type);

CustomSettings parseCustomsParallel(const std::string& _type);
std::unordered_map<FormID, AddOnSettings> parseAddOnsParallel(const std::string& _type);
void processCustomFile(const std::string& filename, CustomSettings& combinedSettings);
void processAddOnFile(const std::string& filename, std::unordered_map<FormID, AddOnSettings>& combinedSettings);
void mergeCustomSettings(CustomSettings& dest, const CustomSettings& src);
void mergeAddOnSettings(std::unordered_map<FormID, AddOnSettings>& dest, const std::unordered_map<FormID, AddOnSettings>& src);


void LoadINISettings();
void LoadJSONSettings();
[[maybe_unused]] void LoadSettings();
void LoadSettingsParallel();
void SaveSettings();


namespace LogSettings {
#ifndef NDEBUG
    inline bool log_trace = true;
#else
	inline bool log_trace = false;
#endif
    inline bool log_info = true;
    inline bool log_warning = true;
    inline bool log_error = true;
};