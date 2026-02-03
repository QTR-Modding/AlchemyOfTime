#include "Serialization.h"
#include "DynamicFormTracker.h"
#include "Manager.h"

bool SaveLoadData::Save(SKSE::SerializationInterface* serializationInterface) {
    assert(serializationInterface);
    Locker locker(m_Lock);

    const auto numRecords = m_Data.size();
    if (!serializationInterface->WriteRecordData(numRecords)) {
        logger::error("Failed to save {} data records", numRecords);
        return false;
    }

    for (const auto& [lhs, rhs] : m_Data) {
        // we serialize formid, editorid, and refid separately
        std::uint32_t formid = lhs.first.form_id;
        if (!serializationInterface->WriteRecordData(formid)) {
            logger::error("Failed to save FormID");
            return false;
        }

        const std::string editorid = lhs.first.editor_id;
        Serialization::write_string(serializationInterface, editorid);

        std::uint32_t refid = lhs.second;
        if (!serializationInterface->WriteRecordData(refid)) {
            logger::error("Failed to save RefID");
            return false;
        }

        // save the number of rhs records
        const auto numRhsRecords = rhs.size();
        if (!serializationInterface->WriteRecordData(numRhsRecords)) {
            logger::error("Failed to save the size {} of rhs records", numRhsRecords);
            return false;
        }

        for (const auto& rhs_ : rhs) {
            if (!serializationInterface->WriteRecordData(rhs_)) {
                logger::error("Failed to save data");
                return false;
            }
        }
    }
    return true;
}

bool SaveLoadData::Save(SKSE::SerializationInterface* serializationInterface, const std::uint32_t type,
                        const std::uint32_t version) {
    if (!serializationInterface->OpenRecord(type, version)) {
        logger::error("Failed to open record for Data Serialization!");
        return false;
    }

    return Save(serializationInterface);
}

bool SaveLoadData::Load(SKSE::SerializationInterface* serializationInterface) {
    assert(serializationInterface);

    std::size_t recordDataSize;
    serializationInterface->ReadRecordData(recordDataSize);
    logger::info("Loading data from serialization interface with size: {}", recordDataSize);

    Locker locker(m_Lock);
    m_Data.clear();

    for (auto i = 0; std::cmp_less(i, recordDataSize); i++) {
        SaveDataRHS rhs;

        std::uint32_t formid = 0;
        serializationInterface->ReadRecordData(formid);
        if (!serializationInterface->ResolveFormID(formid, formid)) {
            logger::error("Failed to resolve form ID, 0x{:X}.", formid);
            continue;
        }

        std::string editorid;
        if (!Serialization::read_string(serializationInterface, editorid)) {
            logger::error("Failed to read EditorID");
            return false;
        }

        std::uint32_t refid = 0;
        serializationInterface->ReadRecordData(refid);

        SaveDataLHS lhs({formid, editorid}, refid);

        std::size_t rhsSize = 0;
        serializationInterface->ReadRecordData(rhsSize);

        for (auto j = 0; std::cmp_less(j, rhsSize); j++) {
            StageInstancePlain rhs_;
            serializationInterface->ReadRecordData(rhs_);
            if (rhs_._delay_formid > 0 && !serializationInterface->ResolveFormID(rhs_._delay_formid, rhs_._delay_formid)) {
                logger::error("Failed to resolve form ID, 0x{:X}.", formid);
                continue;
            }
            rhs.push_back(rhs_);
        }

        m_Data[lhs] = rhs;
    }

    return true;
}

bool DFSaveLoadData::Save(SKSE::SerializationInterface* serializationInterface) {
    assert(serializationInterface);
    Locker locker(m_Lock);

    const auto numRecords = m_Data.size();
    if (!serializationInterface->WriteRecordData(numRecords)) {
        logger::error("Failed to save {} data records", numRecords);
        return false;
    }

    for (const auto& [lhs, rhs] : m_Data) {
        // we serialize formid, editorid, and refid separately
        std::uint32_t formid = lhs.first;
        if (!serializationInterface->WriteRecordData(formid)) {
            logger::error("Failed to save FormID {:x}", formid);
            return false;
        }

        const std::string editorid = lhs.second;
        Serialization::write_string(serializationInterface, editorid);

        // save the number of rhs records
        const auto numRhsRecords = rhs.size();
        if (!serializationInterface->WriteRecordData(numRhsRecords)) {
            logger::error("Failed to save the size {} of rhs records", numRhsRecords);
            return false;
        }

        for (const auto& rhs_ : rhs) {
            if (!serializationInterface->WriteRecordData(rhs_)) {
                logger::error("Failed to save data");
                return false;
            }
        }
    }
    return true;
}

