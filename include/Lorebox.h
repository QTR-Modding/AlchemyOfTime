#pragma once
#include <shared_mutex>
#include <unordered_set>

namespace Lorebox
{
	static std::string aot_kw_name = "KW_quantAoT";
	inline RE::BGSKeyword* aot_kw = nullptr;

	inline std::shared_mutex kw_mutex;
	inline std::unordered_set<FormID> kw_added;

	bool Load_KW_AoT();

	template <typename T>
	void AddKeyword(T* a_form)
	{
		auto a_formid = a_form->GetFormID();
		if (std::shared_lock lock(kw_mutex);
			kw_added.contains(a_formid)) return;  // already added
		// Add the keyword to the form
		if (!a_form->AddKeyword(aot_kw)) {
			logger::error("Failed to add keyword to formid: {:x}", a_formid);
			return;
		}

		// Keep track of added keywords
		std::unique_lock ulock(kw_mutex);
		kw_added.insert(a_formid);
	}
}
