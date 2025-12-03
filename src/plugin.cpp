#include "Events.h"
#include "Hooks.h"
#include "Lorebox.h"
#include "MCP.h"
#include "Manager.h"
#include "Threading.h"

namespace {
    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void OnMessage(SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            // 0) Check Po3's Tweaks
            if (!IsPo3Installed()) {
                logger::error("Po3 is not installed.");
                MsgBoxesNotifs::Windows::Po3ErrMsg();
                return;
            }

            // 1) Keyword
            if (!Lorebox::Load_KW_AoT()) {
                logger::error("Failed to load keyword.");
                MsgBoxesNotifs::InGame::CustomMsg("Failed to load settings. Check log for details.");
                return;
            }

            // 2) Load settings
            {
                SpeedProfiler prof("LoadSettings");
                PresetParse::LoadSettingsParallel();
            }
            if (Settings::failed_to_load) {
                logger::critical("Failed to load settings.");
                MsgBoxesNotifs::InGame::CustomMsg("Failed to load settings. Check log for details.");
                return;
            }

            // 3) Initialize Manager
            const auto sources = std::vector<Source>{};
            M = Manager::GetSingleton(sources);
            if (!M) return;

            // 4) Register event sinks
            const auto eventSink = EventSink::GetSingleton();
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
            UI::Register();
            logger::info("MCP registered.");

            // 6) install hooks
            Hooks::Install();
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
            EventSink::GetSingleton()->HandleWOsInCell();
        }
    }
}


SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    InitializeSerialization();
    if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
        SKSE::stl::report_and_fail("Failed to register message listener");
        // ReSharper disable once CppUnreachableCode
        return false;
    }
    logger::info("Number of threads: {}", numThreads);
    return true;
}