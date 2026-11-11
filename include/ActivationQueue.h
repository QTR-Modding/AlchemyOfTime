#pragma once
#include <REX/REX/Singleton.h>
#include <CLibUtil/rng.hpp>
#include "ClibUtilsQTR/Ticker.hpp"
#include "Data.h"

class ActivationQueue : public REX::Singleton<ActivationQueue>, public Ticker {
public:
    using Key = std::tuple<FormID, FormID, FormID, TriggerKind>; // source, item, trigger, kind

    ActivationQueue();
    ~ActivationQueue();

    void Reset();
    void ClearRef(RefID refid);
    // Called during trigger selection while Manager's source lock is held.
    void Refresh(const RefInfo& ref_info, const std::unordered_map<FormID, std::unique_ptr<Source>>& sources,
                 const Utils::Inventory::Snapshot& inventory);
    void Refresh(RE::TESObjectREFR* world_ref, const Source& source, WorldTriggerMatchContext& context);
    [[nodiscard]] bool CanActivate(RefID refid, const Key& key, float activation_delay, float update_time);

private:
    std::vector<Key> GetQueuedTriggers(RefID refid);
    void RemoveTrigger(RefID refid, const Key& key);
    void UpdateLoop();
    void RequestUpdates(const std::vector<RefID>& refids);

    struct KeyHash {
        std::size_t operator()(const Key& key) const noexcept;
    };

    struct Entry {
        float activation_delay;
        std::optional<float> ready_time;
    };

    float last_update_ = 0.0f;
    clib_util::RNG<> random_;
    std::mutex mutex_;
    std::unordered_map<RefID, std::unordered_map<Key, Entry, KeyHash>> pending_;
};