#pragma once

namespace NumericKeywords {
    constexpr std::uint32_t record_type = 'KAOT';
    constexpr std::uint32_t record_version = 1;

    std::string GetName(float value);
    RE::BGSKeyword* Create(float value);
    RE::BGSKeyword* Find(float value);
    bool Save(SKSE::SerializationInterface* serialization);
    bool Load(SKSE::SerializationInterface* serialization, std::uint32_t length);
    void BeginLoad();
    std::optional<FormID> RemapSaved(FormID formid);
}