bool DFSaveLoadData::Save(SKSE::SerializationInterface* serializationInterface, const std::uint32_t type,
                          const std::uint32_t version) {
    if (!serializationInterface->OpenRecord(type, version)) {
        logger::error("Failed to open record for Data Serialization!");
        return false;
    }

    return Save(serializationInterface);
}

bool DFSaveLoadData::Load(SKSE::SerializationInterface* serializationInterface) {
    assert(serializationInterface);

    std::size_t recordDataSize;
    serializationInterface->ReadRecordData(recordDataSize);
    logger::info("Loading data from serialization interface with size: {}", recordDataSize);

    Locker locker(m_Lock);
    m_Data.clear();

    for (auto i = 0; std::cmp_less(i, recordDataSize); i++) {
        DFSaveDataRHS rhs;

        std::uint32_t formid = 0;
        serializationInterface->ReadRecordData(formid);
        if (!serializationInterface->ResolveFormID(formid, formid)) {
            logger::error("Failed to resolve form ID, 0x{:X}.", formid);
            continue;
        }

        std::string editorid;
        if (!Serialization::read_string(serializationInterface, editorid)) {
            logger::error("Failed to read EditorID");
            return false;
        }

        DFSaveDataLHS lhs({formid, editorid});

        std::size_t rhsSize = 0;
        serializationInterface->ReadRecordData(rhsSize);

        for (auto j = 0; std::cmp_less(j, rhsSize); j++) {
            DFSaveData rhs_;
            serializationInterface->ReadRecordData(rhs_);
            rhs.push_back(rhs_);
        }

        m_Data[lhs] = rhs;
    }

    return true;
}

#define DISABLE_IF_UNINSTALLED \
    if (!M || M->isUninstalled.load()) return;

void SaveCallback(SKSE::SerializationInterface* serializationInterface) {
    DISABLE_IF_UNINSTALLED
    M->SendData();
    if (!M->Save(serializationInterface, Settings::kDataKey, Settings::kSerializationVersion)) {
        logger::critical("Failed to save Data");
    }
    auto* DFT = DynamicFormTracker::GetSingleton();
    DFT->SendData();
    if (!DFT->Save(serializationInterface, Settings::kDFDataKey, Settings::kSerializationVersion)) {
        logger::critical("Failed to save Data");
    }
}

void LoadCallback(SKSE::SerializationInterface* serializationInterface) {
    DISABLE_IF_UNINSTALLED

    M->isLoading.store(true);
    logger::info("Loading Data from skse co-save.");

    M->Reset();
    auto* DFT = DynamicFormTracker::GetSingleton();
    DFT->Reset();

    std::uint32_t type;
    std::uint32_t version;
    std::uint32_t length;

    unsigned int cosave_found = 0;
    while (serializationInterface->GetNextRecordInfo(type, version, length)) {
        auto temp = Utils::DecodeTypeCode(type);

        if (version == Settings::kSerializationVersion - 1) {
            logger::info("Older version of Alchemy of Time detected.");
            /*Utilities::MsgBoxesNotifs::InGame::CustomMsg("You are using an older"
                " version of Alchemy of Time (AoT). Versions older than 0.1.4 are unfortunately not supported."
                "Please roll back to a save game where AoT was not installed or AoT version is 0.1.4 or newer.");*/
            // continue;
            cosave_found = 1; // DFT is not saved in older versions
        } else if (version != Settings::kSerializationVersion) {
            logger::critical("Loaded data has incorrect version. Recieved ({}) - Expected ({}) for Data Key ({})",
                             version, Settings::kSerializationVersion, temp);
            continue;
        }
        switch (type) {
            case Settings::kDataKey: {
                logger::info("Manager: Loading Data.");
                logger::trace("Loading Record: {} - Version: {} - Length: {}", temp, version, length);
                if (!M->Load(serializationInterface))
                    logger::critical("Failed to Load Data for Manager");
                else
                    cosave_found++;
            }
            break;
            case Settings::kDFDataKey: {
                logger::info("DFT: Loading Data.");
                logger::trace("Loading Record: {} - Version: {} - Length: {}", temp, version, length);
                if (!DFT->Load(serializationInterface))
                    logger::critical("Failed to Load Data for DFT");
                else
                    cosave_found++;
            }
            break;
            default:
                logger::critical("Unrecognized Record Type: {}", temp);
                break;
        }
    }

    if (cosave_found == 2) {
        DFT->ReceiveData();
        M->ReceiveData();
        logger::info("Data loaded from skse co-save.");
    } else
        logger::info("No cosave data found.");

    M->isLoading.store(false);
}
#undef DISABLE_IF_UNINSTALLED

void InitializeSerialization() {
    auto* serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(Settings::kDataKey);
    serialization->SetSaveCallback(SaveCallback);
    serialization->SetLoadCallback(LoadCallback);
    SKSE::log::trace("Cosave serialization initialized.");
}