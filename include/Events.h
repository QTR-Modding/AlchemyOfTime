#pragma once
#include "REX/REX/Singleton.h"

class EventSink final : public REX::Singleton<EventSink>,
                        public RE::BSTEventSink<RE::TESEquipEvent>,
                        public RE::BSTEventSink<RE::TESActivateEvent>,
                        public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                        public RE::BSTEventSink<RE::TESFurnitureEvent>,
                        public RE::BSTEventSink<RE::TESSleepStopEvent>,
                        public RE::BSTEventSink<RE::TESWaitStopEvent>,
                        public RE::BSTEventSink<RE::BGSActorCellEvent>,
                        public RE::BSTEventSink<RE::TESFormDeleteEvent> {
    std::atomic<bool> listen_cellchange = true;

    bool furniture_entered = false;
    RE::NiPointer<RE::TESObjectREFR> furniture = nullptr;

    static void HandleWO(RE::TESObjectREFR* ref);

public:
    void HandleWOsInCell() const;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event,
                                          RE::BSTEventSource<RE::TESActivateEvent>*) override;

    // to disable ref activation and external container-fake container placement
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,
                                          RE::BSTEventSource<RE::TESFurnitureEvent>*) override;


    RE::BSEventNotifyControl ProcessEvent(const RE::TESSleepStopEvent*,
                                          RE::BSTEventSource<RE::TESSleepStopEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESWaitStopEvent*,
                                          RE::BSTEventSource<RE::TESWaitStopEvent>*) override;

    // https://github.com/SeaSparrowOG/RainExtinguishesFires/blob/c1aee0045aeb987b2f70e495b301c3ae8bd7b3a3/src/loadEventManager.cpp#L15
    RE::BSEventNotifyControl ProcessEvent(const RE::BGSActorCellEvent* a_event,
                                          RE::BSTEventSource<RE::BGSActorCellEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event,
                                          RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;
};