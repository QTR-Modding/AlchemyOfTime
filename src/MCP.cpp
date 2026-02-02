#include "MCP.h"
#include "SimpleIni.h"
#include "Lorebox.h"
#include "Manager.h"
#include "ClibUtil/editorID.hpp"

#ifndef IM_ARRAYSIZE
#define IM_ARRAYSIZE(_ARR) ((int)(sizeof(_ARR) / sizeof(*(_ARR))))
#endif

void HelpMarker(const char* desc) {
    ImGuiMCP::TextDisabled("(?)");
    if (ImGuiMCP::BeginItemTooltip()) {
        ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 35.0f);
        ImGuiMCP::TextUnformatted(desc);
        ImGuiMCP::PopTextWrapPos();
        ImGuiMCP::EndTooltip();
    }
}

void __stdcall UI::RenderSettings() {
    for (const auto& [section_name, section_settings] : Settings::INI_settings) {
        if (ImGuiMCP::CollapsingHeader(section_name.c_str(), ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGuiMCP::BeginTable("table_settings", 2, table_flags)) {
                for (const auto& [setting_name, setting] : section_settings) {
                    ImGuiMCP::TableNextRow();
                    ImGuiMCP::TableNextColumn();
                    if (setting_name == "DisableWarnings") {
                        IniSettingToggle(Settings::disable_warnings, setting_name, section_name,
                                         "Disables in-game warning pop-ups.");
                    } else if (setting_name == "WorldObjectsEvolve") {
                        bool temp = Settings::world_objects_evolve.load();
                        IniSettingToggle(temp, setting_name, section_name,
                                         "Allows items out in the world to transform.");
                        Settings::world_objects_evolve.store(temp);
                    } else if (setting_name == "PlacedObjectsEvolve") {
                        bool temp = Settings::placed_objects_evolve.load();
                        IniSettingToggle(temp, setting_name, section_name,
                                         "Allows hand-placed objects (vanilla or by mods) to transform.");
                        Settings::placed_objects_evolve.store(temp);
                    } else if (setting_name == "UnOwnedObjectsEvolve") {
                        bool temp = Settings::unowned_objects_evolve.load();
                        IniSettingToggle(temp, setting_name, section_name, "Allows unowned objects to transform.");
                        Settings::unowned_objects_evolve.store(temp);
                    } else {
                        // we just want to display the settings in read only mode
                        ImGuiMCP::Text(setting_name.c_str());
                    }
                    ImGuiMCP::TableNextColumn();
                    const auto temp_setting_val = setting_name == "DisableWarnings"
                                                      ? Settings::disable_warnings
                                                      : setting;
                    const char* value = temp_setting_val ? "Enabled" : "Disabled";
                    const auto color = setting ? ImGuiMCP::ImVec4(0, 1, 0, 1) : ImGuiMCP::ImVec4(1, 0, 0, 1);
                    ImGuiMCP::TextColored(color, value);
                }
                ImGuiMCP::EndTable();
            }
        }
    }

    ImGuiMCP::SetNextItemWidth(320.f);
    int max_dirty_updates = static_cast<int>(Settings::max_dirty_updates.load());
    if (ImGuiMCP::SliderInt("Max Updates Per Tick", &max_dirty_updates,
                            static_cast<int>(Settings::max_dirty_updates_min),
                            static_cast<int>(Settings::max_dirty_updates_max))) {
        const auto clamped = std::clamp<size_t>(
            static_cast<size_t>(max_dirty_updates),
            Settings::max_dirty_updates_min,
            Settings::max_dirty_updates_max);
        Settings::max_dirty_updates.store(clamped);
        PresetParse::SaveSettings();
    }
    ImGuiMCP::SameLine();
    HelpMarker("Limits how many objects are updated each tick.");

    #ifndef NDEBUG
    ImGuiMCP::Checkbox("DrawDebug", &draw_debug);
    #endif
}

