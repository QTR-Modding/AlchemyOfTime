#pragma once
#include "Settings.h"
#include "CLibUtilsQTR/Serialization.hpp"

using SaveDataLHS = std::pair<Utils::Types::FormEditorID, RefID>;
using SaveDataRHS = std::vector<StageInstancePlain>;

class SaveLoadData : public Serialization::BaseData<SaveDataLHS, SaveDataRHS> {
public:
    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override;

    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                            std::uint32_t version) override;

    [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface) override;
};

void SaveCallback(SKSE::SerializationInterface* serializationInterface);

void LoadCallback(SKSE::SerializationInterface* serializationInterface);

void InitializeSerialization();