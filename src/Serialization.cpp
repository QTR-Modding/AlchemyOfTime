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
        logger::trace("Formid:{}", formid);
        if (!serializationInterface->WriteRecordData(formid)) {
            logger::error("Failed to save formid");
            return false;
        }

        const std::string editorid = lhs.first.editor_id;
        logger::trace("Editorid:{}", editorid);
        Serialization::write_string(serializationInterface, editorid);

        std::uint32_t refid = lhs.second;
        logger::trace("Refid:{}", refid);
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
            logger::trace("size of rhs_: {}", sizeof(rhs_));
            if (!serializationInterface->WriteRecordData(rhs_)) {
                logger::error("Failed to save data");
                return false;
            }
        }
    }
    return true;
}

bool SaveLoadData::Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
    std::uint32_t version) {
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


    logger::trace("Loading data from serialization interface.");
    for (auto i = 0; i < recordDataSize; i++) {
                
        SaveDataRHS rhs;
                 
        std::uint32_t formid = 0;
        logger::trace("ReadRecordData:{}", serializationInterface->ReadRecordData(formid));
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
        logger::trace("ReadRecordData:{}", serializationInterface->ReadRecordData(refid));

        logger::trace("Formid:{}", formid);
        logger::trace("Refid:{}", refid);
        logger::trace("Editorid:{}", editorid);

        SaveDataLHS lhs({formid,editorid},refid);
        logger::trace("Reading value...");

        std::size_t rhsSize = 0;
        logger::trace("ReadRecordData: {}", serializationInterface->ReadRecordData(rhsSize));
        logger::trace("rhsSize: {}", rhsSize);

        for (auto j = 0; j < rhsSize; j++) {
            StageInstancePlain rhs_;
            logger::trace("ReadRecordData: {}", serializationInterface->ReadRecordData(rhs_));
            //print the content of rhs_ which is StageInstancePlain
            logger::trace(
                "rhs_ content: start_time: {}, no: {},"
                "count: {}, is_fake: {}, is_decayed: {}, _elapsed: {}, _delay_start: {}, _delay_mag: {}, "
                "_delay_formid: {}",
                rhs_.start_time, rhs_.no, rhs_.count, rhs_.is_fake, rhs_.is_decayed, rhs_._elapsed,
                rhs_._delay_start, rhs_._delay_mag, rhs_._delay_formid);
            rhs.push_back(rhs_);
        }

        m_Data[lhs] = rhs;
        logger::trace("Loaded data for formid {}, editorid {}, and refid {}", formid, editorid,refid);
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
        logger::trace("Formid:{}", formid);
        if (!serializationInterface->WriteRecordData(formid)) {
            logger::error("Failed to save formid");
            return false;
        }

        const std::string editorid = lhs.second;
        logger::trace("Editorid:{}", editorid);
        Serialization::write_string(serializationInterface, editorid);

        // save the number of rhs records
        const auto numRhsRecords = rhs.size();
        if (!serializationInterface->WriteRecordData(numRhsRecords)) {
            logger::error("Failed to save the size {} of rhs records", numRhsRecords);
            return false;
        }

        for (const auto& rhs_ : rhs) {
            logger::trace("size of rhs_: {}", sizeof(rhs_));
            if (!serializationInterface->WriteRecordData(rhs_)) {
                logger::error("Failed to save data");
                return false;
            }
        }
    }
    return true;
}

bool DFSaveLoadData::Save(SKSE::SerializationInterface* serializationInterface, std::uint32_t type,
    std::uint32_t version) {
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

    logger::trace("Loading data from serialization interface.");
    for (auto i = 0; i < recordDataSize; i++) {
        DFSaveDataRHS rhs;

        std::uint32_t formid = 0;
        logger::trace("ReadRecordData:{}", serializationInterface->ReadRecordData(formid));
        if (!serializationInterface->ResolveFormID(formid, formid)) {
            logger::error("Failed to resolve form ID, 0x{:X}.", formid);
            continue;
        }

        std::string editorid;
        if (!Serialization::read_string(serializationInterface, editorid)) {
            logger::error("Failed to read editorid");
            return false;
        }

        logger::trace("Formid:{}", formid);
        logger::trace("Editorid:{}", editorid);

        DFSaveDataLHS lhs({formid, editorid});
        logger::trace("Reading value...");

        std::size_t rhsSize = 0;
        logger::trace("ReadRecordData: {}", serializationInterface->ReadRecordData(rhsSize));
        logger::trace("rhsSize: {}", rhsSize);

        for (auto j = 0; j < rhsSize; j++) {
            DFSaveData rhs_;
            logger::trace("ReadRecordData: {}", serializationInterface->ReadRecordData(rhs_));
            logger::trace(
                "rhs_ content: dyn_formid: {}, customid_bool: {},"
                "customid: {}, acteff_elapsed: {}",
                rhs_.dyn_formid, rhs_.custom_id.first, rhs_.custom_id.second, rhs_.acteff_elapsed);
            rhs.push_back(rhs_);
        }

        m_Data[lhs] = rhs;
        logger::trace("Loaded data for formid {}, editorid {}", formid, editorid);
    }

    return true;
}