#pragma once
#include <shared_mutex>
#include <unordered_set>

namespace Lorebox
{
	static std::string aot_kw_name = "LoreBox_quantAoT";
	static std::string aot_kw_name_lorebox = "$LoreBox_quantAoT";
	inline RE::BGSKeyword* aot_kw = nullptr;

	inline std::shared_mutex kw_mutex;
	inline std::unordered_set<FormID> kw_added;

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

	std::wstring BuildLoreForHover();
}
