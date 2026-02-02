#include "Events.h"
#include "Manager.h"
#include "Settings.h"
#include "Threading.h"

void EventSink::HandleWO(RE::TESObjectREFR* ref) {
    if (!ref) return;
    if (!Settings::IsItem(ref)) return;
    if (!Settings::placed_objects_evolve.load() && Utils::WorldObject::IsPlacedObject(ref)) return;

    M->Update(ref);
}

void EventSink::HandleWOsInCell(const RE::TESObjectCELL* a_cell) {
    const auto cell = a_cell ? a_cell : RE::PlayerCharacter::GetSingleton()->GetParentCell();
    if (!cell) return;

    std::vector<RE::ObjectRefHandle> refs;
    cell->ForEachReference([&refs](RE::TESObjectREFR* a_obj) {
        if (!a_obj) return RE::BSContainer::ForEachResult::kContinue;
        if (a_obj->HasContainer()) return RE::BSContainer::ForEachResult::kContinue;
        refs.push_back(a_obj->GetHandle());
        return RE::BSContainer::ForEachResult::kContinue;
    });

    for (auto& a_handle : refs) {
        HandleWO(a_handle.get().get());
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
    if (!event->crosshairRef) return RE::BSEventNotifyControl::kContinue;

    if (!event->crosshairRef->HasContainer()) HandleWO(event->crosshairRef.get());
    else M->Update(event->crosshairRef.get());

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

    if (const auto a_cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(a_event->cellID)) {
        if (a_event->flags.get() == RE::BGSActorCellEvent::CellFlag::kEnter) {
            HandleWOsInCell(a_cell);
        }
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