#pragma once

namespace NumericKeywords {
    void IndexExisting();
    RE::BGSKeyword* GetOrCreate(float value);
    bool Save(SKSE::SerializationInterface* serializationInterface);
}
