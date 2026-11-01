#include "NumericKeywords.h"
#include <ClibUtil/string.hpp>
#include "Settings.h"

namespace {
    constexpr std::string_view numeric_keyword_prefix = "AoT_Value_";
    std::map<float, RE::FormID> numeric_keywords;
}

void NumericKeywords::IndexExisting() {
    const auto& [forms, lock] = RE::TESForm::GetAllFormsByEditorID();
    RE::BSWriteLockGuard guard{lock};
    if (!forms) return;
    for (const auto keyword : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSKeyword>()) {
        if (!keyword) continue;
        const std::string_view editor_id = keyword->formEditorID;
        if (clib_util::string::iequals(editor_id.substr(0, numeric_keyword_prefix.size()), numeric_keyword_prefix)) {
            forms->insert({keyword->formEditorID, keyword});
        }
    }
}

RE::BGSKeyword* NumericKeywords::GetOrCreate(const float value) {
    static std::mutex creation_mutex;
    std::lock_guard guard(creation_mutex);
    auto name = std::format("{}{}", numeric_keyword_prefix, value == 0.f ? 0.f : value);
    std::ranges::replace(name, '.', '_');
    clib_util::string::replace_all(name, "-", "minus_");
    clib_util::string::replace_all(name, "+", "plus_");
    if (const auto keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(name)) {
        numeric_keywords[value] = keyword->GetFormID();
        return keyword;
    }

    const RE::BSFixedString editor_id(name);
    const auto keyword = RE::BGSKeyword::CreateKeyword(editor_id);
    RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSKeyword>().push_back(keyword);
    const auto& [forms, lock] = RE::TESForm::GetAllFormsByEditorID();
    {
        RE::BSWriteLockGuard map_guard{lock};
        if (forms) forms->insert({editor_id, keyword});
    }
    numeric_keywords[value] = keyword->GetFormID();
    return keyword;
}

bool NumericKeywords::Save(SKSE::SerializationInterface* serializationInterface) {
    if (!serializationInterface->OpenRecord(Settings::kNumericKeywordsKey, Settings::kSerializationVersion)) return false;
    const auto count = numeric_keywords.size();
    if (!serializationInterface->WriteRecordData(count)) return false;
    for (const auto& [value, formid] : numeric_keywords) {
        if (!serializationInterface->WriteRecordData(value) || !serializationInterface->WriteRecordData(formid)) return false;
    }
    return true;
}

