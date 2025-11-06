#include "Serialization.h"

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
            logger::error("Failed to save formid");
            return false;
        }

        const std::string editorid = lhs.first.editor_id;
        Serialization::write_string(serializationInterface, editorid);

        std::uint32_t refid = lhs.second;
        if (!serializationInterface->WriteRecordData(refid)) {
            logger::error("Failed to save refid");
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


    for (auto i = 0; i < recordDataSize; i++) {
                
        SaveDataRHS rhs;
                 
        std::uint32_t formid = 0;
        serializationInterface->ReadRecordData(formid);
        if (!serializationInterface->ResolveFormID(formid, formid)) {
            logger::error("Failed to resolve form ID, 0x{:X}.", formid);
            continue;
        }
                 
        std::string editorid;
        if (!Serialization::read_string(serializationInterface, editorid)) {
            logger::error("Failed to read editorid");
            return false;
        }

        std::uint32_t refid = 0;
        serializationInterface->ReadRecordData(refid);

        SaveDataLHS lhs({formid,editorid},refid);

        std::size_t rhsSize = 0;
        serializationInterface->ReadRecordData(rhsSize);

        for (auto j = 0; j < rhsSize; j++) {
            StageInstancePlain rhs_;
            serializationInterface->ReadRecordData(rhs_);
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
            logger::error("Failed to save formid");
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

    for (auto i = 0; i < recordDataSize; i++) {
        DFSaveDataRHS rhs;

        std::uint32_t formid = 0;
        serializationInterface->ReadRecordData(formid);
        if (!serializationInterface->ResolveFormID(formid, formid)) {
            logger::error("Failed to resolve form ID, 0x{:X}.", formid);
            continue;
        }

        std::string editorid;
        if (!Serialization::read_string(serializationInterface, editorid)) {
            logger::error("Failed to read editorid");
            return false;
        }


        DFSaveDataLHS lhs({formid, editorid});

        std::size_t rhsSize = 0;
        serializationInterface->ReadRecordData(rhsSize);

        for (auto j = 0; j < rhsSize; j++) {
            DFSaveData rhs_;
            serializationInterface->ReadRecordData(rhs_);
            rhs.push_back(rhs_);
        }

        m_Data[lhs] = rhs;
    }

    return true;
}