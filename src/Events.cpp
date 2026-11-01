#include "Events.h"
#include "Manager.h"
#include "Settings.h"
#include "Threading.h"
#include "Hooks.h"
#include "Lorebox.h"
#include "MCP.h"
#include "Queue.h"

bool EventSink::Register(const SKSE::LoadInterface* skse) {
    startup.skse = skse;
    SKSE::GetModCallbackEventSource()->AddEventSink(this);
    return SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
}

void EventSink::Initialize() {
    startup.phase = Startup::Phase::Initialized;
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
    auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
    eventSourceHolder->AddEventSink<RE::TESActivateEvent>(this);
    eventSourceHolder->AddEventSink<RE::TESCellAttachDetachEvent>(this);
    eventSourceHolder->AddEventSink<RE::TESObjectLoadedEvent>(this);
    eventSourceHolder->AddEventSink<RE::TESInitScriptEvent>(this);
    eventSourceHolder->AddEventSink<RE::TESMoveAttachDetachEvent>(this);
    eventSourceHolder->AddEventSink<RE::TESResetEvent>(this);
    eventSourceHolder->AddEventSink<RE::TESFurnitureEvent>(this);
    eventSourceHolder->AddEventSink<RE::TESFormDeleteEvent>(this);
    SKSE::GetCrosshairRefEventSource()->AddEventSink(this);
    logger::info("Event sinks added.");

    // 5) Start MCP
    UI::Register();
    logger::info("MCP registered.");

    // 6) install hooks
    Hooks::Install();
    logger::info("Hooks installed.");
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void EventSink::OnMessage(SKSE::MessagingInterface::Message* message) {
    const auto sink = GetSingleton();
    if (message->type == SKSE::MessagingInterface::kPostPostLoad) {
        if (sink->startup.skse->GetPluginInfo("Keyword Item Distributor")) {
            sink->startup.phase = Startup::Phase::KeywordDistribution;
        }
    }
    if (message->type == SKSE::MessagingInterface::kDataLoaded && sink->startup.phase == Startup::Phase::DataLoaded) {
        sink->Initialize();
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
        EventSink::HandleRefsInCell();
    }
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const SKSE::ModCallbackEvent* event,
                                                 RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
    if (startup.phase == Startup::Phase::KeywordDistribution && event->eventName == "KID_KeywordDistributionDone") {
        Initialize();
    }
    return RE::BSEventNotifyControl::kContinue;
}

void EventSink::HandleRef(RE::TESObjectREFR* ref, const bool skip_if_queued) {
    if (!ref) return;
    if (!ref->HasContainer()) {
        if (!Settings::IsItem(ref)) return;
        if (!Settings::placed_objects_evolve.load() && Utils::WorldObject::IsPlacedObject(ref)) return;
    }

    M->RequestRefUpdate(ref, skip_if_queued);
}

void EventSink::HandleRefsInCell(const RE::TESObjectCELL* a_cell) {
    const auto cell = a_cell ? a_cell : RE::PlayerCharacter::GetSingleton()->GetParentCell();
    if (!cell) return;

    std::vector<RE::ObjectRefHandle> refs;
    cell->ForEachReference([&refs](RE::TESObjectREFR* a_obj) {
        if (!a_obj) return RE::BSContainer::ForEachResult::kContinue;
        refs.push_back(a_obj->GetHandle());
        return RE::BSContainer::ForEachResult::kContinue;
    });

    for (auto& a_handle : refs) {
        HandleRef(a_handle.get().get());
    }
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESActivateEvent* event,
                                                 RE::BSTEventSource<RE::TESActivateEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;
    if (!event->objectActivated) return RE::BSEventNotifyControl::kContinue;
    if (event->objectActivated == RE::PlayerCharacter::GetSingleton()->GetGrabbedRef())
        return
            RE::BSEventNotifyControl::kContinue;
    if (event->objectActivated->IsActivationBlocked()) return RE::BSEventNotifyControl::kContinue;

    if (event->objectActivated->HasContainer()) return RE::BSEventNotifyControl::kContinue;

    M->SwapWithStage(event->objectActivated.get());

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                                 RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;

    HandleRef(event->crosshairRef.get());

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESCellAttachDetachEvent* event,
                                                 RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) {
    logger::trace("TESCellAttachDetachEvent");
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event || !event->attached || !event->reference) return RE::BSEventNotifyControl::kContinue;

    logger::trace("TESCellAttachDetachEvent: Attached ref {:x}", event->reference->GetFormID());

    HandleRef(event->reference.get(), true);

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESObjectLoadedEvent* event,
                                                 RE::BSTEventSource<RE::TESObjectLoadedEvent>*) {
    if (!event) return RE::BSEventNotifyControl::kContinue;
    logger::trace("TESObjectLoadedEvent: ref {:x}, loaded {}", event->formID, event->loaded);
    if (M->isLoading.load() || !event->loaded) return RE::BSEventNotifyControl::kContinue;
    if (M->IsRefQueued(event->formID)) return RE::BSEventNotifyControl::kContinue;

    HandleRef(RE::TESForm::LookupByID<RE::TESObjectREFR>(event->formID), true);

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESInitScriptEvent* event,
                                                 RE::BSTEventSource<RE::TESInitScriptEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event || !event->objectInitialized) return RE::BSEventNotifyControl::kContinue;
    logger::trace("TESInitScriptEvent: Initialized ref {:x}", event->objectInitialized->GetFormID());

    HandleRef(event->objectInitialized.get(), true);

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESMoveAttachDetachEvent* event,
                                                 RE::BSTEventSource<RE::TESMoveAttachDetachEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event || !event->isCellAttached) return RE::BSEventNotifyControl::kContinue;
    logger::trace("TESMoveAttachDetachEvent: Moved ref {:x}", event->movedRef->GetFormID());

    HandleRef(event->movedRef.get(), true);

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESResetEvent* event,
                                                 RE::BSTEventSource<RE::TESResetEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;
    logger::trace("TESResetEvent: Reset ref {:x}", event->object->GetFormID());

    // A reset can replace inventory contents without calling our add/remove hooks.
    HandleRef(event->object.get());

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESFurnitureEvent* event,
                                                 RE::BSTEventSource<RE::TESFurnitureEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;
    if (!event->actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
    if (furniture_entered && event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter)
        return RE::BSEventNotifyControl::kContinue;
    if (!furniture_entered && event->type == RE::TESFurnitureEvent::FurnitureEventType::kExit)
        return RE::BSEventNotifyControl::kContinue;
    if (event->targetFurniture->GetBaseObject()->formType.underlying() != 40)
        return RE::BSEventNotifyControl::kContinue;

    const auto bench = event->targetFurniture->GetBaseObject()->As<RE::TESFurniture>();
    if (!bench) return RE::BSEventNotifyControl::kContinue;
    const auto bench_type = static_cast<std::uint8_t>(bench->workBenchData.benchType.get());

    //if (bench_type != 2 && bench_type != 3 && bench_type != 7) return RE::BSEventNotifyControl::kContinue;

    if (!Settings::qform_bench_map.contains(bench_type)) return RE::BSEventNotifyControl::kContinue;

    if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter) {
        furniture_entered = true;
        furniture = event->targetFurniture;
        M->HandleCraftingEnter(bench_type);
    } else if (event->type == RE::TESFurnitureEvent::FurnitureEventType::kExit) {
        if (event->targetFurniture == furniture) {
            M->HandleCraftingExit();
            furniture_entered = false;
            furniture = nullptr;
        }
    } else logger::info("Furniture event: Unknown");

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESFormDeleteEvent* a_event,
                                                 RE::BSTEventSource<RE::TESFormDeleteEvent>*) {
    if (!a_event) return RE::BSEventNotifyControl::kContinue;
    if (!a_event->formID) return RE::BSEventNotifyControl::kContinue;
    if (M->HandleFormDelete(a_event->formID)) {
        logger::trace("Form deleted: {:x}", a_event->formID);
    }
    return RE::BSEventNotifyControl::kContinue;
}