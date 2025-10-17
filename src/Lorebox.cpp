#include "Lorebox.h"
#include "Hooks.h"
#include "Manager.h"
#include "Utils.h"
#include "MCP.h"

namespace {
    // Resolve whose inventory we’re showing
    RE::TESObjectREFR* ResolveInventoryOwner() {
        if (const auto ui = RE::UI::GetSingleton()) {
            if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
                if (auto ref = Menu::GetContainerFromMenu(); ref) return ref;
            }
            if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
                if (auto ref = Menu::GetVendorChestFromMenu(); ref) return ref;
            }
        }
        return RE::PlayerCharacter::GetSingleton()->AsReference();
    }

    // Color helpers
    std::wstring Hex6(uint32_t rgba) {
        uint32_t rgb = (rgba > 0xFFFFFF) ? (rgba >> 8) : rgba;
        return std::format(L"#{:06X}", rgb & 0xFFFFFF);
    }

    // Name helpers (very simple widening; Skyrim names are mostly ASCII)
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
                return std::format(L"\u2192 {}", FormNameW(endForm)); // →
            }
            return L"\u2192 (transform)";
        }
        const auto slope = st.GetDelaySlope();
        if (std::abs(slope) < EPSILON) return L"Frozen (x0)";
        if (slope > 0) {
            if (src.IsStageNo(st.no + 1)) {
                return std::format(L"\u2192 {}", StageNameOrW(src, st.no + 1, std::format(L"Stage {}", st.no + 1)));
            }
            return L"\u2192 Final";
        }
        if (st.no > 0 && src.IsStageNo(st.no - 1)) {
            return std::format(L"\u2190 {}", StageNameOrW(src, st.no - 1, std::format(L"Stage {}", st.no - 1))); // ←
        }
        return L"\u2190 Initial";
    }

    std::wstring BarW(float pct01, const std::wstring& colorHex) {
        pct01 = std::clamp(pct01, 0.f, 1.f);
        int filled = static_cast<int>(std::round(pct01 * 10.f));
        std::wstring bar;
        bar.reserve(10);
        for (int i = 0; i < 10; ++i) bar += (i < filled ? L"\u2588" : L"\u2591"); // █ / ░
        return std::format(L"<font color=\"{}\">{}</font> {}%", colorHex, bar, static_cast<int>(pct01 * 100.f + 0.5f));
    }

    struct Row {
        Count count{};
        int minutes{-1};
        float slope{1.f};
        FormID mod{0};
        bool transforming{false};
        std::wstring next;
        float progressPct{0.f};
        StageNo stage{};
    };
}

