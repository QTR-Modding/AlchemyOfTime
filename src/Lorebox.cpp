#include "Lorebox.h"

#include "Hooks.h"
#include "Manager.h"
#include "Utils.h"

namespace {

    std::wstring Widen(const std::string& s) { return { s.begin(), s.end() }; }

    std::wstring HexColor(const uint32_t rgb) {
        return std::format(L"#{:06X}", rgb & 0xFFFFFF);
    }

    std::wstring FormNameW(FormID fid) {
        if (!fid) return L"";
        if (auto f = RE::TESForm::LookupByID(fid)) {
            const char* nm = f->GetName();
            if (nm && std::strlen(nm) > 0) return Widen(nm);
        }
        return std::format(L"");
    }

    std::wstring HrsMinsW(const float hours) {
        if (hours < 0.f) return L"now";
        const int totalM = static_cast<int>(std::round(hours * 60.f));
        constexpr int minsPerDay = 24 * 60;
        const int d = totalM / minsPerDay;
        const int remAfterDays = totalM % minsPerDay;
        const int h = remAfterDays / 60;
        const int m = remAfterDays % 60;

        if (d > 0) {
            // Build "Xd Yh Zm" omitting zero parts
            std::wstring out;
            if (d > 0) out += std::format(L"{}d", d);
            if (h > 0) out += (out.empty() ? L"" : L" ") + std::format(L"{}h", h);
            if (m > 0) out += (out.empty() ? L"" : L" ") + std::format(L"{}m", m);
            if (out.empty()) return L"0m"; // should not happen, but fallback
            return out;
        }

        if (h <= 0) return std::format(L"{}m", m);
        if (m == 0) return std::format(L"{}h", h);
        return std::format(L"{}h {}m", h, m);
    }

    std::wstring StageNameOrW(const Source& src, const StageNo no, std::wstring fallback) {
        const auto n = src.GetStageName(no);
        if (!n.empty()) return Widen(n);
        return fallback;
    }

    std::wstring NextLabelW(const Source& src, const StageInstance& st) {
        if (st.xtra.is_transforming) {
            const auto tr = st.GetDelayerFormID();
            if (tr && src.settings.transformers.contains(tr)) {
                const auto endForm = src.settings.transformers.at(tr).first;
                // Use configured arrow
                return std::format(L"{} {}", Lorebox::arrow_right, FormNameW(endForm));
            }
            return std::format(L"{} (transform)", Lorebox::arrow_right);
        }
        const auto slope = st.GetDelaySlope();
        if (std::abs(slope) < EPSILON) return L"";
        if (slope > 0) {
            if (src.IsStageNo(st.no + 1)) {
                return std::format(L"{} {}", Lorebox::arrow_right, StageNameOrW(src, st.no + 1, std::format(L"Stage {}", st.no + 1)));
            }
			auto decayed_form = RE::TESForm::LookupByID(src.settings.decayed_id);
			return std::format(L"{} {}", Lorebox::arrow_right,Widen(decayed_form->GetName())); // TODO: give custom name for final stage
        }
        if (st.no > 0 && src.IsStageNo(st.no - 1)) {
            return std::format(L"{} {}", Lorebox::arrow_left, StageNameOrW(src, st.no - 1, std::format(L"Stage {}", st.no - 1)));
        }
        return std::format(L"{} Initial", Lorebox::arrow_left);
    }

    struct Row {
        Count count{};
        int minutes{-1};
        float slope{1.f};
        FormID mod{0};
        bool transforming{false};
        std::wstring next;
        // percentage in range [0,100]
        int pct{-1};
        std::wstring tag; // bracketed name (and optional multiplier)
    };
}