void __stdcall UI::RenderStatus() {
    constexpr auto color_operational = ImGuiMCP::ImVec4(0, 1, 0, 1);
    constexpr auto color_not_operational = ImGuiMCP::ImVec4(1, 0, 0, 1);

    if (!M) {
        ImGuiMCP::TextColored(color_not_operational, "Mod is not working! Check log for more info.");
        return;
    }

    if (ImGuiMCP::BeginTable("table_status", 3, table_flags)) {
        ImGuiMCP::TableSetupColumn("Module");
        ImGuiMCP::TableSetupColumn("Default Preset");
        ImGuiMCP::TableSetupColumn("Custom Presets");
        ImGuiMCP::TableHeadersRow();
        for (const auto& [module_name,module_enabled] : Settings::INI_settings["Modules"]) {
            if (!module_enabled) continue;
            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(module_name.c_str());
            ImGuiMCP::TableNextColumn();
            const auto loaded_default = Settings::defaultsettings.contains(module_name)
                                            ? Settings::defaultsettings.at(module_name).IsHealthy()
                                            : false;
            const auto loaded_default_str = loaded_default ? "Loaded" : "Not Loaded";
            const auto color = loaded_default ? color_operational : color_not_operational;
            ImGuiMCP::TextColored(color, loaded_default_str);
            ImGuiMCP::TableNextColumn();
            const auto& custom_presets = Settings::custom_settings[module_name];

            std::string loaded_custom_str = "Not Loaded";
            auto color_custom = color_not_operational;
            if (!custom_presets.empty()) {
                loaded_custom_str = std::format("Loaded ({})", custom_presets.size());
                color_custom = color_operational;
            }
            ImGuiMCP::TextColored(color_custom, loaded_custom_str.c_str());
        }
        ImGuiMCP::EndTable();
    }

    ExcludeList();
}

