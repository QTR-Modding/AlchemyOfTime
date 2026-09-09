#pragma once
#include "REX/REX/Singleton.h"

class EventSink final : public REX::Singleton<EventSink>,
                        public RE::BSTEventSink<RE::TESActivateEvent>,
                        public RE::BSTEventSink<SKSE::CrosshairRefEvent>,
                        public RE::BSTEventSink<RE::TESCellAttachDetachEvent>,
                        public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
                        public RE::BSTEventSink<RE::TESInitScriptEvent>,
                        public RE::BSTEventSink<RE::TESMoveAttachDetachEvent>,
                        public RE::BSTEventSink<RE::TESResetEvent>,
                        public RE::BSTEventSink<RE::TESFurnitureEvent>,
                        public RE::BSTEventSink<RE::TESFormDeleteEvent> {
    bool furniture_entered = false;
    RE::NiPointer<RE::TESObjectREFR> furniture = nullptr;

    static void HandleRef(RE::TESObjectREFR* ref, bool skip_if_queued = false);

public:
    static void HandleRefsInCell(const RE::TESObjectCELL* a_cell = nullptr);

    RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* event,
                                          RE::BSTEventSource<RE::TESActivateEvent>*) override;

    // to disable ref activation and external container-fake container placement
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,
                                          RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent* event,
                                          RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,
                                          RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESInitScriptEvent* event,
                                          RE::BSTEventSource<RE::TESInitScriptEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESMoveAttachDetachEvent* event,
                                          RE::BSTEventSource<RE::TESMoveAttachDetachEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESResetEvent* event,
                                          RE::BSTEventSource<RE::TESResetEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,
                                          RE::BSTEventSource<RE::TESFurnitureEvent>*) override;

    RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* a_event,
                                          RE::BSTEventSource<RE::TESFormDeleteEvent>*) override;
};