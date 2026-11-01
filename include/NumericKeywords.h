#pragma once

namespace NumericKeywords {
    void IndexExisting();
    RE::BGSKeyword* GetOrCreate(float value);
    bool Save(SKSE::SerializationInterface* serializationInterface);
    bool Load(SKSE::SerializationInterface* serializationInterface, std::unordered_map<RE::FormID, RE::FormID>& remapped_formids);
}