void __stdcall UI::RenderLoreBox() {
    // Init UI toggles from runtime once
    static bool initialized = false;
    static ImGuiMCP::ImVec4 col_title, col_neutral, col_slow, col_fast, col_transform, col_separator;
    static char sep_symbol[16] = {0};
    static char arrow_right_buf[16] = {0};
    static char arrow_left_buf[16] = {0};
    if (!initialized) {
        lorebox_show_title = Lorebox::show_title.load();
        lorebox_show_percentage = Lorebox::show_percentage.load();
        auto cvt = [](const uint32_t c) -> ImGuiMCP::ImVec4 {
            const float r = (c >> 16 & 0xFF) / 255.0f;
            const float g = (c >> 8 & 0xFF) / 255.0f;
            const float b = (c & 0xFF) / 255.0f;
            return ImGuiMCP::ImVec4(r, g, b, 1.0f);
        };
        col_title = cvt(Lorebox::color_title.load());
        col_neutral = cvt(Lorebox::color_neutral.load());
        col_slow = cvt(Lorebox::color_slow.load());
        col_fast = cvt(Lorebox::color_fast.load());
        col_transform = cvt(Lorebox::color_transform.load());
        col_separator = cvt(Lorebox::color_separator.load());
        // copy current symbols to buffers, encoding non-ASCII as \uXXXX so they show up in MCP UI
        auto w2esc_to_buf = [](const std::wstring& ws, char* dst, const size_t cap) {
            const std::string s = String::EncodeEscapesToAscii(ws);
            if (cap) { strncpy_s(dst, cap, s.c_str(), _TRUNCATE); }
        };
        w2esc_to_buf(Lorebox::separator_symbol, sep_symbol, sizeof(sep_symbol));
        w2esc_to_buf(Lorebox::arrow_right, arrow_right_buf, sizeof(arrow_right_buf));
        w2esc_to_buf(Lorebox::arrow_left, arrow_left_buf, sizeof(arrow_left_buf));
        initialized = true;
    }

    ImGuiMCP::Text("LoreBox");
    if (ImGuiMCP::BeginTable("table_lorebox_section", 2, table_flags)) {
        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Checkbox("Show tooltip title", &lorebox_show_title);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::TextColored(lorebox_show_title ? ImGuiMCP::ImVec4(0, 1, 0, 1) : ImGuiMCP::ImVec4(1, 0, 0, 1),
                              lorebox_show_title ? "Enabled" : "Disabled");

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Checkbox("Show progress percentage", &lorebox_show_percentage);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::TextColored(lorebox_show_percentage ? ImGuiMCP::ImVec4(0, 1, 0, 1) : ImGuiMCP::ImVec4(1, 0, 0, 1),
                              lorebox_show_percentage ? "Enabled" : "Disabled");

        // New options
        static bool show_mod_name = Lorebox::show_modulator_name.load();
        static bool show_multiplier = Lorebox::show_multiplier.load();
        static bool colorize = Lorebox::colorize_rows.load();

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Checkbox("Show modulator/transformer name", &show_mod_name);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::TextColored(show_mod_name ? ImGuiMCP::ImVec4(0, 1, 0, 1) : ImGuiMCP::ImVec4(1, 0, 0, 1),
                              show_mod_name ? "Enabled" : "Disabled");

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Checkbox("Show multiplier (x) next to name", &show_multiplier);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::TextColored(show_multiplier ? ImGuiMCP::ImVec4(0, 1, 0, 1) : ImGuiMCP::ImVec4(1, 0, 0, 1),
                              show_multiplier ? "Enabled" : "Disabled");

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Checkbox("Colorize rows by state", &colorize);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::TextColored(colorize ? ImGuiMCP::ImVec4(0, 1, 0, 1) : ImGuiMCP::ImVec4(1, 0, 0, 1),
                              colorize ? "Enabled" : "Disabled");

        // Color editors (use ColorEdit4 with NoAlpha)
        auto colorEdit4RGB = [](const char* label, ImGuiMCP::ImVec4& col) {
            float c[4] = {col.x, col.y, col.z, 1.0f};
            if (ImGuiMCP::ColorEdit4(
                label, c,
                ImGuiMCP::ImGuiColorEditFlags_NoInputs | ImGuiMCP::ImGuiColorEditFlags_NoAlpha |
                ImGuiMCP::ImGuiColorEditFlags_DisplayRGB)) {
                col.x = c[0];
                col.y = c[1];
                col.z = c[2];
            }
        };

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        colorEdit4RGB("Title color", col_title);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Text("#%02X%02X%02X", static_cast<int>(col_title.x * 255), static_cast<int>(col_title.y * 255),
                       static_cast<int>(col_title.z * 255));
        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        colorEdit4RGB("Neutral color", col_neutral);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Text("#%02X%02X%02X", static_cast<int>(col_neutral.x * 255), static_cast<int>(col_neutral.y * 255),
                       static_cast<int>(col_neutral.z * 255));
        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        colorEdit4RGB("Slow color", col_slow);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Text("#%02X%02X%02X", static_cast<int>(col_slow.x * 255), static_cast<int>(col_slow.y * 255),
                       static_cast<int>(col_slow.z * 255));
        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        colorEdit4RGB("Fast color", col_fast);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Text("#%02X%02X%02X", static_cast<int>(col_fast.x * 255), static_cast<int>(col_fast.y * 255),
                       static_cast<int>(col_fast.z * 255));
        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        colorEdit4RGB("Transform color", col_transform);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Text("#%02X%02X%02X", static_cast<int>(col_transform.x * 255),
                       static_cast<int>(col_transform.y * 255),
                       static_cast<int>(col_transform.z * 255));
        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        colorEdit4RGB("Separator color", col_separator);
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::Text("#%02X%02X%02X", static_cast<int>(col_separator.x * 255),
                       static_cast<int>(col_separator.y * 255),
                       static_cast<int>(col_separator.z * 255));

        // Symbol editors
        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::InputText("Separator symbol", sep_symbol, IM_ARRAYSIZE(sep_symbol));
        ImGuiMCP::TableNextColumn();
        HelpMarker("ASCII or escape codes (e.g., \\u2022) recommended for compatibility. Example: *, -, \\u2022");

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::InputText("Arrow right", arrow_right_buf, IM_ARRAYSIZE(arrow_right_buf));
        ImGuiMCP::TableNextColumn();
        HelpMarker("Default '->'. You can use escapes like \\u2192.");

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableNextColumn();
        ImGuiMCP::InputText("Arrow left", arrow_left_buf, IM_ARRAYSIZE(arrow_left_buf));
        ImGuiMCP::TableNextColumn();
        HelpMarker("Default '<-'. You can use escapes like \\u2190.");

        ImGuiMCP::EndTable();

        // sync to atomics
        Lorebox::show_title.store(lorebox_show_title, std::memory_order_relaxed);
        Lorebox::show_percentage.store(lorebox_show_percentage, std::memory_order_relaxed);
        Lorebox::show_modulator_name.store(show_mod_name, std::memory_order_relaxed);
        Lorebox::show_multiplier.store(show_multiplier, std::memory_order_relaxed);
        Lorebox::colorize_rows.store(colorize, std::memory_order_relaxed);

        if (ImGuiMCP::Button("Save##lorebox_save")) {
            // persist toggles + colors + symbols
            CSimpleIniA ini;
            ini.SetUnicode();
            ini.LoadFile(Settings::INI_path);
            ini.SetBoolValue("LoreBox", "ShowTitle", lorebox_show_title);
            ini.SetBoolValue("LoreBox", "ShowPercentage", lorebox_show_percentage);
            ini.SetBoolValue("LoreBox", "ShowModulatorName", show_mod_name);
            ini.SetBoolValue("LoreBox", "ShowMultiplier", show_multiplier);
            ini.SetBoolValue("LoreBox", "ColorizeRows", colorize);

            auto toHex = [](const ImGuiMCP::ImVec4& c) {
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
            auto fromCol = [](const ImGuiMCP::ImVec4& c) -> uint32_t {
                const uint32_t R = static_cast<uint32_t>(std::round(c.x * 255.f));
                const uint32_t G = static_cast<uint32_t>(std::round(c.y * 255.f));
                const uint32_t B = static_cast<uint32_t>(std::round(c.z * 255.f));
                return R << 16 | G << 8 | B;
            };
            Lorebox::color_title.store(fromCol(col_title));
            Lorebox::color_neutral.store(fromCol(col_neutral));
            Lorebox::color_slow.store(fromCol(col_slow));
            Lorebox::color_fast.store(fromCol(col_fast));
            Lorebox::color_transform.store(fromCol(col_transform));
            Lorebox::color_separator.store(fromCol(col_separator));

            // update symbols: decode backslash escapes into wide
            Lorebox::separator_symbol = String::DecodeEscapesFromAscii(sep_symbol);
            Lorebox::arrow_right = String::DecodeEscapesFromAscii(arrow_right_buf);
            Lorebox::arrow_left = String::DecodeEscapesFromAscii(arrow_left_buf);
        }
    }
}

void __stdcall UI::RenderInspect() {
    if (!M) {
        ImGuiMCP::Text("Not available");
        return;
    }

    RefreshButton();

    ImGuiMCP::Text("Location");
    if (ImGuiMCP::BeginCombo("##combo 1", item_current.c_str())) {
        for (const auto& key : locations | std::views::keys) {
            if (filter->PassFilter(key.c_str())) {
                if (const bool is_selected = item_current == key; ImGuiMCP::Selectable(key.c_str(), is_selected)) {
                    item_current = key;
                    UpdateSubItem();
                }
            }
        }
        ImGuiMCP::EndCombo();
    }

    ImGuiMCP::SameLine();
    DrawFilter1();

    is_list_box_focused =
        ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_NoNavOverride) || ImGuiMCP::IsItemActive();

    if (locations.contains(item_current)) {
        InstanceMap& selectedItem = locations[item_current];
        ImGuiMCP::Text("Item");
        if (ImGuiMCP::BeginCombo("##combo 2", sub_item_current.c_str())) {
            for (const auto& key : selectedItem | std::views::keys) {
                if (filter2->PassFilter(key.c_str())) {
                    const bool is_selected = sub_item_current == key;
                    if (ImGuiMCP::Selectable(key.c_str(), is_selected)) {
                        sub_item_current = key;
                    }
                    if (is_selected) {
                        ImGuiMCP::SetItemDefaultFocus();
                    }
                }
            }
            ImGuiMCP::EndCombo();
        }

        ImGuiMCP::SameLine();
        DrawFilter2();

        if (selectedItem.contains(sub_item_current)) {
            const auto& selectedInstances = selectedItem[sub_item_current];

            ImGuiMCP::Text("Instances");
            if (ImGuiMCP::BeginTable("table_inspect", 8, table_flags)) {
                ImGuiMCP::TableSetupColumn("Stage");
                ImGuiMCP::TableSetupColumn("Count");
                ImGuiMCP::TableSetupColumn("Start Time");
                ImGuiMCP::TableSetupColumn("Duration");
                ImGuiMCP::TableSetupColumn("Time Modulation");
                ImGuiMCP::TableSetupColumn("Dynamic Form");
                ImGuiMCP::TableSetupColumn("Transforming");
                ImGuiMCP::TableSetupColumn("Decayed");
                ImGuiMCP::TableHeadersRow();
                for (const auto& item : selectedInstances) {
                    ImGuiMCP::TableNextRow();
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextUnformatted(std::format("{}/{} {}", item.stage_number.first, item.stage_number.second,
                                                          item.stage_name).c_str());
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextUnformatted(std::format("{}", item.count).c_str());
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextUnformatted(std::format("{}", item.start_time).c_str());
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextUnformatted(std::format("{}", item.duration).c_str());
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextUnformatted(std::format("{} ({})", item.delay_magnitude, item.delayer).c_str());
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextUnformatted(item.is_fake ? "Yes" : "No");
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextUnformatted(item.is_transforming ? "Yes" : "No");
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextUnformatted(item.is_decayed ? "Yes" : "No");
                }
                ImGuiMCP::EndTable();
            }
        }
    }
}

