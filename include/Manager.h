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

    std::unordered_map<RefID, RefStop> _ref_stops_;
    std::unordered_set<RefID> queue_delete_;

    std::unordered_set<FormID> do_not_register;

    void WoUpdateLoop(const std::vector<RefID>& refs);

    static void PreDeleteRefStop(RefStop& a_ref_stop, RE::NiAVObject* a_obj);

    void UpdateLoop();

    void QueueWOUpdate(const RefStop& a_refstop);

    static void UpdateRefStop(Source& src, const StageInstance& wo_inst, RefStop& a_ref_stop, float stop_t);

    [[nodiscard]] unsigned int GetNInstances();

    [[nodiscard]] Source* MakeSource(FormID source_formid, const DefaultSettings* settings);

    static void CleanUpSourceData(Source* src);

    [[nodiscard]] Source* GetSource(FormID some_formid);

    [[nodiscard]] Source* ForceGetSource(FormID some_formid);

    static bool IsSource(FormID some_formid);

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

    std::set<float> GetUpdateTimes(const RE::TESObjectREFR* inventory_owner);
    bool UpdateInventory(RE::TESObjectREFR* ref, float t);
    void UpdateInventory(RE::TESObjectREFR* ref);
    void UpdateWO(RE::TESObjectREFR* ref);
	void SyncWithInventory(RE::TESObjectREFR* ref);
    void UpdateRef(RE::TESObjectREFR* loc);

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

	const char* GetType() override { return "Manager"; }

    void Uninstall() {isUninstalled.store(true);}

	void ClearWOUpdateQueue() {
		std::unique_lock lock(queueMutex_);
	    _ref_stops_.clear();
	}

    // use it only for world objects! checks if there is a stage instance for the given refid
    [[nodiscard]] bool RefIsRegistered(RefID refid);

    void Register(FormID some_formid, Count count, RefID location_refid,
                                           Duration register_time = 0);

	void HandleCraftingEnter(unsigned int bench_type);

	void HandleCraftingExit();

    void Update(RE::TESObjectREFR* from, RE::TESObjectREFR* to=nullptr, const RE::TESForm* what=nullptr, Count count=0);

	void SwapWithStage(RE::TESObjectREFR* wo_ref);

    void Reset();

	void HandleFormDelete(FormID a_refid);

    void SendData();

    // for syncing the previous session's (fake form) data with the current session
    void HandleLoc(RE::TESObjectREFR* loc_ref);
    
    StageInstance* RegisterAtReceiveData(FormID source_formid, RefID loc,
                                          const StageInstancePlain& st_plain);

    void ReceiveData();

    void Print();

    std::vector<Source> GetSources() {
		std::shared_lock lock(sourceMutex_);
        return sources;
    }

    std::unordered_map<RefID, float> GetUpdateQueue() {
		std::unordered_map<RefID, float> _ref_stops_copy;
		std::shared_lock lock(queueMutex_);
		for (const auto& [key, value] : _ref_stops_) {
			_ref_stops_copy[key] = value.stop_time;
		}
        return _ref_stops_copy;
    }

	void HandleDynamicWO(RE::TESObjectREFR* ref);

    void HandleWOBaseChange(RE::TESObjectREFR* ref);

	bool IsTickerActive() const {
	    return isRunning();
	}

};