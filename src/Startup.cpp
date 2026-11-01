#include "Startup.h"
#include "Events.h"
#include "Hooks.h"
#include "Lorebox.h"
#include "MCP.h"
#include "Manager.h"
#include "Queue.h"
#include "Threading.h"

namespace {
    enum class StartPoint { DataLoaded, KID, Started };
    StartPoint start_point = StartPoint::DataLoaded;
    const SKSE::LoadInterface* skse_interface = nullptr;

    void Initialize() {
        start_point = StartPoint::Started;
        // 0) Check Po3's Tweaks
        if (!Utils::IsPo3Installed()) {
            logger::error("Po3 is not installed.");
            Utils::MsgBoxesNotifs::Windows::Po3ErrMsg();
            return;
        }

        // 1) Keyword
        if (!Lorebox::Load_KW_AoT()) {
            logger::error("Failed to load keyword.");
            Utils::MsgBoxesNotifs::InGame::CustomMsg("Failed to load settings. Check log for details.");
            return;
        }

        // 2) Load settings
        {
            SpeedProfiler prof("LoadSettings");
            PresetParse::LoadSettingsParallel();
        }
        if (Settings::failed_to_load) {
            logger::critical("Failed to load settings.");
            Utils::MsgBoxesNotifs::InGame::CustomMsg("Failed to load settings. Check log for details.");
            return;
        }

        // 3) Initialize Managers
        M = Manager::GetSingleton();
        if (!M) return;
        QueueManager::GetSingleton()->Start();

        // 4) Register event sinks
        const auto eventSink = EventSink::GetSingleton();
        auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
        eventSourceHolder->AddEventSink<RE::TESActivateEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESCellAttachDetachEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESObjectLoadedEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESInitScriptEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESMoveAttachDetachEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESResetEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESFurnitureEvent>(eventSink);
        eventSourceHolder->AddEventSink<RE::TESFormDeleteEvent>(eventSink);
        SKSE::GetCrosshairRefEventSource()->AddEventSink(eventSink);
        logger::info("Event sinks added.");

        // 5) Start MCP
        UI::Register();
        logger::info("MCP registered.");

        // 6) install hooks
        Hooks::Install();
        logger::info("Hooks installed.");
    }

    class KeywordDistributionSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
    public:
        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event,
                                              RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
            if (start_point == StartPoint::KID && event->eventName == "KID_KeywordDistributionDone") {
                Initialize();
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };
    KeywordDistributionSink keyword_distribution_sink;

    void OnMessage(SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kPostPostLoad) {
            if (skse_interface->GetPluginInfo("Keyword Item Distributor")) start_point = StartPoint::KID;
        } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (start_point == StartPoint::DataLoaded) Initialize();
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
            logger::info("PostLoadGame.");
            if (const auto ui = RE::UI::GetSingleton();
                ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::JournalMenu::MENU_NAME)) {
                logger::warn("Missing esps?");
                return;
            }
            if (!M || M->isUninstalled.load()) return;
            M->Update(RE::PlayerCharacter::GetSingleton());
            EventSink::HandleRefsInCell();
        }
    }
}

void Startup::Install(const SKSE::LoadInterface* skse) {
    skse_interface = skse;
    SKSE::GetModCallbackEventSource()->AddEventSink(&keyword_distribution_sink);
    if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
        SKSE::stl::report_and_fail("Failed to register message listener");
    }
}