void __stdcall UI::RenderUpdateQ() {
    if (!M) {
        ImGuiMCP::Text("Not available");
        return;
    }

    RefreshButton();
    ImGuiMCP::Text("Update Queue: %s", M->IsTickerActive() ? "Active" : "Paused");
    // need a combo box to select the ticker speed
    ImGuiMCP::SetNextItemWidth(180.f);
    const auto ticker_speed_str = Settings::Ticker::to_string(Settings::ticker_speed);
    if (ImGuiMCP::BeginCombo("##combo_ticker_speed", ticker_speed_str.c_str())) {
        for (int i = 0; i < Settings::Ticker::enum_size; ++i) {
            const auto speed = static_cast<Settings::Ticker::Intervals>(i);
            const auto speed_str = Settings::Ticker::to_string(speed);
            if (ImGuiMCP::Selectable(speed_str.c_str(), Settings::ticker_speed == speed)) {
                Settings::SetCurrentTickInterval(speed);
                M->UpdateInterval(std::chrono::milliseconds(Settings::Ticker::GetInterval(Settings::ticker_speed)));
                PresetParse::SaveSettings();
            }
        }
        ImGuiMCP::EndCombo();
    }
    ImGuiMCP::SameLine();
    HelpMarker(
        "Choosing faster options reduces the time between updates, making the evolution of items out in the world more responsive. It will take time when switching from slower settings.");

    if (Settings::world_objects_evolve.load()) {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0, 1, 0, 1), "World Objects Evolve: Enabled");
    } else {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1, 0, 0, 1), "World Objects Evolve: Disabled");
    }

    if (ImGuiMCP::BeginTable("table_queue", 2, table_flags)) {
        ImGuiMCP::TableSetupColumn("Name");
        ImGuiMCP::TableSetupColumn("Update Time");
        ImGuiMCP::TableHeadersRow();
        for (const auto& [fst, snd] : update_q | std::views::values) {
            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(fst.c_str());
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(std::format("{}", snd).c_str());
        }
        ImGuiMCP::EndTable();
    }
}

