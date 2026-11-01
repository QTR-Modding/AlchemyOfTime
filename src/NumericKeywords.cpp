#include "NumericKeywords.h"
#include "Utils.h"
#include <ClibUtil/string.hpp>

namespace {
    struct KeywordRegistry {
        std::mutex creation_mutex;
        std::map<float, RE::BGSKeyword*> keywords;
        std::unordered_map<FormID, float> saved_values;
    };
    KeywordRegistry registry;

    std::string GetName(const float value) {
        auto name = Utils::FormatDecimal(value);
        std::ranges::replace(name, '.', '_');
        if (name.starts_with('-')) name.replace(0, 1, "minus_");
        return "AoT_Value_" + name;
    }
}

RE::BGSKeyword* NumericKeywords::Find(const float value) {
    const auto found = registry.keywords.find(value);
    return found == registry.keywords.end() ? nullptr : found->second;
}

RE::BGSKeyword* NumericKeywords::Create(const float value) {
    std::lock_guard registry_lock(registry.creation_mutex);
    if (const auto keyword = Find(value)) return keyword;
    const auto name = GetName(value);
    auto& keywords = RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSKeyword>();
    auto keyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(name);
    if (!keyword) {
        for (const auto candidate : keywords) {
            if (candidate && clib_util::string::iequals(candidate->formEditorID.c_str(), name)) {
                keyword = candidate;
                break;
            }
        }
    }
    if (!keyword) {
        keyword = RE::BGSKeyword::CreateKeyword(name);
        keywords.push_back(keyword);
    }
    const RE::BSFixedString editor_id(name);
    const auto& [forms, lock] = RE::TESForm::GetAllFormsByEditorID();
    {
        RE::BSWriteLockGuard guard{lock};
        if (forms) forms->insert({editor_id, keyword});
    }
    registry.keywords.emplace(value, keyword);
    return keyword;
}

bool NumericKeywords::Save(SKSE::SerializationInterface* serialization) {
    if (!serialization->OpenRecord(record_type, record_version)) return false;
    const auto count = static_cast<std::uint32_t>(registry.keywords.size());
    if (!serialization->WriteRecordData(count)) return false;
    for (const auto& [value, keyword] : registry.keywords) {
        const auto formid = keyword->GetFormID();
        if (!serialization->WriteRecordData(formid) || !serialization->WriteRecordData(value)) return false;
    }
    return true;
}

void NumericKeywords::BeginLoad() {
    registry.saved_values.clear();
}

bool NumericKeywords::Load(SKSE::SerializationInterface* serialization, const std::uint32_t length) {
    std::uint32_t count;
    if (serialization->ReadRecordData(count) != sizeof(count)) return false;
    constexpr auto entry_size = sizeof(FormID) + sizeof(float);
    if (length < sizeof(count) || count > (length - sizeof(count)) / entry_size) return false;
    for (std::uint32_t i = 0; i < count; ++i) {
        FormID formid;
        float value;
        if (serialization->ReadRecordData(formid) != sizeof(formid) ||
            serialization->ReadRecordData(value) != sizeof(value) || !std::isfinite(value)) return false;
        registry.saved_values.emplace(formid, value);
    }
    return true;
}

std::optional<FormID> NumericKeywords::RemapSaved(const FormID formid) {
    const auto value = registry.saved_values.find(formid);
    if (value == registry.saved_values.end()) return std::nullopt;
    const auto keyword = Find(value->second);
    return keyword ? keyword->GetFormID() : 0;
}
