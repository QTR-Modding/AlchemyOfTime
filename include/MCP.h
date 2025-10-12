#pragma once
#include "Events.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "Logger.h"


static void HelpMarker(const char* desc);

namespace UI {

    struct Instance {
        std::pair<StageNo,StageNo> stage_number;
        std::string stage_name;
        Count count;
        float start_time;
        Duration duration;
        float delay_magnitude;
        std::string delayer;
        bool is_fake;
        bool is_transforming;
        bool is_decayed;
    };

    struct GameObject {
        std::string name;
        uint32_t formid;

		bool operator<(const GameObject& other) const {
			return formid < other.formid;
		}
    };

	struct Stage {
		GameObject item;
        StageName name;
		Duration duration;
		bool is_fake;
        bool crafting_allowed;
		//unsigned int n_instances;

		bool operator<(const Stage& other) const {
			return no < other.no;
		}

		// constructor
		Stage(GameObject item_, StageName name_, const Duration duration_, const bool is_fake_, const bool crafting_allowed_, const unsigned int no_)
			: item(std::move(item_)), name(std::move(name_)), duration(duration_), is_fake(is_fake_), crafting_allowed(crafting_allowed_), no(no_) {}

	private:
        unsigned int no = 0;
	};

    struct MCPSource {
		std::set<Stage> stages;
		std::set<GameObject> containers;
		std::set<GameObject> transformers;
		std::map<FormID,GameObject> transformer_enditems;
		std::map<FormID,Duration> transform_durations;
		std::set<GameObject> time_modulators;
		std::map<FormID,float> time_modulator_multipliers;
        std::string type;
    };

    using InstanceMap = std::map<std::string, std::vector<Instance>>;
    using LocationMap = std::map<std::string, InstanceMap>;

    inline std::string log_path = GetLogPath().string();
    inline std::vector<std::string> logLines;

    inline LocationMap locations;
	inline std::map<RefID, std::pair<std::string, float>> update_q;
	inline std::vector<MCPSource> mcp_sources;

    inline std::string last_generated;
    inline std::string item_current = "##current";
    inline std::string sub_item_current = "##item";
    inline std::string source_current = "##source";
    inline bool is_list_box_focused = false;
    inline ImGuiTextFilter* filter;
    inline ImGuiTextFilter* filter2;
	inline std::string filter_module = "None";
	inline int selected_source_index = 0;
    inline ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

    inline Manager* M;
    void __stdcall RenderSettings();
    void __stdcall RenderStatus();
    void __stdcall RenderInspect();
    void __stdcall RenderUpdateQ();
    void __stdcall RenderStages();
    void __stdcall RenderDFT();
    void __stdcall RenderLog();
    void Register(Manager* manager);

    inline std::map<FormID,std::pair<std::string,int>> dynamic_forms;
    inline int dft_form_limit = DynamicFormTracker::GetSingleton()->form_limit;

    void ExcludeList();
	void IniSettingToggle(bool& setting, const std::string& setting_name, const std::string&section_name, const char* desc);
	void DrawFilter1();
	void DrawFilter2();
	bool DrawFilterModule();
    void UpdateSubItem();
    void UpdateLocationMap(const std::vector<Source>& sources);
	void UpdateStages(const std::vector<Source>& sources);
    void RefreshButton();
    void Refresh();

    std::string GetName(FormID formid);

};