void __stdcall UI::RenderStages() {
    if (!M) {
        ImGuiMCP::Text("Not available");
        return;
    }

    RefreshButton();

    if (mcp_sources.empty()) {
        ImGuiMCP::Text("No stages available");
        return;
    }

    ImGuiMCP::Text("Source List");

    source_current = "Source " + std::to_string(selected_source_index + 1) + ": " + mcp_sources[selected_source_index].
                     stages.begin()->item.name;

    if (ImGuiMCP::BeginCombo("##SourceList", source_current.c_str())) {
        for (size_t i = 0; i < mcp_sources.size(); ++i) {
            if (filter_module != "None" && mcp_sources[i].type != filter_module) continue;
            std::string label = "Source " + std::to_string(i + 1) + ": " + mcp_sources[i].stages.begin()->item.name;
            if (ImGuiMCP::Selectable(label.c_str(), selected_source_index == static_cast<int>(i))) {
                selected_source_index = static_cast<int>(i);
                source_current = label;
            }
        }
        ImGuiMCP::EndCombo();
    }

    ImGuiMCP::SameLine();
    if (const auto module_filter_old = filter_module; DrawFilterModule() && module_filter_old != filter_module) {
        for (size_t i = 0; i < mcp_sources.size(); ++i) {
            if (filter_module != "None" && mcp_sources[i].type != filter_module) continue;
            selected_source_index = static_cast<int>(i);
            source_current = "Source " + std::to_string(i + 1) + ": " + mcp_sources[i].stages.begin()->item.name;
            break;
        }
    }

    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Exclude##addtoexclude")) {
        const auto& temp_selected_source = mcp_sources[selected_source_index];
        const auto temp_form = RE::TESForm::LookupByID(temp_selected_source.stages.begin()->item.formid);
        if (const auto temp_editorid = clib_util::editorID::get_editorID(temp_form); !temp_editorid.empty()) {
            Settings::AddToExclude(temp_editorid, temp_selected_source.type, "MCP");
        }
    }

    if (mcp_sources.empty() || selected_source_index >= mcp_sources.size()) {
        ImGuiMCP::Text("No sources available");
        return;
    }

    ImGuiMCP::Text("");
    ImGuiMCP::Text("Stages");
    const auto& src = mcp_sources[selected_source_index];

    if (ImGuiMCP::BeginTable("table_stages", 5, table_flags)) {
        ImGuiMCP::TableSetupColumn("Item");
        ImGuiMCP::TableSetupColumn("Name");
        ImGuiMCP::TableSetupColumn("Duration");
        ImGuiMCP::TableSetupColumn("Crafting Allowed");
        ImGuiMCP::TableSetupColumn("Is Dynamic Form");
        ImGuiMCP::TableHeadersRow();
        for (const auto& stage : src.stages) {
            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text((stage.item.name + std::format(" ({:x})", stage.item.formid)).c_str());
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(stage.name.c_str());
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(std::format("{}", stage.duration).c_str());
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(stage.crafting_allowed ? "Yes" : "No");
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(stage.is_fake ? "Yes" : "No");
        }
        ImGuiMCP::EndTable();
    }

    ImGuiMCP::Text("");
    ImGuiMCP::Text("Containers");
    if (ImGuiMCP::BeginTable("table_containers", 1, table_flags)) {
        ImGuiMCP::TableSetupColumn("Container (FormID)");
        ImGuiMCP::TableHeadersRow();
        for (const auto& [name, formid] : src.containers) {
            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text((name + std::format(" ({:x})", formid)).c_str());
        }
        ImGuiMCP::EndTable();
    }

    ImGuiMCP::Text("");
    ImGuiMCP::Text("Transformers");
    if (ImGuiMCP::BeginTable("table_transformers", 3, table_flags)) {
        ImGuiMCP::TableSetupColumn("Item");
        ImGuiMCP::TableSetupColumn("Transformed Item");
        ImGuiMCP::TableSetupColumn("Duration");
        ImGuiMCP::TableHeadersRow();
        for (const auto& [name, formid] : src.transformers) {
            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text((name + std::format(" ({:x})", formid)).c_str());
            //ImGui::TableNextColumn();
            const auto temp_name = src.transformer_enditems.contains(formid)
                                       ? src.transformer_enditems.at(formid).name
                                       : std::format("{:x}", formid);
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(temp_name.c_str());
            ImGuiMCP::TableNextColumn();
            const auto temp_duration = src.transform_durations.contains(formid)
                                           ? std::format("{}", src.transform_durations.at(formid))
                                           : "???";
            ImGuiMCP::Text(temp_duration.c_str());
        }
        ImGuiMCP::EndTable();
    }

    ImGuiMCP::Text("");
    ImGuiMCP::Text("Time Modulators");
    if (ImGuiMCP::BeginTable("table_time_modulators", 2, table_flags)) {
        ImGuiMCP::TableSetupColumn("Item");
        ImGuiMCP::TableSetupColumn("Multiplier");
        ImGuiMCP::TableHeadersRow();
        for (const auto& [name, formid] : src.time_modulators) {
            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text((name + std::format(" ({:x})", formid)).c_str());
            ImGuiMCP::TableNextColumn();
            const auto temp_duration = src.time_modulator_multipliers.contains(formid)
                                           ? std::format("{}", src.time_modulator_multipliers.at(formid))
                                           : "???";
            ImGuiMCP::Text(temp_duration.c_str());
        }
        ImGuiMCP::EndTable();
    }
}

