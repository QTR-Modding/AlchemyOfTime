#include "MCP.h"
#include "SimpleIni.h"
#include "Lorebox.h"

#ifndef IM_ARRAYSIZE
#define IM_ARRAYSIZE(_ARR) ((int)(sizeof(_ARR) / sizeof(*(_ARR))))
#endif

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

// Decode basic C-style escapes in ASCII buffer to wide string (\n, \r, \t, \\, \xHH, \uXXXX)
static std::wstring DecodeEscapesFromAscii(const char* s)
{
    auto hexVal = [](const char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    };

    std::wstring out;
    for (size_t i = 0; s && s[i];) {
        char c = s[i++];
        if (c == '\\' && s[i]) {
            char t = s[i++];
            switch (t) {
            case 'n': out.push_back(L'\n'); break;
            case 'r': out.push_back(L'\r'); break;
            case 't': out.push_back(L'\t'); break;
            case '\\': out.push_back(L'\\'); break;
            case 'x': {
                int val = 0, digits = 0; 
                while (s[i]) { int hv = hexVal(s[i]); if (hv < 0) break; val = (val << 4) | hv; ++i; ++digits; if (digits >= 2) break; }
                if (digits > 0) out.push_back(static_cast<wchar_t>(val));
                else out.push_back(L'x');
                break; }
            case 'u': {
                int val = 0, digits = 0; 
                while (s[i] && digits < 4) { int hv = hexVal(s[i]); if (hv < 0) break; val = (val << 4) | hv; ++i; ++digits; }
                if (digits == 4) out.push_back(static_cast<wchar_t>(val));
                else out.push_back(L'u');
                break; }
            default:
                out.push_back(static_cast<unsigned char>(t));
                break;
            }
        } else {
            out.push_back(static_cast<unsigned char>(c));
        }
    }
    return out;
}

// Encode wide string to ASCII with C-style escapes for non-ASCII (e.g., L"\u2022").
static std::string EncodeEscapesToAscii(const std::wstring& ws)
{
    std::string out;
    out.reserve(ws.size() * 6);
    auto pushHex4 = [&](const wchar_t wc){
        char buf[7]{}; // \uXXXX + null
        std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(wc & 0xFFFF));
        out += buf;
    };
    for (wchar_t wc : ws) {
        if (wc == L'\\') { out += "\\\\"; }
        else if (wc == L'\n') { out += "\\n"; }
        else if (wc == L'\r') { out += "\\r"; }
        else if (wc == L'\t') { out += "\\t"; }
        else if (wc >= 0 && wc <= 0x7F) { out.push_back(static_cast<char>(wc)); }
        else { pushHex4(wc); }
    }
    return out;
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

