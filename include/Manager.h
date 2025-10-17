#pragma once
#include "Data.h"
#include "Ticker.h"

class Manager final : public Ticker, public SaveLoadData {
	RE::TESObjectREFR* player_ref = RE::PlayerCharacter::GetSingleton()->As<RE::TESObjectREFR>();
	
    std::map<Types::FormFormID, std::pair<int, Count>> handle_crafting_instances;  // formid1: source formid, formid2: stage formid
    std::unordered_map<FormID, bool> faves_list;
    std::unordered_map<FormID, bool> equipped_list;

    std::unordered_map<RefID, std::vector<FormID>> locs_to_be_handled;  // onceki sessiondan kalan fake formlar

    bool should_reset = false;

    // 0x0003eb42 damage health

    std::shared_mutex sourceMutex_;
    std::shared_mutex queueMutex_;

    std::vector<Source> sources;

    std::unordered_map<std::string, bool> _other_settings;

    unsigned int _instance_limit = 200000;

    // queueMutex_ guards these
    std::unordered_map<RefID, RefStop> _ref_stops_;
    std::unordered_set<RefID> queue_delete_;

    std::unordered_set<FormID> do_not_register;

    static void PreDeleteRefStop(RefStop& a_ref_stop, RE::NiAVObject* a_obj);

    // Ticker thread entry. [locks: queueMutex_]
    void UpdateLoop();

    // Enqueue/merge a RefStop. [locks: queueMutex_]
    void QueueWOUpdate(const RefStop& a_refstop);

    static void UpdateRefStop(Source& src, const StageInstance& wo_inst, RefStop& a_ref_stop, float stop_t);

    // [expects: sourceMutex_] (read-only traversal)
    [[nodiscard]] unsigned int GetNInstances();

    // Creates and appends a new Source. [expects: sourceMutex_] (unique)
    [[nodiscard]] Source* MakeSource(FormID source_formid, const DefaultSettings* settings);

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


    static void ApplyStageInWorld(RE::TESObjectREFR* wo_ref, const Stage& stage, RE::TESBoundObject* source_bound = nullptr);

    static inline void ApplyEvolutionInInventoryX(RE::TESObjectREFR* inventory_owner, Count update_count, FormID old_item,
                                                   FormID new_item);

    static inline void ApplyEvolutionInInventory_(RE::TESObjectREFR* inventory_owner, Count update_count, FormID old_item,
                                                  FormID new_item);

    void ApplyEvolutionInInventory(const std::string& _qformtype_, RE::TESObjectREFR* inventory_owner, Count update_count,
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

    // [expects: sourceMutex_] (unique)
    void UpdateWO(RE::TESObjectREFR* ref);
	// [expects: sourceMutex_] (unique)
    void SyncWithInventory(RE::TESObjectREFR* ref);

    // [expects: sourceMutex_] (unique)
    void UpdateRef(RE::TESObjectREFR* loc);

	// queue access helper, only safe under queueMutex_. Prefer using GetUpdateQueue() which locks internally.
	RefStop* GetRefStop(RefID refid);

public:
    Manager(const std::vector<Source>& data, const std::chrono::milliseconds interval)
        : Ticker([this]() { UpdateLoop(); }, interval), sources(data) {
        Init();
    }

    static Manager* GetSingleton(const std::vector<Source>& data, const int u_intervall = Settings::Ticker::GetInterval(Settings::ticker_speed)) {
        static Manager singleton(data, std::chrono::milliseconds(u_intervall));
        return &singleton;
    }

    // Use Or Take Compatibility
    std::atomic<bool> listen_equip = true;
    std::atomic<bool> listen_container_change = true;

    std::atomic<bool> isUninstalled = false;
    std::atomic<bool> isLoading = false;

	const char* GetType() override { return "Manager"; }

    void Uninstall() {isUninstalled.store(true);} 

	// [locks: queueMutex_]
	void ClearWOUpdateQueue() {
		std::unique_lock lock(queueMutex_);
	    _ref_stops_.clear();
	}

    // use it only for world objects! checks if there is a stage instance for the given refid
    [[nodiscard]] bool RefIsRegistered(RefID refid);

    // Registers instances; may mutate sources. [expects: sourceMutex_] (unique)
    void Register(FormID some_formid, Count count, RefID location_refid,
                                           Duration register_time = 0);

	// These read from sources under a shared_lock internally
	void HandleCraftingEnter(unsigned int bench_type);

	void HandleCraftingExit();

    // External entry point. Handles queue + source locking internally.
    void Update(RE::TESObjectREFR* from, RE::TESObjectREFR* to=nullptr, const RE::TESForm* what=nullptr, Count count=0, RefID from_refid=0);

	// Swap based on stage instance. Requires sourceMutex_ held for pointer lifetime.
    void SwapWithStage(RE::TESObjectREFR* wo_ref);

    // Clears and resets all data. [locks: sourceMutex_] (unique) + [locks: queueMutex_]
    void Reset();

	// [locks: sourceMutex_] (unique)
	void HandleFormDelete(FormID a_refid);

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
    std::vector<Source> GetSources() {
		std::shared_lock lock(sourceMutex_);
        return sources;
    }

    // Snapshot of the update queue. [locks: queueMutex_] (shared)
    std::unordered_map<RefID, float> GetUpdateQueue() {
		std::unordered_map<RefID, float> _ref_stops_copy;
		std::shared_lock lock(queueMutex_);
		for (const auto& [key, value] : _ref_stops_) {
			_ref_stops_copy[key] = value.stop_time;
		}
        return _ref_stops_copy;
    }

	void HandleDynamicWO(RE::TESObjectREFR* ref);

    // Note: called from contexts that already hold sourceMutex_. Do not acquire it inside.
    void HandleWOBaseChange(RE::TESObjectREFR* ref);

	bool IsTickerActive() const {
	    return isRunning();
	}

};


inline Manager* M = nullptr;