#pragma once
#include "Data.h"
#include "ClibUtilsQTR/Ticker.hpp"

class QueueManager;

class Manager final : public Ticker, public SaveLoadData {
    std::shared_mutex dirty_mtx_;
    std::unordered_map<RefID, RE::ObjectRefHandle> dirty_refs_;
    std::atomic<int32_t> n_instances_{0};

    void MarkDirty_(RE::TESObjectREFR* r);

    struct UpdateCtx {
        RE::TESObjectREFR* from;
        RE::TESObjectREFR* to;
        const RE::TESForm* what;
        Count count;
        RefID from_refid;
        bool refreshRefs;

        bool to_is_world_object;
        bool is_player_owned;

        FormID what_formid;
        RefID to_refid;
        FormID to_base_id;

        Duration curr_time;

        UpdateCtx(RE::TESObjectREFR* from,
                  RE::TESObjectREFR* to, const RE::TESForm* what,
                  Count count, RefID from_refid, bool refreshRefs);
    };

    static void RecalcCtx_(UpdateCtx& c);


    static void NormalizeWorldObjectCount_(UpdateCtx& ctx);

    // possibly locks: queueMutex_
    void QueueDeleteIfFromIsWorldObject_(const UpdateCtx& ctx);

    static void ApplyBarterMenuSemantics_(UpdateCtx& ctx);

    static void ApplyAlchemyNullSkip_(UpdateCtx& ctx);

    void ApplyTransferToSource_(Source& src, UpdateCtx& ctx, const InvMap& to_inv);

    void SplitWorldObjectStackIfNeeded_(Source& src, const UpdateCtx& ctx);

    void RefreshRefs_(const UpdateCtx& ctx);


    RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();

    // form_id1: source formid, formid2: stage formid, pair: <number of stage form, initial source count>
    std::map<Utils::Types::FormFormID, std::pair<Count, Count>> handle_crafting_instances;
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
    // maps stage formid to source formids
    std::unordered_map<FormID, std::unordered_set<FormID>> stage_to_sources;
    // maps location refid to source formids. we only care about location presence in sources in this one (e.g. the count is irrelevant)
    std::unordered_map<RefID, std::unordered_set<FormID>> loc_to_sources;

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

    [[nodiscard]] uint32_t GetNInstances();
    uint32_t GetNInstancesFast() const;

    // Creates and appends a new Source. [expects: sourceMutex_] (unique)
    [[nodiscard]] Source* MakeSource(FormID source_formid, const DefaultSettings* settings);

    void IndexSourceStages(const Source& source);

    void AddLocationIndex(RefID location_id, FormID source_formid);
    void RemoveLocationIndex(RefID location_id, FormID source_formid);
    void UpdateLocationIndexForSource(const Source& src, RefID location_id);
    void RefreshLocationIndex(RefID location_id);

    // Cleans up a Source instance. [expects: sourceMutex_] (unique)
    void CleanUpSourceData(Source* src);
    void CleanUpSourceData(Source* src, RefID a_loc);

    // Lookup by form id among existing sources. [expects: sourceMutex_] (shared)
    [[nodiscard]] Source* GetSource(FormID some_formid);
    // Lookup by ref id among existing sources. [expects: sourceMutex_] (shared)
    [[nodiscard]] Source* GetSourceByLocation(RefID location_id);

    // Get or create a Source; may mutate the sources list. [expects: sourceMutex_] (unique)
    [[nodiscard]] Source* ForceGetSource(FormID some_formid);

    static bool IsSource(FormID some_formid);

    // Returns pointer into Source::data; pointer valid only while sourceMutex_ remains held. 
    // [expects: sourceMutex_] (shared)
    [[nodiscard]] StageInstance* GetWOStageInstance(const RE::TESObjectREFR* wo_ref);

    static inline void ApplyStageInWorld_Fake(RE::TESObjectREFR* wo_ref, const char* xname);


    static void ApplyStageInWorld(RE::TESObjectREFR* wo_ref, const Stage& stage,
                                  RE::TESBoundObject* source_bound = nullptr);

    [[nodiscard]] static bool ApplyEvolutionInInventory(const RefInfo& a_info,
                                                        Count update_count,
                                                        FormID old_item, FormID new_item);

    static void RemoveItem(const RefInfo& moveFromInfo, FormID item_id, Count count);

    static void AddItem(const RefInfo& addToInfo, const RefInfo& addFromInfo, FormID item_id, Count count);