std::wstring Lorebox::BuildLoreForHover()
{
    if (!Hooks::allow_translation_hook || !M) return L"";
    const FormID hovered = Hooks::hovered_formid.load(std::memory_order_relaxed);
    if (!hovered) return L"";

    // Snapshot sources safely
    const auto sources = M->GetSources();

    // Find source that owns this base form
    const Source* src = nullptr;
    for (const auto& s : sources) {
        if (!s.IsHealthy()) continue;
        if (s.IsStage(hovered)) { src = &s; break; }
    }
    if (!src) return L"";

    auto* owner = ResolveInventoryOwner();
    const auto ownerId = owner ? owner->GetFormID() : 0u;
    if (!ownerId || !src->data.contains(ownerId)) return L"";

    const auto now = RE::Calendar::GetSingleton()->GetHoursPassed();

    // Collect rows for instances matching this hovered base item
    std::vector<Row> rows;
    Count totalCount = 0;
    bool anyFrozen = false;

    for (const auto& st : src->data.at(ownerId)) {
        if (st.count <= 0) continue;
        if (st.xtra.is_decayed) continue;
        if (st.xtra.form_id != hovered) continue;

        Row r;
        r.count = st.count;
        r.slope = st.GetDelaySlope();
        r.mod = st.GetDelayerFormID();
        r.transforming = st.xtra.is_transforming;
        r.next = NextLabelW(*src, st);
        r.stage = st.no;

        float nextT = src->GetNextUpdateTime(const_cast<StageInstance*>(&st));
        if (std::abs(r.slope) < EPSILON) {
            r.minutes = -1;
            anyFrozen = true;
        } else {
            r.minutes = (nextT > 0.f) ? static_cast<int>(std::round((nextT - now) * 60.f)) : -1;
        }

        // progress within current stage (forward only)
        const float dur = src->GetStageDuration(st.no);
        r.progressPct = (dur > 0.f && r.slope > 0.f) ? std::clamp(st.GetElapsed(now) / dur, 0.f, 1.f) : 0.f;

        totalCount += r.count;
        rows.push_back(std::move(r));
    }

    if (rows.empty()) return L"";

    // Earliest ETA
    int bestMin = INT_MAX;
    for (const auto& r : rows) if (r.minutes >= 0) bestMin = std::min(bestMin, r.minutes);

    // Grouping: by (next label, rounded minutes, slope, mod, transforming)
    struct Key {
        std::wstring next;
        int minutes;
        float slope;
        FormID mod;
        bool transforming;
        bool operator<(const Key& o) const {
            if (next != o.next) return next < o.next;
            if (minutes != o.minutes) return minutes < o.minutes;
            if (transforming != o.transforming) return transforming < o.transforming;
            if (slope != o.slope) return slope < o.slope;
            return mod < o.mod;
        }
    };
    std::map<Key, Count> grouped;
    for (const auto& r : rows) {
        grouped[{r.next, r.minutes, r.slope, r.mod, r.transforming}] += r.count;
    }

    // Build HTML body
    // Palette
    const auto cMuted   = L"#A0AEC0";
    const auto cAccent  = L"#FFD54F";
    const auto cGood    = L"#48BB78";
    const auto cWarn    = L"#ED8936";
    const auto cInfo    = L"#63B3ED";

    std::wstring body;
    body.reserve(1024);

    // Header
    body += std::format(L"<b><font color=\"{}\">Alchemy of Time</font></b><br>", cAccent);
    body += std::format(L"<font color=\"{}\">Total:</font> <b>{}x</b><br>", cMuted, totalCount);
    if (bestMin != INT_MAX) {
        body += std::format(L"<font color=\"{}\">Earliest:</font> {}<br>", cMuted, HrsMinsW(bestMin / 60.f));
    }
    if (anyFrozen) {
        const auto baseId = owner->GetBaseObject() ? owner->GetBaseObject()->GetFormID() : 0u;
        if (baseId && src->ShouldFreezeEvolution(baseId)) {
            body += std::format(L"<font color=\"{}\">Frozen:</font> outside allowed containers<br>", cWarn);
        } else {
            body += std::format(L"<font color=\"{}\">Frozen</font><br>", cWarn);
        }
    }

    // Grouped lines
    int lines = 0;
    constexpr int MAX_LINES = 6;
    for (const auto& [k, cnt] : grouped) {
        if (lines >= MAX_LINES) break;

        std::wstring modPart;
        if (k.transforming) {
            if (k.mod) modPart = std::format(L" <font color=\"{}\">[transformer: {}</font>]", cInfo, FormNameW(k.mod));
        } else if (k.mod) {
            if (src->settings.transformers.contains(k.mod)) {
                modPart = std::format(L" <font color=\"{}\">[transformer: {}</font>]", cInfo, FormNameW(k.mod));
            } else if (src->settings.delayers.contains(k.mod)) {
                modPart = std::format(L" <font color=\"{}\">[x{:.2f} via {}]</font>", cGood, k.slope, FormNameW(k.mod));
            } else {
                modPart = std::format(L" <font color=\"{}\">[x{:.2f}]</font>", cGood, k.slope);
            }
        } else if (std::abs(k.slope - 1.f) > 0.01f) {
            modPart = std::format(L" <font color=\"{}\">[x{:.2f}]</font>", cGood, k.slope);
        }

        const auto eta = (k.minutes < 0) ? L"now" : HrsMinsW(k.minutes / 60.f);
        body += std::format(L"\u2022 <b>{}x</b> <font color=\"{}\">{}</font> in <b>{}</b>{}<br>",
                            cnt, cInfo, k.next, eta, modPart);
        ++lines;
    }
    const auto remaining = static_cast<int>(grouped.size()) - lines;
    if (remaining > 0) {
        body += std::format(L"<font color=\"{}\">+{} more…</font><br>", cMuted, remaining);
    }

    // Per-instance progress (first 3 by earliest ETA)
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return (a.minutes < b.minutes);
    });
    int progShown = 0;
    for (const auto& r : rows) {
        if (progShown >= 3) break;
        // pick a color for bar: forward green, backward warn, frozen muted
        const std::wstring barColor = (std::abs(r.slope) < EPSILON) ? cMuted : (r.slope > 0 ? cGood : cWarn);
        const auto bar = BarW(r.progressPct, barColor);
        body += std::format(L"<font color=\"{}\">Stage {}</font> {}<br>", cMuted, r.stage, bar);
        ++progShown;
    }

    return body;
}