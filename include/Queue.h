#pragma once
#include <REX/REX/Singleton.h>
#include "ClibUtilsQTR/Ticker.hpp"

struct AddItemTask {
    RefID to = 0;
    RefID from = 0;
    FormID item_id = 0;
    Count count = 0;
};

struct RemoveItemTask {
    RefID from = 0;
    FormID item_id = 0;
    Count count = 0;
};

struct AddRemoveItemTask {
    AddItemTask add;
    RemoveItemTask remove;
};

class QueueManager : public REX::Singleton<QueueManager>, public RE::BSTEventSink<RE::TESLoadGameEvent> {
    void UpdateLoop();
    static void UpdateLoopPlayer();

    int ticker_speed = 100;
    //int player_ticker_speed = 1000;

    int n_tasks_per_tick = 1000;

    Ticker ticker{[this]() { UpdateLoop(); }, std::chrono::milliseconds(ticker_speed)};
    //Ticker player_ticker{[this]() { UpdateLoopPlayer(); }, std::chrono::milliseconds(player_ticker_speed)};

    struct Transfer {
        FormID what{0};
        Count count{0};
        RefID from_refid{0};
    };

    using Key = std::pair<RefID, RefID>;

    struct KeyHash {
        std::size_t operator()(const Key& k) const noexcept;
    };

    using PendingMap = std::unordered_map<Key, std::deque<Transfer>, KeyHash>;
    using WorkItem = std::pair<Key, Transfer>;

    std::mutex mutex_process_;
    PendingMap pending_process;
    std::mutex mutex_moveitem_;
    std::unordered_map<RefID, std::deque<AddRemoveItemTask>> pending_moveitem_;

    static void ProcessAddItemTask(RE::TESObjectREFR* owner, const AddItemTask& task);
    static Count ProcessRemoveItemTask(RE::TESObjectREFR* owner, const RemoveItemTask& task,
                                       RE::TESObjectREFR::InventoryItemMap& inventory);

    bool ProcessPendingMoves(int n_tasks);
    void ProcessPendingProcess(int n_tasks);

    static void RefreshUI();

    static void PruneAddRemoveItemTasks(std::unordered_map<RefID, std::vector<AddRemoveItemTask>>& tasks_to_prune);

public:
    void Start() {
        if (!ticker.isRunning()) {
            ticker.Start();
        }
        //player_ticker.Start();
    }

    // doesnt run on game thread. unsafe as is.
    /*void QueueUpdate(const RE::TESObjectREFR* from, const RE::TESObjectREFR* to = nullptr,
                     const RE::TESForm* what = nullptr,
                     Count count = 0, RefID from_refid = 0);*/

    void QueueAddRemoveItemTask(const AddItemTask& add_task, const RemoveItemTask& remove_task);

    PendingMap RequestPendingProcess(int n_pending);
    std::unordered_map<RefID, std::vector<AddRemoveItemTask>> RequestPendingMoveItem(int n_pending);
    bool HasPendingMoveItemTasks();

    RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* a_event,
                                          RE::BSTEventSource<RE::TESLoadGameEvent>* a_eventSource) override {
        /*logger::info("QueueManager received TESLoadGameEvent, starting player update ticker");
        player_ticker.Start();*/
        return RE::BSEventNotifyControl::kContinue;
    }
};