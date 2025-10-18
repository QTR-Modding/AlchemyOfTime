#pragma once
#include <shared_mutex>
#include <unordered_set>
#include <atomic>

namespace Lorebox
{
	static std::string aot_kw_name = "LoreBox_quantAoT";
	static std::string aot_kw_name_lorebox = "$LoreBox_quantAoT";
	inline RE::BGSKeyword* aot_kw = nullptr;

	inline std::shared_mutex kw_mutex;
	inline std::unordered_set<FormID> kw_added;

    inline std::unordered_set<FormID> kw_removed;

	// UI toggle: show a title before rows in lorebox
	inline std::atomic show_title{ true };
	// UI toggle: show progress percentage per instance (default true)
	inline std::atomic show_percentage{ true };
	// UI toggle: show delayer (time modulator) and transformer name
	inline std::atomic show_modulator_name{ false };
	// UI toggle: colorize each row based on modulation/transform state
	inline std::atomic colorize_rows{ true };
	// UI toggle: append multiplier to modulator tag (e.g., ": x0.5")
	inline std::atomic show_multiplier{ false };

	// Configurable colors (0xRRGGBB)
	inline std::atomic<uint32_t> color_title{ 0xA86CCD };      // title accent
	inline std::atomic<uint32_t> color_neutral{ 0xE8EAED };    // default light text
	inline std::atomic<uint32_t> color_slow{ 0xFF8A80 };       // light red for x>1 (slower)
	inline std::atomic<uint32_t> color_fast{ 0x80D8FF };       // light blue for x<1 (faster)
	inline std::atomic<uint32_t> color_transform{ 0xFFCC80 };  // light orange for transform
	inline std::atomic<uint32_t> color_separator{ 0x9AA0A6 };  // bullet/separator color

	// Configurable symbols (ASCII recommended for maximum font support)
	inline std::wstring separator_symbol = L"-"; // bullet default
	inline std::wstring arrow_right = L"->";          // forward arrow
	inline std::wstring arrow_left  = L"<-";          // backward arrow

    inline bool Load_KW_AoT() {
	    aot_kw = RE::BGSKeyword::CreateKeyword(aot_kw_name);
		return aot_kw != nullptr;
	}

    inline bool AddKeyword(RE::BGSKeywordForm* a_form, FormID a_formid)
	{
		if (std::shared_lock lock(kw_mutex);
			kw_added.contains(a_formid)) return false;  // already added

        if (!a_form->HasKeyword(aot_kw)) {
		    if (!a_form->AddKeyword(aot_kw)) {
			    logger::error("Failed to add keyword to formid: {:x}", a_formid);
		    }
			else {
		        std::unique_lock ulock(kw_mutex);
		        kw_added.insert(a_formid);
				kw_removed.erase(a_formid);
			    return true;
			}
		}
		return false;
	}

	inline bool RemoveKeyword(RE::TESForm* a_form) {

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

	inline void ReAddKWs() {

		std::vector<std::pair<RE::BGSKeywordForm*,FormID>> to_add;
		{
            std::unique_lock lock(kw_mutex);
            for (const auto& formid : kw_removed) {
                if (const auto form = RE::TESForm::LookupByID(formid)) {
                    if (const auto kw_form = form->As<RE::BGSKeywordForm>()) {
						to_add.push_back({ kw_form, formid });
                    }
                }
            }
            kw_removed.clear();
		}

		for (const auto& [kw_form, formid] : to_add) {
			AddKeyword(kw_form,formid);
		}
	}

	inline bool HasKW(const RE::TESForm* a_form) {
		std::shared_lock lock(kw_mutex);
		return kw_added.contains(a_form->GetFormID());
    }

	struct LoreBoxInfo {
		Count count;
		float h_remaining;
	};

	// Returns a UTF-16 (wstring) body to be used as translation result
	std::wstring BuildLoreForHover();
}
