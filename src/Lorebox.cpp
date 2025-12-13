#include "Lorebox.h"
#include "Manager.h"
#include "Utils.h"

namespace {
    std::wstring Widen(const std::string& s) { return {s.begin(), s.end()}; }

    std::wstring HexColor(const uint32_t rgb) {
        return std::format(L"#{:06X}", rgb & 0xFFFFFF);
    }

    constexpr auto INFINITY_SYM = L"~"; // ∞

    std::wstring FormNameW(const FormID fid) {
        if (!fid) return L"";
        if (const auto f = RE::TESForm::LookupByID(fid)) {
            const char* nm = f->GetName();
            if (nm && std::strlen(nm) > 0) return Widen(nm);
        }
        return std::format(L"");
    }

    std::wstring HrsMinsW(const float hours) {
        // If negative (due/past), show as 0 seconds instead of "Now"
        if (hours < 0.f) return L"0s";
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

    struct Transition {
        FormID nextFormId{0};
        bool hasNext{false};
        bool transforming{false};
        bool backwards{false};
        std::wstring label; // pre-formatted label with arrow and target name
    };

    Transition ComputeNext(const Source& src, const StageInstance& st) {
        Transition t{};
        if (st.xtra.is_transforming) {
            t.transforming = true;
            const auto tr = st.GetDelayerFormID();
            if (tr && src.settings.transformers.contains(tr)) {
                t.nextFormId = src.settings.transformers.at(tr).first;
                t.hasNext = t.nextFormId != 0;
                t.label = std::format(L"{} {}", Lorebox::arrow_right, FormNameW(t.nextFormId));
            } else {
                t.label = std::format(L"{} (transform)", Lorebox::arrow_right);
            }
            return t;
        }

        const float slope = st.GetDelaySlope();
        if (std::abs(slope) < EPSILON) {
            // Frozen; keep empty label as current implementation
            return t;
        }

        if (slope > 0.f) {
            // Forward
            if (src.IsStageNo(st.no + 1)) {
                const auto ns = src.GetStage(st.no + 1);
                t.nextFormId = ns->formid;
                t.hasNext = true;
                t.label = std::format(L"{} {}",
                                      Lorebox::arrow_right,
                                      StageNameOrW(src, st.no + 1, std::format(L"Stage {}", st.no + 1)));
            } else {
                t.nextFormId = src.settings.decayed_id;
                t.hasNext = t.nextFormId != 0;
                const auto decayed_form = RE::TESForm::LookupByID(t.nextFormId);
                t.label = std::format(L"{} {}", Lorebox::arrow_right,
                                      decayed_form ? Widen(decayed_form->GetName()) : L"");
            }
        } else {
            // Backwards
            t.backwards = true;
            if (st.no > 0 && src.IsStageNo(st.no - 1)) {
                const auto ps = src.GetStage(st.no - 1);
                t.nextFormId = ps->formid;
                t.hasNext = true;
                t.label = std::format(L"{} {}",
                                      Lorebox::arrow_left,
                                      StageNameOrW(src, st.no - 1, std::format(L"Stage {}", st.no - 1)));
            } else {
                t.label = std::format(L"{} Initial", Lorebox::arrow_left);
            }
        }
        return t;
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
        std::wstring curr; // current stage name if available
    };
}

bool Lorebox::AddKeyword(RE::BGSKeywordForm* a_form, FormID a_formid) {
    if (std::shared_lock lock(kw_mutex);
        kw_added.contains(a_formid))
        return false; // already added

    if (!a_form->HasKeyword(aot_kw)) {
        if (!a_form->AddKeyword(aot_kw)) {
            logger::error("Failed to add keyword to formid: {:x}", a_formid);
        } else {
            std::unique_lock ulock(kw_mutex);
            kw_added.insert(a_formid);
            kw_removed.erase(a_formid);
            return true;
        }
    }
    return false;
}

bool Lorebox::ReAddKW(RE::TESForm* a_form) {
    const auto kw_form = a_form->As<RE::BGSKeywordForm>();

    if (!kw_form) return false;

    const auto a_formid = a_form->GetFormID();

    if (std::shared_lock lock(kw_mutex);
        !kw_removed.contains(a_formid)) {
        return false; // not removed
    }

    return AddKeyword(kw_form, a_formid);
}

bool Lorebox::RemoveKW(RE::TESForm* a_form) {
    if (std::shared_lock lock(kw_mutex);
        !kw_added.contains(a_form->GetFormID()))
        return false; // not added

    const auto kw_form = a_form->As<RE::BGSKeywordForm>();

    if (!kw_form) return false;

    if (kw_form->HasKeyword(aot_kw)) {
        if (!kw_form->RemoveKeyword(aot_kw)) {
            logger::error("Failed to remove keyword from formid: {:x}", a_form->GetFormID());
            return false;
        }
        const auto a_formid = a_form->GetFormID();
        std::unique_lock ulock(kw_mutex);
        kw_removed.insert(a_formid);
        kw_added.erase(a_formid);
        return true;
    }
    return false;
}

void Lorebox::ReAddKWs() {
    std::vector<std::pair<RE::BGSKeywordForm*, FormID>> to_add;
    {
        std::unique_lock lock(kw_mutex);
        for (const auto& formid : kw_removed) {
            if (const auto form = RE::TESForm::LookupByID(formid)) {
                if (const auto kw_form = form->As<RE::BGSKeywordForm>()) {
                    to_add.push_back({kw_form, formid});
                }
            }
        }
        kw_removed.clear();
    }

    for (const auto& [kw_form, formid] : to_add) {
        AddKeyword(kw_form, formid);
    }
}

bool Lorebox::HasKW(const RE::TESForm* a_form) {
    std::shared_lock lock(kw_mutex);
    return kw_added.contains(a_form->GetFormID());
}

bool Lorebox::IsRemoved(const FormID a_formid) {
    std::shared_lock lock(kw_mutex);
    return kw_removed.contains(a_formid);
}

std::wstring Lorebox::BuildLoreFor(FormID hovered, RefID ownerId) {
    // Snapshot sources safely
    const auto sources = M->GetSources();

    const Source* src = nullptr;
    for (const auto& s : sources) {
        if (!s.IsHealthy()) continue;
        if (s.IsStage(hovered)) {
            src = &s;
            break;
        }
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

            // Compute next transition once and reuse
            const auto trans = ComputeNext(*src, st);
            if (trans.nextFormId != 0 && trans.nextFormId == st.xtra.form_id) {
                // Frozen lore
                std::wstring stageNameW;
                if (const auto name = src->GetStageName(st.no); !name.empty()) {
                    stageNameW = std::wstring(name.begin(), name.end());
                }
                return BuildFrozenLore(stageNameW);
            }

            Row r;
            r.count = st.count;
            r.slope = st.GetDelaySlope();
            r.mod = st.GetDelayerFormID();
            r.transforming = st.xtra.is_transforming;
            r.next = trans.label;
            if (const auto curName = src->GetStageName(st.no); !curName.empty()) {
                r.curr = Widen(curName);
            }

            const float nextT = src->GetNextUpdateTime(&st);
            r.minutes = std::abs(r.slope) < EPSILON
                            ? -1
                            : nextT > 0.f
                            ? static_cast<int>(std::round((nextT - now) * 60.f))
                            : -1;

            // Compute percentage
            if (show_percentage.load(std::memory_order_relaxed)) {
                float pct = -1.f;
                if (st.xtra.is_transforming) {
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

            // If due/past (negative remaining) and not frozen, force to 0s and 100%
            if (r.minutes < 0 && std::abs(r.slope) >= EPSILON) {
                r.pct = 100;
            }

            // Collect mod/transformer name for separate line if enabled
            if (auto nm = FormNameW(r.mod); !nm.empty()) {
                if (r.transforming) {
                    if (show_modulator_name.load(std::memory_order_relaxed)) {
                        r.tag = std::format(L"[{}]", nm);
                    }
                } else {
                    if (show_modulator_name.load(std::memory_order_relaxed)) {
                        if (show_multiplier.load(std::memory_order_relaxed)) {
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
    if (show_title.load(std::memory_order_relaxed)) {
        const auto titleHex = HexColor(color_title.load());
        out += std::format(L"<b><font color=\"{}\">Alchemy of Time</font></b>", titleHex);
        out += L"<br>";
    }

    // Colors
    const auto HEX_NEUTRAL = HexColor(color_neutral.load());
    const auto HEX_SLOW = HexColor(color_slow.load());
    const auto HEX_FAST = HexColor(color_fast.load());
    const auto HEX_TRANSFORM = HexColor(color_transform.load());

    int printed = 0;

    bool firstLine = true;
    const auto HEX_SEP = HexColor(color_separator.load());
    for (const auto& r : rows) {
        if (printed >= MAX_ROWS) break;

        const std::wstring eta = std::abs(r.slope) < EPSILON ? std::wstring{INFINITY_SYM} : HrsMinsW(r.minutes / 60.f);

        // choose color if enabled
        const bool doColors = colorize_rows.load(std::memory_order_relaxed);
        const std::wstring* rowColor = &HEX_NEUTRAL;
        if (doColors) {
            if (r.transforming) rowColor = &HEX_TRANSFORM;
            else if (std::abs(r.slope - 1.f) < 0.01f) rowColor = &HEX_NEUTRAL;
            else if (r.slope > 1.f) rowColor = &HEX_SLOW;
            else if (r.slope < 1.f) rowColor = &HEX_FAST;
        }

        if (!firstLine) {
            // Insert a separator symbol between lines
            out += std::format(L"<br><font color=\"{}\">{}</font><br>", HEX_SEP, separator_symbol);
        }
        firstLine = false;

        std::wstring line;
        if (show_percentage.load(std::memory_order_relaxed) && r.pct >= 0) {
            if (!r.curr.empty()) {
                line = std::format(L"{}x {} {} | {} ({}%)", r.count, r.curr, r.next, eta, r.pct);
            } else {
                line = std::format(L"{}x {} | {} ({}%)", r.count, r.next, eta, r.pct);
            }
        } else {
            if (!r.curr.empty()) {
                line = std::format(L"{}x {} {} | {}", r.count, r.curr, r.next, eta);
            } else {
                line = std::format(L"{}x {} | {}", r.count, r.next, eta);
            }
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

std::wstring Lorebox::BuildFrozenLore() {
    return BuildFrozenLore(L"");
}

std::wstring Lorebox::BuildFrozenLore(const std::wstring& currentStageName) {
    std::wstring out;

    // Optional title
    if (show_title.load(std::memory_order_relaxed)) {
        const auto titleHex = HexColor(color_title.load());
        out += std::format(L"<b><font color=\"{}\">Alchemy of Time</font></b>", titleHex);
        out += L"<br>";
    }

    const auto HEX_NEUTRAL = HexColor(color_neutral.load());

    // Build single-line body: optional current stage name, then infinity and optional percentage
    std::wstring line;
    if (!currentStageName.empty()) {
        line = std::format(L"{} | {}", currentStageName, INFINITY_SYM);
    } else {
        line = INFINITY_SYM;
    }
    if (show_percentage.load(std::memory_order_relaxed)) {
        line += L" (0%)";
    }

    if (colorize_rows.load(std::memory_order_relaxed)) {
        out += std::format(L"<font color=\"{}\">{}</font>", HEX_NEUTRAL, line);
    } else {
        out += line;
    }

    return out;
}

const wchar_t* Lorebox::OnDynamicTranslationRequest(std::string_view) {
    loreboxStr = BuildFrozenLore();
    const auto item_data = Menu::GetSelectedItemDataInMenu();
    if (!item_data) {
        return loreboxStr.c_str();
    }
    const auto item = item_data->objDesc->GetObject();
    if (!item) {
        return loreboxStr.c_str();
    }
    const auto owner = Menu::GetOwnerOfItem(item_data);
    if (!owner) {
        if (RE::UI::GetSingleton()->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
            // Try to resolve current stage name from the hovered form
            if (const auto hovered = item->GetFormID()) {
                for (const auto& s : M->GetSources()) {
                    if (!s.IsHealthy()) continue;
                    if (!s.IsStage(hovered)) continue;
                    if (const auto name = s.GetStageName(s.GetStageNo(hovered)); !name.empty()) {
                        loreboxStr = BuildFrozenLore(std::wstring{name.begin(), name.end()});
                        return loreboxStr.c_str();
                    }
                    break; // found source, but no name
                }
            }
            loreboxStr = BuildFrozenLore();
            return loreboxStr.c_str();
        }
        return loreboxStr.c_str();
    }
    loreboxStr = BuildLoreFor(item->GetFormID(), owner->GetFormID());
    return loreboxStr.c_str();
}