void __stdcall UI::RenderLoreBox()
{
    // Init UI toggles from runtime once
    static bool initialized = false;
    static ImVec4 col_title, col_neutral, col_slow, col_fast, col_transform, col_separator;
    static char sep_symbol[16] = {0};
    static char arrow_right_buf[16] = {0};
    static char arrow_left_buf[16] = {0};
    if (!initialized) {
        lorebox_show_title = Lorebox::show_title.load();
        lorebox_show_percentage = Lorebox::show_percentage.load();
        auto cvt = [](const uint32_t c) -> ImVec4 {
            const float r = ((c >> 16) & 0xFF) / 255.0f;
            const float g = ((c >> 8) & 0xFF) / 255.0f;
            const float b = (c & 0xFF) / 255.0f;
            return ImVec4(r,g,b,1.0f);
        };
        col_title     = cvt(Lorebox::color_title.load());
        col_neutral   = cvt(Lorebox::color_neutral.load());
        col_slow      = cvt(Lorebox::color_slow.load());
        col_fast      = cvt(Lorebox::color_fast.load());
        col_transform = cvt(Lorebox::color_transform.load());
        col_separator = cvt(Lorebox::color_separator.load());
        // copy current symbols to buffers, encoding non-ASCII as \uXXXX so they show up in MCP UI
        auto w2esc_to_buf = [](const std::wstring& ws, char* dst, const size_t cap){
            std::string s = EncodeEscapesToAscii(ws);
            if (cap) { strncpy_s(dst, cap, s.c_str(), _TRUNCATE); }
        };
        w2esc_to_buf(Lorebox::separator_symbol, sep_symbol, sizeof(sep_symbol));
        w2esc_to_buf(Lorebox::arrow_right, arrow_right_buf, sizeof(arrow_right_buf));
        w2esc_to_buf(Lorebox::arrow_left, arrow_left_buf, sizeof(arrow_left_buf));
        initialized = true;
    }

    ImGui::Text("LoreBox");
    if (ImGui::BeginTable("table_lorebox_section", 2, table_flags)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Checkbox("Show tooltip title", &lorebox_show_title);
        ImGui::TableNextColumn();
        ImGui::TextColored(lorebox_show_title ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), lorebox_show_title ? "Enabled" : "Disabled");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Checkbox("Show progress percentage", &lorebox_show_percentage);
        ImGui::TableNextColumn();
        ImGui::TextColored(lorebox_show_percentage ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), lorebox_show_percentage ? "Enabled" : "Disabled");

        // New options
        static bool show_mod_name = Lorebox::show_modulator_name.load();
        static bool show_multiplier = Lorebox::show_multiplier.load();
        static bool colorize      = Lorebox::colorize_rows.load();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Checkbox("Show modulator/transformer name", &show_mod_name);
        ImGui::TableNextColumn();
        ImGui::TextColored(show_mod_name ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), show_mod_name ? "Enabled" : "Disabled");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Checkbox("Show multiplier (x) next to name", &show_multiplier);
        ImGui::TableNextColumn();
        ImGui::TextColored(show_multiplier ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), show_multiplier ? "Enabled" : "Disabled");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Checkbox("Colorize rows by state", &colorize);
        ImGui::TableNextColumn();
        ImGui::TextColored(colorize ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), colorize ? "Enabled" : "Disabled");

        // Color editors (use ColorEdit4 with NoAlpha)
        auto colorEdit4RGB = [](const char* label, ImVec4& col) {
            float c[4] = { col.x, col.y, col.z, 1.0f };
            if (ImGui::ColorEdit4(label, c, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_DisplayRGB)) {
                col.x = c[0]; col.y = c[1]; col.z = c[2];
            }
        };

        ImGui::TableNextRow(); ImGui::TableNextColumn(); colorEdit4RGB("Title color", col_title); ImGui::TableNextColumn();
        ImGui::Text("#%02X%02X%02X", static_cast<int>(col_title.x * 255), static_cast<int>(col_title.y * 255), static_cast<int>(col_title.z * 255));
        ImGui::TableNextRow(); ImGui::TableNextColumn(); colorEdit4RGB("Neutral color", col_neutral); ImGui::TableNextColumn();
        ImGui::Text("#%02X%02X%02X", static_cast<int>(col_neutral.x * 255), static_cast<int>(col_neutral.y * 255), static_cast<int>(col_neutral.z * 255));
        ImGui::TableNextRow(); ImGui::TableNextColumn(); colorEdit4RGB("Slow color", col_slow); ImGui::TableNextColumn();
        ImGui::Text("#%02X%02X%02X", static_cast<int>(col_slow.x * 255), static_cast<int>(col_slow.y * 255), static_cast<int>(col_slow.z * 255));
        ImGui::TableNextRow(); ImGui::TableNextColumn(); colorEdit4RGB("Fast color", col_fast); ImGui::TableNextColumn();
        ImGui::Text("#%02X%02X%02X", static_cast<int>(col_fast.x * 255), static_cast<int>(col_fast.y * 255), static_cast<int>(col_fast.z * 255));
        ImGui::TableNextRow(); ImGui::TableNextColumn(); colorEdit4RGB("Transform color", col_transform); ImGui::TableNextColumn();
        ImGui::Text("#%02X%02X%02X", static_cast<int>(col_transform.x * 255), static_cast<int>(col_transform.y * 255), static_cast<int>(col_transform.z * 255));
        ImGui::TableNextRow(); ImGui::TableNextColumn(); colorEdit4RGB("Separator color", col_separator); ImGui::TableNextColumn();
        ImGui::Text("#%02X%02X%02X", static_cast<int>(col_separator.x * 255), static_cast<int>(col_separator.y * 255), static_cast<int>(col_separator.z * 255));

        // Symbol editors
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::InputText("Separator symbol", sep_symbol, IM_ARRAYSIZE(sep_symbol));
        ImGui::TableNextColumn();
        HelpMarker("ASCII or escape codes (e.g., \\u2022) recommended for compatibility. Example: *, -, \\u2022");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::InputText("Arrow right", arrow_right_buf, IM_ARRAYSIZE(arrow_right_buf));
        ImGui::TableNextColumn();
        HelpMarker("Default '->'. You can use escapes like \\u2192.");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::InputText("Arrow left", arrow_left_buf, IM_ARRAYSIZE(arrow_left_buf));
        ImGui::TableNextColumn();
        HelpMarker("Default '<-'. You can use escapes like \\u2190.");

        ImGui::EndTable();

        // sync to atomics
        Lorebox::show_title.store(lorebox_show_title, std::memory_order_relaxed);
        Lorebox::show_percentage.store(lorebox_show_percentage, std::memory_order_relaxed);
        Lorebox::show_modulator_name.store(show_mod_name, std::memory_order_relaxed);
        Lorebox::show_transformer_name.store(show_mod_name, std::memory_order_relaxed);
        Lorebox::show_multiplier.store(show_multiplier, std::memory_order_relaxed);
        Lorebox::colorize_rows.store(colorize, std::memory_order_relaxed);

        if (ImGui::Button("Save##lorebox_save")) {
            // persist toggles + colors + symbols
            CSimpleIniA ini;
            ini.SetUnicode();
            ini.LoadFile(Settings::INI_path);
            ini.SetBoolValue("LoreBox", "ShowTitle", lorebox_show_title);
            ini.SetBoolValue("LoreBox", "ShowPercentage", lorebox_show_percentage);
            ini.SetBoolValue("LoreBox", "ShowModName", show_mod_name);
            ini.SetBoolValue("LoreBox", "ShowTransformerName", show_mod_name);
            ini.SetBoolValue("LoreBox", "ShowMultiplier", show_multiplier);
            ini.SetBoolValue("LoreBox", "ColorizeRows", colorize);

            auto toHex = [](const ImVec4& c) {
                const uint32_t R = static_cast<uint32_t>(std::round(c.x * 255.f));
                const uint32_t G = static_cast<uint32_t>(std::round(c.y * 255.f));
                const uint32_t B = static_cast<uint32_t>(std::round(c.z * 255.f));
                return std::format("{:02X}{:02X}{:02X}", R, G, B);
            };
            ini.SetValue("LoreBox", "ColorTitle", toHex(col_title).c_str());
            ini.SetValue("LoreBox", "ColorNeutral", toHex(col_neutral).c_str());
            ini.SetValue("LoreBox", "ColorSlow", toHex(col_slow).c_str());
            ini.SetValue("LoreBox", "ColorFast", toHex(col_fast).c_str());
            ini.SetValue("LoreBox", "ColorTransform", toHex(col_transform).c_str());
            ini.SetValue("LoreBox", "ColorSeparator", toHex(col_separator).c_str());

            ini.SetValue("LoreBox", "SeparatorSymbol", sep_symbol);
            ini.SetValue("LoreBox", "ArrowRight", arrow_right_buf);
            ini.SetValue("LoreBox", "ArrowLeft", arrow_left_buf);
            ini.SaveFile(Settings::INI_path);

            // update runtime atomics
            auto fromCol = [](const ImVec4& c) -> uint32_t {
                const uint32_t R = static_cast<uint32_t>(std::round(c.x * 255.f));
                const uint32_t G = static_cast<uint32_t>(std::round(c.y * 255.f));
                const uint32_t B = static_cast<uint32_t>(std::round(c.z * 255.f));
                return (R << 16) | (G << 8) | B;
            };
            Lorebox::color_title.store(fromCol(col_title));
            Lorebox::color_neutral.store(fromCol(col_neutral));
            Lorebox::color_slow.store(fromCol(col_slow));
            Lorebox::color_fast.store(fromCol(col_fast));
            Lorebox::color_transform.store(fromCol(col_transform));
            Lorebox::color_separator.store(fromCol(col_separator));

            // update symbols: decode backslash escapes into wide
            Lorebox::separator_symbol = DecodeEscapesFromAscii(sep_symbol);
            Lorebox::arrow_right = DecodeEscapesFromAscii(arrow_right_buf);
            Lorebox::arrow_left = DecodeEscapesFromAscii(arrow_left_buf);
        }
    }
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
    // New LoreBox section
    SKSEMenuFramework::AddSectionItem("LoreBox", RenderLoreBox);
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
    if (setting != previous_state) {
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
            if (const auto* stage = source.GetStage(max_stage_no)) {
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
