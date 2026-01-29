#pragma once
#include "Data.h"
#include "ClibUtilsQTR/Ticker.hpp"

class Manager final : public Ticker, public SaveLoadData {
    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();

    // form_id1: source formid, formid2: stage formid, pair: <number of stage form, initial source count>
    std::map<Types::FormFormID, std::pair<Count, Count>> handle_crafting_instances;
    std::unordered_map<FormID, bool> faves_list;
    std::unordered_map<FormID, bool> equipped_list;

    std::unordered_map<RefID, std::vector<FormID>> locs_to_be_handled; // onceki sessiondan kalan fake formlar

    bool should_reset = false;

    // 0x0003eb42 damage health

    // LOCKING CONVENTION (debug assertions enforce this in Manager.cpp):
    // Acquire sourceMutex_ before queueMutex_ when both are needed.
    // It is legal to lock queueMutex_ while holding sourceMutex_.
    // It is ILLEGAL to acquire sourceMutex_ while any shared/unique lock on queueMutex_ is held (deadlock prevention).
    // No re-entrancy: the same thread must not take the same mutex twice (shared or unique) and no upgrade/downgrade.
    // Methods are annotated with [expects: ...] or [locks: ...] to indicate required / performed locking.
    // Use SRC_SHARED_GUARD / SRC_UNIQUE_GUARD then QUE_SHARED_GUARD / QUE_UNIQUE_GUARD in that order.
    std::shared_mutex sourceMutex_;
    std::shared_mutex queueMutex_;

    std::unordered_map<FormID, std::unique_ptr<Source>> sources;
    std::unordered_map<FormID, Source*> stage_to_source;

    std::unordered_map<std::string, bool> _other_settings;

    unsigned int _instance_limit = 200000;

    // queueMutex_ guards these
    std::unordered_map<RefID, RefStop> _ref_stops_;
    std::unordered_set<RefID> queue_delete_;

    std::unordered_set<FormID> do_not_register;

    static void PreDeleteRefStop(RefStop& a_ref_stop);

    // Ticker thread entry. [locks: queueMutex_]
    void UpdateLoop();

    // Enqueue/merge a RefStop. [locks: queueMutex_]
    void QueueWOUpdate(const RefStop& a_refstop);

    static void UpdateRefStop(const Source& src, const StageInstance& wo_inst, RefStop& a_ref_stop, float stop_t);

    // [expects: sourceMutex_] (read-only traversal)
    [[nodiscard]] unsigned int GetNInstances();

    // Creates and appends a new Source. [expects: sourceMutex_] (unique)
    [[nodiscard]] Source* MakeSource(FormID source_formid, const DefaultSettings* settings);

    void IndexSourceStages(Source& source);

    // Cleans up a Source instance. [expects: sourceMutex_] (unique)
    static void CleanUpSourceData(Source* src);

    // Lookup by form id among existing sources. [expects: sourceMutex_] (shared)
    [[nodiscard]] Source* GetSource(FormID some_formid);

    // Get or create a Source; may mutate the sources list. [expects: sourceMutex_] (unique)
    [[nodiscard]] Source* ForceGetSource(FormID some_formid);

    static bool IsSource(FormID some_formid);

    // Returns pointer into Source::data; pointer valid only while sourceMutex_ remains held. [expects: sourceMutex_] (shared)
    [[nodiscard]] StageInstance* GetWOStageInstance(const RE::TESObjectREFR* wo_ref);

    static inline void ApplyStageInWorld_Fake(RE::TESObjectREFR* wo_ref, const char* xname);


    static void ApplyStageInWorld(RE::TESObjectREFR* wo_ref, const Stage& stage,
                                  RE::TESBoundObject* source_bound = nullptr);

    [[nodiscard]] static bool ApplyEvolutionInInventory(RE::TESObjectREFR* inventory_owner,
                                                        Count update_count,
                                                        FormID old_item, FormID new_item);

    static inline void RemoveItem(RE::TESObjectREFR* moveFrom, FormID item_id, Count count);

    static void AddItem(RE::TESObjectREFR* addTo, RE::TESObjectREFR* addFrom, FormID item_id, Count count);

    void Init();

    // [expects: sourceMutex_] (shared)
    std::set<float> GetUpdateTimes(const RE::TESObjectREFR* inventory_owner);

