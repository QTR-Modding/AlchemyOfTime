#pragma once
#include "Settings.h"
#include "CLibUtilsQTR/Serialization.hpp"

using SaveDataLHS = std::pair<Types::FormEditorID, RefID>;
using SaveDataRHS = std::vector<StageInstancePlain>;

struct DFSaveData {
    FormID dyn_formid = 0;
    std::pair<bool, uint32_t> custom_id = {false, 0};
    float acteff_elapsed = -1.f;
};

using DFSaveDataLHS = std::pair<FormID, std::string>;
using DFSaveDataRHS = std::vector<DFSaveData>;


class SaveLoadData : public Serialization::BaseData<SaveDataLHS, SaveDataRHS> {
public:
    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override;

    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                            std::uint32_t version) override;

    [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface) override;
};

class DFSaveLoadData : public Serialization::BaseData<DFSaveDataLHS, DFSaveDataRHS> {
public:
    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface) override;;

    [[nodiscard]] bool Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
                            std::uint32_t version) override;;

    [[nodiscard]] bool Load(SKSE::SerializationInterface* serializationInterface) override;
};


void SaveCallback(SKSE::SerializationInterface* serializationInterface);

void LoadCallback(SKSE::SerializationInterface* serializationInterface);

void InitializeSerialization();