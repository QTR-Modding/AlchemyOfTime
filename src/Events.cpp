#include "Events.h"
#include "Manager.h"
#include "Settings.h"
#include "Threading.h"

void EventSink::HandleWO(RE::TESObjectREFR* ref) {
    if (!ref) return;
    //if (ref->extraList.GetOwner() && !ref->extraList.GetOwner()->IsPlayer()) return;
    if (!Settings::IsItem(ref)) return;

    if (!Settings::placed_objects_evolve.load() && WorldObject::IsPlacedObject(ref)) return;

    M->Update(ref);
}

void EventSink::HandleWOsInCell() const {
    logger::trace("HandleWOsInCell: Calling Update.");
    const auto* player = RE::PlayerCharacter::GetSingleton();
    //M->Update(player);
    const auto player_cell = player->GetParentCell();
    if (!player_cell) return;
    player_cell->ForEachReference([this](RE::TESObjectREFR* arg) {
        if (!arg) return RE::BSContainer::ForEachResult::kContinue;
        if (arg->HasContainer()) return RE::BSContainer::ForEachResult::kContinue;
        HandleWO(arg);
        return RE::BSContainer::ForEachResult::kContinue;
    });
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESEquipEvent* event,
                                                 RE::BSTEventSource<RE::TESEquipEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!M->listen_equip.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;
    if (!event->actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;
    if (!Settings::IsItem(event->baseObject)) return RE::BSEventNotifyControl::kContinue;
    if (!event->equipped) {
        logger::trace("Item unequipped: {}", event->baseObject);
        return RE::BSEventNotifyControl::kContinue;
    }
    if (const auto temp_form = RE::TESForm::LookupByID(event->baseObject);
        temp_form && temp_form->Is(RE::FormType::AlchemyItem)) {
        logger::trace("Item equipped: Alchemy item.");
        return RE::BSEventNotifyControl::kContinue;
    }

    logger::trace("Item equipped: Calling Update.");
    M->Update(RE::PlayerCharacter::GetSingleton());

    return RE::BSEventNotifyControl::kContinue;
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

    if (!M->RefIsRegistered(event->objectActivated->GetFormID())) return RE::BSEventNotifyControl::kContinue;
    if (event->objectActivated->HasContainer()) return RE::BSEventNotifyControl::kContinue;

    M->SwapWithStage(event->objectActivated.get());

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                                 RE::BSTEventSource<SKSE::CrosshairRefEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!event) return RE::BSEventNotifyControl::kContinue;
    if (!event->crosshairRef) return RE::BSEventNotifyControl::kContinue;

    if (!event->crosshairRef->HasContainer()) HandleWO(event->crosshairRef.get());
    else if (M->RefIsRegistered(event->crosshairRef->GetFormID())) M->Update(event->crosshairRef.get());

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
    auto bench_type = static_cast<std::uint8_t>(bench->workBenchData.benchType.get());

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

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESSleepStopEvent*,
                                                 RE::BSTEventSource<RE::TESSleepStopEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    HandleWOsInCell();
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESWaitStopEvent*,
                                                 RE::BSTEventSource<RE::TESWaitStopEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    HandleWOsInCell();
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::BGSActorCellEvent* a_event,
                                                 RE::BSTEventSource<RE::BGSActorCellEvent>*) {
    if (M->isLoading.load()) return RE::BSEventNotifyControl::kContinue;
    if (!listen_cellchange.load()) return RE::BSEventNotifyControl::kContinue;
    if (!a_event) return RE::BSEventNotifyControl::kContinue;
    const auto eventActorHandle = a_event->actor;
    const auto eventActorPtr = eventActorHandle ? eventActorHandle.get() : nullptr;
    const auto eventActor = eventActorPtr ? eventActorPtr.get() : nullptr;
    if (!eventActor) return RE::BSEventNotifyControl::kContinue;

    if (eventActor != RE::PlayerCharacter::GetSingleton()) return RE::BSEventNotifyControl::kContinue;

    const auto cellID = a_event->cellID;
    auto* cellForm = cellID ? RE::TESForm::LookupByID(cellID) : nullptr;
    const auto* cell = cellForm ? cellForm->As<RE::TESObjectCELL>() : nullptr;
    if (!cell) return RE::BSEventNotifyControl::kContinue;

    if (a_event->flags.any(RE::BGSActorCellEvent::CellFlag::kEnter)) {
        listen_cellchange.store(false);
        M->ClearWOUpdateQueue();
        HandleWOsInCell();
        listen_cellchange.store(true);
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl EventSink::ProcessEvent(const RE::TESFormDeleteEvent* a_event,
                                                 RE::BSTEventSource<RE::TESFormDeleteEvent>*) {
    if (!a_event) return RE::BSEventNotifyControl::kContinue;
    if (!a_event->formID) return RE::BSEventNotifyControl::kContinue;
    if (M->HandleFormDelete(a_event->formID)) {
        logger::info("Form deleted: {:x}", a_event->formID);
    }
    return RE::BSEventNotifyControl::kContinue;
}