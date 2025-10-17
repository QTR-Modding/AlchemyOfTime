#include "Lorebox.h"
#include "Manager.h"
#include "Utils.h"

namespace {

    std::wstring Widen(const std::string& s) { return { s.begin(), s.end() }; }

    std::wstring FormNameW(FormID fid) {
        if (!fid) return L"None";
        if (auto f = RE::TESForm::LookupByID(fid)) {
            const char* nm = f->GetName();
            if (nm && std::strlen(nm) > 0) return Widen(nm);
            auto ed = clib_util::editorID::get_editorID(f);
            if (!ed.empty()) return Widen(ed);
            return std::format(L"{:08X}", fid);
        }
        return std::format(L"{:08X}", fid);
    }

    std::wstring HrsMinsW(float hours) {
        if (hours < 0.f) return L"now";
        const int total = static_cast<int>(std::round(hours * 60.f));
        const int h = total / 60;
        const int m = total % 60;
        if (h <= 0) return std::format(L"{}m", m);
        if (m == 0) return std::format(L"{}h", h);
        return std::format(L"{}h {}m", h, m);
    }

    std::wstring StageNameOrW(const Source& src, StageNo no, std::wstring fallback) {
        const auto n = src.GetStageName(no);
        if (!n.empty()) return Widen(n);
        return fallback;
    }

    std::wstring NextLabelW(const Source& src, const StageInstance& st) {
        if (st.xtra.is_transforming) {
            const auto tr = st.GetDelayerFormID();
            if (tr && src.settings.transformers.contains(tr)) {
                const auto endForm = src.settings.transformers.at(tr).first;
                // ASCII fallback instead of Unicode arrow
                return std::format(L"-> {}", FormNameW(endForm));
            }
            return L"-> (transform)";
        }
        const auto slope = st.GetDelaySlope();
        if (std::abs(slope) < EPSILON) return L"Frozen";
        if (slope > 0) {
            if (src.IsStageNo(st.no + 1)) {
                return std::format(L"-> {}", StageNameOrW(src, st.no + 1, std::format(L"Stage {}", st.no + 1)));
            }
            return L"-> Final";
        }
        if (st.no > 0 && src.IsStageNo(st.no - 1)) {
            return std::format(L"<- {}", StageNameOrW(src, st.no - 1, std::format(L"Stage {}", st.no - 1)));
        }
        return L"<- Initial";
    }

    struct Row {
        Count count{};
        int minutes{-1};
        float slope{1.f};
        FormID mod{0};
        bool transforming{false};
        std::wstring next;
    };
}

std::wstring Lorebox::BuildLoreForHover()
{
    std::string menu_name;
    auto item_data = Menu::GetSelectedItemDataInMenu(menu_name);
	if (!item_data) {
		logger::error("No selected item data in menu '{}'.", menu_name);
		return L"";
	}

	const FormID hovered = item_data->objDesc->GetObject()->GetFormID();
    if (!hovered) return L"";

    // Snapshot sources safely
    const auto sources = M->GetSources();

    const Source* src = nullptr;
    for (const auto& s : sources) {
        if (!s.IsHealthy()) continue;
        if (s.IsStage(hovered)) { src = &s; break; }
    }
    if (!src) return L"";

    RE::NiPointer<RE::TESObjectREFR> owner;
    if (!LookupReferenceByHandle(item_data->owner,owner)) {
        logger::error("Could not find owner reference for item data.");
		return L"";
    }
    const auto ownerId = owner ? owner->GetFormID() : 0;
    if (!ownerId || !src->data.contains(ownerId)) {
		logger::error("No data for owner {:08X} in source {:08X}.", ownerId, src->formid);
        return L"";
    }

    const auto now = RE::Calendar::GetSingleton()->GetHoursPassed();

    // Collect rows for hovered base item (one row per StageInstance)
    std::vector<Row> rows;
    rows.reserve(16);

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
        r.minutes = (std::abs(r.slope) < EPSILON) ? -1
                   : (nextT > 0.f ? static_cast<int>(std::round((nextT - now) * 60.f)) : -1);

        rows.push_back(std::move(r));
    }

    if (rows.empty()) return L"";

    // Sort by ETA (frozen/unknown at the end), then by target label
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        const int am = a.minutes < 0 ? INT_MAX : a.minutes;
        const int bm = b.minutes < 0 ? INT_MAX : b.minutes;
        if (am != bm) return am < bm;
        if (a.transforming != b.transforming) return a.transforming > b.transforming; // transforming first
        return a.next < b.next;
    });

    // Build multi-line body (each StageInstance on a separate line) without trailing <br>
    std::wstring out;
    out.reserve(512);

    int printed = 0;
    constexpr int MAX_ROWS = 8;
    bool firstLine = true;
    for (const auto& r : rows) {
        if (printed >= MAX_ROWS) break;

        const auto eta = (r.minutes < 0) ? L"now" : HrsMinsW(r.minutes / 60.f);

        std::wstring modTag;
        if (r.transforming) {
            if (r.mod) modTag = std::format(L" [T: {}]", FormNameW(r.mod));
        } else if (r.mod) {
            if (src->settings.delayers.contains(r.mod)) {
                modTag = std::format(L" [x{:.2f}]", r.slope);
            }
        } else if (std::abs(r.slope - 1.f) > 0.01f) {
            modTag = std::format(L" [x{:.2f}]", r.slope);
        }

        if (!firstLine) out += L"<br>";
        firstLine = false;

        out += std::format(L"{}x {} {}", r.count, r.next, eta);
        out += modTag;
        ++printed;
    }

    const auto remaining = static_cast<int>(rows.size()) - printed;
    if (remaining > 0) {
        if (!firstLine) out += L"<br>";
        out += std::format(L"+{}...", remaining);
    }

    return out;
}