std::wstring Lorebox::BuildLoreForHover()
{
	// in case it fails we return just the title

	// prepare title in case of early return


    auto return_str = L" ";

    std::string menu_name;
    auto item_data = Menu::GetSelectedItemDataInMenu(menu_name);
    if (!item_data) {
        logger::error("No selected item data in menu '{}'.", menu_name);
        return return_str;
    }

    auto a_form = item_data->objDesc->GetObject();
    const FormID hovered = a_form->GetFormID();
    if (!hovered) return return_str;

    if (Hooks::is_barter_menu_open) {
        if (RemoveKeyword(a_form)) {
            RE::SendUIMessage::SendInventoryUpdateMessage(RE::TESObjectREFR::LookupByHandle(RE::UI::GetSingleton()->GetMenu<RE::BarterMenu>()->GetTargetRefHandle()).get(),nullptr);
        }
		return return_str;
    }


    auto owner = Menu::GetOwnerOfItem(item_data);
	FormID ownerId = owner ? owner->GetFormID() : 0;

    // Snapshot sources safely
    const auto sources = M->GetSources();

    const Source* src = nullptr;
    for (const auto& s : sources) {
        if (!s.IsHealthy()) continue;
        if (s.IsStage(hovered)) { src = &s; break; }
    }
    if (!src) return return_str;

    const auto now = RE::Calendar::GetSingleton()->GetHoursPassed();

    // Collect rows for hovered base item (one row per StageInstance)
    std::vector<Row> rows;
    rows.reserve(16);

    if (ownerId > 0 && src->data.contains(ownerId)) {
        for (const auto& st : src->data.at(ownerId)) {
            if (st.count <= 0 || st.xtra.is_decayed) continue;
            if (st.xtra.form_id != hovered) continue;

            Row r;
            r.count = st.count;
            r.slope = st.GetDelaySlope();
            r.mod = st.GetDelayerFormID();
            r.transforming = st.xtra.is_transforming;
            r.next = NextLabelW(*src, st);

            const float nextT = src->GetNextUpdateTime(const_cast<StageInstance*>(&st));
            r.minutes = std::abs(r.slope) < EPSILON ? -1
                       : nextT > 0.f ? static_cast<int>(std::round((nextT - now) * 60.f)) : -1;

            // Compute percentage
            if (Lorebox::show_percentage.load(std::memory_order_relaxed)) {
                float pct = -1.f;
                if (st.xtra.is_transforming) {
                    // Transform progress = elapsed since transform / total transform duration
                    const auto tr = st.GetDelayerFormID();
                    if (tr && src->settings.transformers.contains(tr)) {
                        const float elapsed = st.GetTransformElapsed(now);
                        const float total = std::get<1>(src->settings.transformers.at(tr));
                        if (total > 0.f) pct = std::clamp(elapsed / total * 100.f, 0.f, 100.f);
                    }
                } else {
                    // Regular stage progress = elapsed / current stage duration
                    if (src->IsStageNo(st.no)) {
                        const float elapsed = st.GetElapsed(now);
                        const float total = src->GetStageDuration(st.no);
                        if (total > 0.f) pct = std::clamp(elapsed / total * 100.f, 0.f, 100.f);
                    }
                }
                if (pct >= 0.f) r.pct = static_cast<int>(std::round(pct));
            }

            // Collect mod/transformer name for separate line if enabled
            if (auto nm = FormNameW(r.mod); !nm.empty()) {
                if (r.transforming) {
                    if (Lorebox::show_modulator_name.load(std::memory_order_relaxed)) {
                        if (r.mod) {
                            r.tag = std::format(L"[{}]", nm);
                        }
                    }
                } else {
                    if (r.mod && Lorebox::show_modulator_name.load(std::memory_order_relaxed)) {
                        if (Lorebox::show_multiplier.load(std::memory_order_relaxed)) {
                            // Append multiplier if known; fall back to slope if not found in settings
                            float mult = r.slope;
                            if (src->settings.delayers.contains(r.mod)) {
                                mult = src->settings.delayers.at(r.mod);
                            }
                            nm += std::format(L": x{:.2f}", mult);
                        }
                        r.tag = std::format(L"[{}]", nm);
                    }
                }
            }

            rows.push_back(std::move(r));
        }
    }

    if (rows.empty()) return return_str;

    // Sort by ETA (frozen/unknown at the end), then by target label
    std::ranges::sort(rows, [](const Row& a, const Row& b) {
        const int am = a.minutes < 0 ? INT_MAX : a.minutes;
        const int bm = b.minutes < 0 ? INT_MAX : b.minutes;
        if (am != bm) return am < bm;
        if (a.transforming != b.transforming) return a.transforming > b.transforming; // transforming first
        return a.next < b.next;
    });
        
    // Build multi-line body (each StageInstance on a separate line) with a bullet between rows
    std::wstring out;
    out.reserve(512);

    // Optional title
    if (Lorebox::show_title.load(std::memory_order_relaxed)) {
        const auto titleHex = HexColor(Lorebox::color_title.load());
		out += std::format(L"<b><font color=\"{}\">Alchemy of Time</font></b>", titleHex);
        out += L"<br>";
    }

    // Colors
    const auto HEX_NEUTRAL   = HexColor(Lorebox::color_neutral.load());
    const auto HEX_SLOW      = HexColor(Lorebox::color_slow.load());
    const auto HEX_FAST      = HexColor(Lorebox::color_fast.load());
    const auto HEX_TRANSFORM = HexColor(Lorebox::color_transform.load());

    int printed = 0;
    constexpr int MAX_ROWS = 8;
    bool firstLine = true;
    const auto HEX_SEP = HexColor(Lorebox::color_separator.load());
    for (const auto& r : rows) {
        if (printed >= MAX_ROWS) break;

        const auto eta = r.minutes < 0 ? L"Now" : HrsMinsW(r.minutes / 60.f);

        // choose color if enabled
        const bool doColors = Lorebox::colorize_rows.load(std::memory_order_relaxed);
        const std::wstring* rowColor = &HEX_NEUTRAL;
        if (doColors) {
            if (r.transforming) rowColor = &HEX_TRANSFORM;
            else if (std::abs(r.slope - 1.f) < 0.01f) rowColor = &HEX_NEUTRAL;
            else if (r.slope > 1.f) rowColor = &HEX_SLOW;
            else if (r.slope < 1.f) rowColor = &HEX_FAST;
        }

        if (!firstLine) {
            // Insert a separator symbol between lines
            out += std::format(L"<br><font color=\"{}\">{}</font><br>", HEX_SEP, Lorebox::separator_symbol);
        }
        firstLine = false;

        std::wstring line;
        if (Lorebox::show_percentage.load(std::memory_order_relaxed) && r.pct >= 0) {
            line = std::format(L"{}x {} | {} ({}%)", r.count, r.next, eta, r.pct);
        } else {
            line = std::format(L"{}x {} | {}", r.count, r.next, eta);
        }

        if (doColors) {
            out += std::format(L"<font color=\"{}\">{}</font>", *rowColor, line);
        } else {
            out += line;
        }

        // Optional second row for modulator/transformer name
        if (!r.tag.empty()) {
            out += L"<br>";
            if (doColors) {
                out += std::format(L"<font color=\"{}\">{}</font>", *rowColor, r.tag);
            } else {
                out += r.tag;
            }
        }

        ++printed;
    }

    const auto remaining = static_cast<int>(rows.size()) - printed;
    if (remaining > 0) {
        out += L"<br>";
        out += std::format(L"+{}...", remaining);
    }

    return out;
}