void __stdcall UI::RenderDFT() {
    RefreshButton();

    ImGuiMCP::Text(std::format("Dynamic Forms ({}/{})", dynamic_forms.size(), dft_form_limit).c_str());
    if (dynamic_forms.empty()) {
        ImGuiMCP::Text("No dynamic forms found.");
        return;
    }
    // dynamic forms table: FormID, Name, Status
    if (ImGuiMCP::BeginTable("table_dynamic_forms", 3, table_flags)) {
        for (const auto& [formid, form] : dynamic_forms) {
            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(std::format("{:08X}", formid).c_str());
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::Text(form.first.c_str());
            ImGuiMCP::TableNextColumn();
            const auto color = form.second == 2
                                   ? ImGuiMCP::ImVec4(0, 1, 0, 1)
                                   : form.second == 1
                                   ? ImGuiMCP::ImVec4(1, 1, 0, 1)
                                   : ImGuiMCP::ImVec4(1, 0, 0, 1);
            ImGuiMCP::TextColored(color, form.second == 2 ? "Active" : form.second == 1 ? "Protected" : "Inactive");
        }
        ImGuiMCP::EndTable();
    }
}

void __stdcall UI::RenderLog() {
    #ifndef NDEBUG
    ImGuiMCP::Checkbox("Trace", &LogSettings::log_trace);
    #endif
    ImGuiMCP::SameLine();
    ImGuiMCP::Checkbox("Info", &LogSettings::log_info);
    ImGuiMCP::SameLine();
    ImGuiMCP::Checkbox("Warning", &LogSettings::log_warning);
    ImGuiMCP::SameLine();
    ImGuiMCP::Checkbox("Error", &LogSettings::log_error);

    // if "Generate Log" button is pressed, read the log file
    if (ImGuiMCP::Button("Generate Log")) logLines = ReadLogFile();

    // Display each line in a new ImGui::Text() element
    for (const auto& line : logLines) {
        if (!LogSettings::log_trace && line.find("trace") != std::string::npos) continue;
        if (!LogSettings::log_info && line.find("info") != std::string::npos) continue;
        if (!LogSettings::log_warning && line.find("warning") != std::string::npos) continue;
        if (!LogSettings::log_error && line.find("error") != std::string::npos) continue;
        ImGuiMCP::Text(line.c_str());
    }
}

void UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    }

    filter = new ImGuiMCP::ImGuiTextFilter();
    filter2 = new ImGuiMCP::ImGuiTextFilter();

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
}

void UI::ExcludeList() {
    ImGuiMCP::Text("");
    ImGuiMCP::Text("Exclusions per Module:");

    for (const auto& [qform, excludes] : Settings::exclude_list) {
        if (ImGuiMCP::CollapsingHeader(qform.c_str())) {
            if (ImGuiMCP::BeginTable(("#exclude_" + qform).c_str(), 1, table_flags)) {
                ImGuiMCP::TableNextRow();
                ImGuiMCP::TableNextColumn();
                for (const auto& exclude : excludes) {
                    ImGuiMCP::Text(exclude.c_str());
                }

                ImGuiMCP::EndTable();
            }
        }
    }
}

void UI::IniSettingToggle(bool& setting, const std::string& setting_name, const std::string& section_name,
                          const char* desc) {
    const bool previous_state = setting;
    ImGuiMCP::Checkbox(setting_name.c_str(), &setting);
    if (setting != previous_state) {
        // save to INI
        Settings::INI_settings[section_name][setting_name] = setting;
        CSimpleIniA ini;
        ini.SetUnicode();
        ini.LoadFile(Settings::INI_path);
        ini.SetBoolValue(section_name.c_str(), setting_name.c_str(), setting);
        ini.SaveFile(Settings::INI_path);
    }
    ImGuiMCP::SameLine();
    if (desc) HelpMarker(desc);
}