    // [expects: sourceMutex_] (unique)
    bool UpdateInventory(RE::TESObjectREFR* ref, float t);

    // [expects: sourceMutex_] (unique)
    void UpdateInventory(RE::TESObjectREFR* ref);

    void UpdateQueuedWO(RefID refid, FormID hinted_source_formid);

    // [expects: sourceMutex_] (unique)
    void UpdateWO(RE::TESObjectREFR* ref);
    // [expects: sourceMutex_] (unique)
    void SyncWithInventory(RE::TESObjectREFR* ref);

    // [expects: sourceMutex_] (unique)
    void UpdateRef(RE::TESObjectREFR* loc);

    // queue access helper, only safe under queueMutex_. Prefer using GetUpdateQueue() which locks internally.
    RefStop* GetRefStop(RefID refid);

    static bool RefIsUpdatable(const RE::TESObjectREFR* ref);
    // [expects: sourceMutex_] (unique)
    bool DeRegisterRef(RefID refid);

    using ScanRequest = std::pair<RefID, std::vector<FormID>>;

    [[nodiscard]] std::vector<ScanRequest> BuildCellScanRequests_(
        const std::vector<std::pair<RefID, FormID>>& refStopsCopy);

public:
    explicit Manager(const std::chrono::milliseconds interval)
        : Ticker([this]() { UpdateLoop(); }, interval) {
        Init();
    }

    static Manager* GetSingleton() {
        static Manager singleton(std::chrono::milliseconds(Settings::Ticker::GetInterval(Settings::ticker_speed)));
        return &singleton;
    }

    // Use Or Take Compatibility
    std::atomic<bool> listen_equip = true;
    std::atomic<bool> listen_container_change = true;

    std::atomic<bool> isUninstalled = false;
    std::atomic<bool> isLoading = false;

    const char* GetType() override { return "Manager"; }

    void Uninstall() { isUninstalled.store(true); }

    // [locks: queueMutex_]
    void ClearWOUpdateQueue();

    // use it only for world objects! checks if there is a stage instance for the given refid
    [[nodiscard]] bool RefIsRegistered(RefID refid);

    // Registers instances; may mutate sources. [expects: sourceMutex_] (unique)
    void Register(FormID some_formid, Count count, RefID location_refid,
                  Duration register_time = 0);

    // These read from sources under a shared_lock internally
    void HandleCraftingEnter(unsigned int bench_type);

    void HandleCraftingExit();

    // External entry point. Handles queue + source locking internally.
    void Update(RE::TESObjectREFR* from, RE::TESObjectREFR* to = nullptr, const RE::TESForm* what = nullptr,
                Count count = 0, RefID from_refid = 0);

    // Swap based on stage instance. Requires sourceMutex_ held for pointer lifetime.
    void SwapWithStage(RE::TESObjectREFR* wo_ref);

    // Clears and resets all data. [locks: sourceMutex_] (unique) + [locks: queueMutex_]
    void Reset();

    // [locks: sourceMutex_] (unique)
    bool HandleFormDelete(FormID a_refid);

    // Serialisation helpers. [locks: sourceMutex_] (shared)
    void SendData();

    // for syncing the previous session's (fake form) data with the current session
    // [expects: sourceMutex_] (unique)
    void HandleLoc(RE::TESObjectREFR* loc_ref);

    // Restore instances on load; mutates sources. [expects: sourceMutex_] (unique)
    StageInstance* RegisterAtReceiveData(FormID source_formid, RefID loc,
                                         const StageInstancePlain& st_plain);

    void ReceiveData();

    void Print();

    // Snapshot copy of sources (read-only). [locks: sourceMutex_] (shared)
    std::vector<Source> GetSources();

    // Snapshot of the update queue. [locks: queueMutex_] (shared)
    std::unordered_map<RefID, float> GetUpdateQueue();

    void HandleDynamicWO(RE::TESObjectREFR* ref);

    // Note: called from contexts that already hold sourceMutex_. Do not acquire it inside.
    void HandleWOBaseChange(RE::TESObjectREFR* ref);

    bool IsTickerActive() const {
        return isRunning();
    }

    bool IsStageItem(FormID a_formid);

    std::vector<std::pair<RefID, FormID>> GetRefStops();

    void IndexStage(FormID stage_formid, Source* src);
};

inline Manager* M = nullptr;