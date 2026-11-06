#pragma once

#include <cstdint>
#include <RE/B/BSCoreTypes.h>
#include <Windows.h>

namespace AoTAPI {
    constexpr std::uint32_t interface_version = 1;

    template <class T>
    struct Array {
        const T* data;
        std::uint32_t size;
    };

    enum class TimingState : std::uint32_t { Running, Paused, NoTransition };

    struct InstanceInfo {
        RE::FormID source;
        RE::FormID location;
        RE::FormID item;
        std::int32_t count;
        std::uint32_t stage;
        RE::FormID trigger;
        float sampledAt;
        float startedAt;
        float elapsed;
        float duration;
        float speed;
        float nextUpdate;
        float remainingHours;
        float progressPercentage; // -1 when unavailable; otherwise 0..100.
        TimingState timing;
        bool transforming;
        bool dynamic;
        bool decayed;
    };

    struct StageInfo {
        std::uint32_t number;
        RE::FormID item; // Zero for a dynamic stage that has not been created.
        const char* name;
        float duration;
        bool dynamic;
        bool craftingAllowed;
    };

    struct Filters {
        Array<RE::FormID> containers;
        Array<std::uint32_t> stages;
    };

    struct TransformationInfo {
        RE::FormID trigger;
        RE::FormID result;
        float duration;
        Filters filters;
    };

    struct TimeModifierInfo {
        RE::FormID trigger;
        float magnitude;
        Filters filters;
    };

    struct SourceInfo {
        RE::FormID item;
        const char* editorID;
        const char* module;
        RE::FormID finalItem;
        Array<StageInfo> stages;
        Array<RE::FormID> containers;
        Array<TransformationInfo> transformations;
        Array<TimeModifierInfo> timeModifiers;
    };

    // Call on the game thread after kDataLoaded. Visitors run synchronously and
    // return false to stop. Snapshot pointers/strings are valid only during the callback.
    // Query results are false when AoT is unavailable or the visitor is null;
    // true includes a successful query with no matches. Zero filters mean all.
    struct Interface {
        bool (*VisitInstances)(RE::FormID location, RE::FormID item,
                               bool (*visitor)(const InstanceInfo*, void*), void* context);
        bool (*VisitSources)(RE::FormID source,
                             bool (*visitor)(const SourceInfo*, void*), void* context);
        bool (*VisitLocations)(bool (*visitor)(RE::FormID, void*), void* context);
    };

    [[nodiscard]] inline const Interface* RequestAPI() {
        using Request = const Interface* (*)(std::uint32_t);
        const auto module = GetModuleHandleW(L"AlchemyOfTime.dll");
        const auto request = module ? reinterpret_cast<Request>(GetProcAddress(module, "AoT_RequestAPI")) : nullptr;
        return request ? request(interface_version) : nullptr;
    }
}