void UI::DrawFilter1() {
    if (filter->Draw("Location filter", 200)) {
        if (!filter->PassFilter(item_current.c_str())) {
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

void UI::DrawFilter2() {
    if (filter2->Draw("Item filter", 200)) {
        if (!filter2->PassFilter(sub_item_current.c_str())) UpdateSubItem();
    }
}

bool UI::DrawFilterModule() {
    ImGuiMCP::Text("Module Filter:");
    ImGuiMCP::SameLine();
    ImGuiMCP::SetNextItemWidth(180);
    if (ImGuiMCP::BeginCombo("##combo 3", filter_module.c_str())) {
        if (ImGuiMCP::Selectable("None", filter_module == "None")) {
            filter_module = "None";
        }
        for (const auto& [module_name, module_enabled] : Settings::INI_settings["Modules"]) {
            if (!module_enabled) continue;
            if (ImGuiMCP::Selectable(module_name.c_str(), filter_module == module_name)) {
                filter_module = module_name;
            }
        }
        ImGuiMCP::EndCombo();
        return true;
    }
    return false;
}

void UI::UpdateSubItem() {
    for (const auto& key2 : locations[item_current] | std::views::keys) {
        if (filter2->PassFilter(key2.c_str())) {
            sub_item_current = key2;
            break;
        }
    }
}

void UI::UpdateLocationMap(const std::vector<Source>& sources) {
    locations.clear();

    for (const auto& source : sources) {
        for (const auto& [location, instances] : source.data) {
            const auto* locationReference = RE::TESForm::LookupByID<RE::TESObjectREFR>(location);
            std::string locationName_temp;
            if (locationReference) {
                locationName_temp = locationReference->GetName();
                if (locationReference->HasContainer()) locationName_temp += std::format(" ({:x})", location);
            } else {
                locationName_temp = std::format("{:x}", location);
            }
            const char* locationName = locationName_temp.c_str();
            for (auto& stageInstance : instances) {
                const auto* delayerForm = RE::TESForm::LookupByID(stageInstance.GetDelayerFormID());
                auto delayer_name = delayerForm
                                        ? delayerForm->GetName()
                                        : std::format("{:x}", stageInstance.GetDelayerFormID());
                if (delayer_name == "0") delayer_name = "None";
                int max_stage_no = 0;
                while (source.IsStageNo(max_stage_no + 1)) max_stage_no++;

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
                locations[std::string(locationName) + "##location"][
                        (item ? item->GetName() : source.editorid) + "##item"]
                    .push_back(
                        instance);
            }
        }
    }

    if (const auto current = locations.find(item_current); current == locations.end()) {
        item_current = "##current";
        sub_item_current = "##item";
    } else if (const auto item = current->second; !item.contains(sub_item_current)) {
        sub_item_current = "##item";
    }
}

void UI::UpdateStages(const std::vector<Source>& sources) {
    mcp_sources.clear();

    for (const auto& source : sources) {
        if (!source.IsHealthy()) continue;
        StageNo max_stage_no = 0;
        std::set<Stage> temp_stages;
        while (source.IsStageNo(max_stage_no)) {
            if (const auto* stage = source.TryGetStage(max_stage_no)) {
                const auto* temp_form = RE::TESForm::LookupByID(stage->formid);
                if (!temp_form) continue;
                const GameObject item = {temp_form->GetName(), stage->formid};
                temp_stages.insert(Stage(item, stage->name, stage->duration, source.IsFakeStage(max_stage_no),
                                         stage->crafting_allowed, max_stage_no));
            }
            max_stage_no++;
        }
        const auto& stage = source.GetDecayedStage();
        if (const auto* temp_form = RE::TESForm::LookupByID(stage.formid)) {
            const GameObject item = {.name = temp_form->GetName(), .formid = stage.formid};
            temp_stages.insert(Stage(item, "Final", 0.f, source.IsFakeStage(max_stage_no), stage.crafting_allowed,
                                     max_stage_no));
        }
        std::set<GameObject> containers_;
        for (const auto& container : source.settings.containers) {
            const auto temp_formid = container;
            const auto temp_name = GetName(temp_formid);
            containers_.insert(GameObject{temp_name, temp_formid});
        }

        std::set<GameObject> transformers_;
        std::map<FormID, GameObject> transformer_enditems_;
        std::map<FormID, Duration> transform_durations_;
        for (const auto& [fst, snd] : source.settings.transformers) {
            auto temp_formid = fst;
            const auto temp_name = GetName(temp_formid);
            transformers_.insert(GameObject{temp_name, temp_formid});
            const auto temp_formid2 = std::get<0>(snd);
            auto temp_name2 = GetName(temp_formid2);
            transformer_enditems_[temp_formid] = GameObject{temp_name2, temp_formid2};
            transform_durations_[temp_formid] = std::get<1>(snd);
        }
        std::set<GameObject> time_modulators_;
        std::map<FormID, float> time_modulator_multipliers_;
        for (const auto& [fst, snd] : source.settings.delayers) {
            auto temp_formid = fst;
            const auto temp_form = RE::TESForm::LookupByID(temp_formid);
            const auto temp_name = temp_form ? temp_form->GetName() : std::format("{:x}", temp_formid);
            time_modulators_.insert(GameObject{temp_name, temp_formid});
            time_modulator_multipliers_[temp_formid] = snd;
        }

        const auto qform_type = Settings::GetQFormType(source.formid);
        mcp_sources.push_back(MCPSource{temp_stages, containers_, transformers_, transformer_enditems_,
                                        transform_durations_, time_modulators_, time_modulator_multipliers_,
                                        qform_type});
    }
}

void UI::RefreshButton() {
    FontAwesome::PushSolid();

    if (ImGuiMCP::Button((FontAwesome::UnicodeToUtf8(0xf021) + " Refresh").c_str()) || last_generated.empty()) {
        Refresh();
    }
    FontAwesome::Pop();

    ImGuiMCP::SameLine();
    ImGuiMCP::Text(("Last Generated: " + last_generated).c_str());
}

void UI::Refresh() {
    last_generated = std::format("{} (in-game hours)", RE::Calendar::GetSingleton()->GetHoursPassed());
    dynamic_forms.clear();
    for (const auto DFT = DynamicFormTracker::GetSingleton(); const auto& df : DFT->GetDynamicForms()) {
        if (const auto form = RE::TESForm::LookupByID(df); form) {
            auto status = DFT->IsActive(df) ? 2 : DFT->IsProtected(df) ? 1 : 0;
            dynamic_forms[df] = {form->GetName(), status};
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
        } else {
            update_q[refid] = std::make_pair(std::format("{:x}", refid), stop_time);
        }
    }
}

std::string UI::GetName(FormID formid) {
    const auto temp_form = RE::TESForm::LookupByID(formid);
    auto temp_name = temp_form ? temp_form->GetName() : std::format("{:x}", formid);
    if (temp_name.empty()) temp_name = clib_util::editorID::get_editorID(temp_form);
    if (temp_name.empty()) temp_name = "???";
    return temp_name;
}