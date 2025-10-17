#include "MCP.h"
#include "Threading.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        
        // 1) Check Po3's Tweaks
        if (!IsPo3Installed()) {
            logger::error("Po3 is not installed.");
            MsgBoxesNotifs::Windows::Po3ErrMsg();
            return;
        }

		// 2) Load settings
        {
			SpeedProfiler prof("LoadSettings");
            PresetParse::LoadSettingsParallel();
        }
        if (Settings::failed_to_load) {
            MsgBoxesNotifs::InGame::CustomMsg("Failed to load settings. Check log for details.");
            M->Uninstall();
			return;
        }

		// 3) Initialize Manager
        constexpr auto sources = std::vector<Source>();
        M = Manager::GetSingleton(sources);
        if (!M) return;
        
        // 4) Register event sinks
        const auto eventSink = OurEventSink::GetSingleton();
        auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
        eventSourceHolder->AddEventSink<RE::TESEquipEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESActivateEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESFurnitureEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESSleepStopEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESWaitStopEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESFormDeleteEvent>(eventSink);
        SKSE::GetCrosshairRefEventSource()->AddEventSink(eventSink);
        RE::PlayerCharacter::GetSingleton()->AsBGSActorCellEventSource()->AddEventSink(eventSink);
        logger::info("Event sinks added.");
        
        // 5) Start MCP
        UI::Register(M);
        logger::info("MCP registered.");

		// 6) install hooks
		Hooks::Install(M);
		logger::info("Hooks installed.");
    }
    if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
		logger::info("PostLoadGame.");
		if (const auto ui = RE::UI::GetSingleton(); 
            ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ||
            ui->IsMenuOpen(RE::JournalMenu::MENU_NAME)) {
			logger::warn("Missing esps?");
			return;
		}
        if (!M || M->isUninstalled.load()) return;
		M->Update(RE::PlayerCharacter::GetSingleton());
        OurEventSink::GetSingleton()->HandleWOsInCell();
    }
};

#define DISABLE_IF_UNINSTALLED if (!M || M->isUninstalled.load()) return;
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
        auto temp = DecodeTypeCode(type);

        if (version == Settings::kSerializationVersion-1){
            logger::info("Older version of Alchemy of Time detected.");
            /*Utilities::MsgBoxesNotifs::InGame::CustomMsg("You are using an older"
                " version of Alchemy of Time (AoT). Versions older than 0.1.4 are unfortunately not supported."
                "Please roll back to a save game where AoT was not installed or AoT version is 0.1.4 or newer.");*/
            //continue;
            cosave_found = 1; // DFT is not saved in older versions
        }
        else if (version != Settings::kSerializationVersion) {
            logger::critical("Loaded data has incorrect version. Recieved ({}) - Expected ({}) for Data Key ({})",
                             version, Settings::kSerializationVersion, temp);
            continue;
        }
        switch (type) {
            case Settings::kDataKey: {
				logger::info("Manager: Loading Data.");
                logger::trace("Loading Record: {} - Version: {} - Length: {}", temp, version, length);
                if (!M->Load(serializationInterface)) logger::critical("Failed to Load Data for Manager");
                else cosave_found++;
            } break;
            case Settings::kDFDataKey: {
				logger::info("DFT: Loading Data.");
				logger::trace("Loading Record: {} - Version: {} - Length: {}", temp, version, length);
				if (!DFT->Load(serializationInterface)) logger::critical("Failed to Load Data for DFT");
				else cosave_found++;
            } break;
            default:
                logger::critical("Unrecognized Record Type: {}", temp);
                break;
        }
    }

    if (cosave_found==2) {
		DFT->ReceiveData();
        M->ReceiveData();
        logger::info("Data loaded from skse co-save.");
    } else logger::info("No cosave data found.");

	M->isLoading.store(false);

}
#undef DISABLE_IF_UNINSTALLED

void InitializeSerialization() {
    auto* serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(Settings::kDataKey);
    serialization->SetSaveCallback(SaveCallback);
    serialization->SetLoadCallback(LoadCallback);
    SKSE::log::trace("Cosave serialization initialized.");
};

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    InitializeSerialization();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	logger::info("Number of threads: {}", numThreads);
    return true;
}