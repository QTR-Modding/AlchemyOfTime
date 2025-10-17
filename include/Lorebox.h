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

	// UI toggle: show a title before rows in lorebox
	inline std::atomic<bool> show_title{ true };
	// UI toggle: show progress percentage per instance (default true)
	inline std::atomic<bool> show_percentage{ true };
	// UI toggle: show delayer (time modulator) name
	inline std::atomic<bool> show_modulator_name{ true };
	// UI toggle: show transformer name
	inline std::atomic<bool> show_transformer_name{ true };
	// UI toggle: colorize each row based on modulation/transform state
	inline std::atomic<bool> colorize_rows{ true };
	// UI toggle: append multiplier to modulator tag (e.g., ": x0.5")
	inline std::atomic<bool> show_multiplier{ true };

	// Configurable colors (0xRRGGBB)
	inline std::atomic<uint32_t> color_title{ 0x9654FF };      // title accent
	inline std::atomic<uint32_t> color_neutral{ 0xE8EAED };    // default light text
	inline std::atomic<uint32_t> color_slow{ 0xFF8A80 };       // light red for x>1 (slower)
	inline std::atomic<uint32_t> color_fast{ 0x80D8FF };       // light blue for x<1 (faster)
	inline std::atomic<uint32_t> color_transform{ 0xFFCC80 };  // light orange for transform
	inline std::atomic<uint32_t> color_separator{ 0x9AA0A6 };  // bullet/separator color

    inline bool Load_KW_AoT() {
	    aot_kw = RE::BGSKeyword::CreateKeyword(aot_kw_name);
		return aot_kw != nullptr;
	}

    inline void AddKeyword(RE::BGSKeywordForm* a_form, FormID a_formid)
	{
		if (std::shared_lock lock(kw_mutex);
			kw_added.contains(a_formid)) return;  // already added

        if (!a_form->HasKeyword(aot_kw)) {
		    if (!a_form->AddKeyword(aot_kw)) {
			    logger::error("Failed to add keyword to formid: {:x}", a_formid);
			    return;
		    }
		    else {
			    logger::info("Added keyword to formid: {:x}", a_formid);
		    }
		}

		std::unique_lock ulock(kw_mutex);
		kw_added.insert(a_formid);
	}

	struct LoreBoxInfo {
		Count count;
		float h_remaining;
	};

	// Returns a UTF-16 (wstring) body to be used as translation result
	std::wstring BuildLoreForHover();
}