    void Init();

    // [expects: sourceMutex_] (shared)
    std::set<float> GetUpdateTimes(const RE::TESObjectREFR* inventory_owner);

    // [expects: sourceMutex_] (unique)
    bool UpdateInventory(const RefInfo& a_info, float t, const InvMap& inv);

    // [expects: sourceMutex_] (unique)
    void UpdateInventory(const RefInfo& a_info, const InvMap& inv);

    void UpdateQueuedWO(const RefInfo& ref_info, float curr_time);
    // [expects: sourceMutex_] (unique)
    void UpdateWO(RE::TESObjectREFR* ref);
    // [expects: sourceMutex_] (unique)
    void SyncWithInventory(const RefInfo& a_info, const InvMap& inv);


    // [expects: sourceMutex_] (unique)
    void UpdateRef(RE::TESObjectREFR* loc);

    // queue access helper, only safe under queueMutex_. Prefer using GetUpdateQueue() which locks internally.
    RefStop* GetRefStop(RefID refid);

    static bool RefIsUpdatable(const RE::TESObjectREFR* ref);
    // [expects: sourceMutex_] (unique)
    bool DeRegisterRef(RefID refid);

    using ScanRequest = std::pair<RefInfo, std::vector<FormID>>;

    [[nodiscard]] std::vector<ScanRequest> BuildCellScanRequests_(
        const std::vector<RefInfo>& refStopsCopy);

    static bool LocHasStage(Source* src, RefID loc, FormID stage_formid);

    // best-effort disambiguation: owner-ref first, then stage-only
    Source* UpdateGetSource(FormID stage_formid, RefID owner_refid);

    std::optional<float> GetNextUpdateTime(const RefInfo& a_info);

protected:
    void UpdateImpl(RE::TESObjectREFR* from, RE::TESObjectREFR* to, const RE::TESForm* what, Count count,
                    RefID from_refid, bool refreshRefs);
    friend QueueManager;

public:
    explicit Manager(const std::chrono::milliseconds interval)
        : Ticker([this]() { UpdateLoop(); }, interval) {
        Init();
    }

    static Manager* GetSingleton() {
        static Manager singleton(std::chrono::milliseconds(Settings::Ticker::GetInterval(Settings::ticker_speed)));
        return &singleton;
    }

    std::atomic<bool> isUninstalled = false;
    std::atomic<bool> isLoading = false;

    const char* GetType() override { return "Manager"; }

    void Uninstall() { isUninstalled.store(true); }

    // [locks: queueMutex_]
    void ClearWOUpdateQueue();

    // Registers instances; may mutate sources. [expects: sourceMutex_] (unique)
    void Register(FormID some_formid, Count count, const RefInfo& ref_info,
                  Duration register_time, const InvMap& a_inv);
    // Registers instances; may mutate sources. [expects: sourceMutex_] (unique)
    void Register(FormID some_formid, Count count, RefID location_refid, Duration register_time);

    // These read from sources under a shared_lock internally
    void HandleCraftingEnter(unsigned int bench_type);

    void HandleCraftingExit();

    // External entry point. Handles queue + source locking internally.
    void Update(RE::TESObjectREFR* from, RE::TESObjectREFR* to = nullptr, const RE::TESForm* what = nullptr,
                Count count = 0, RefID from_refid = 0);

    // Swap based on stage instance. Holds sourceMutex_ internally. (shared)
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

    // Snapshot copy of sources matching a stage form and location. [locks: sourceMutex_] (shared)
    std::vector<Source> GetSourcesByStageAndOwner(FormID stage_formid, RefID location_id);

    // Snapshot of the update queue. [locks: queueMutex_] (shared)
    std::unordered_map<RefID, float> GetUpdateQueue();

    // [expects: sourceMutex_] (shared)
    void HandleDynamicWO(RE::TESObjectREFR* ref);

    // Note: called from contexts that already hold sourceMutex_. Do not acquire it inside. 
    // [expects: sourceMutex_] (shared)
    void HandleWOBaseChange(RE::TESObjectREFR* ref);

    bool IsTickerActive() const {
        return isRunning();
    }

    std::vector<RefInfo> GetRefStops();

    void IndexStage(FormID stage_formid, FormID source_formid);

    void ProcessDirtyRefs_();

    void InstanceCountUpdate(int32_t delta);
};

inline Manager* M = nullptr;