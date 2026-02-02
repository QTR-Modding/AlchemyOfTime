#include "Queue.h"
#include "Hooks.h"
#include "Manager.h"

void QueueManager::UpdateLoop() {
    if (!ProcessPendingMoves(n_tasks_per_tick)) {
        ProcessPendingProcess(n_tasks_per_tick);
    }
}

void QueueManager::UpdateLoopPlayer() {
}

std::size_t QueueManager::KeyHash::operator()(const Key& k) const noexcept {
    const std::uint64_t packed = (static_cast<std::uint64_t>(k.first) << 32) | static_cast<std::uint64_t>(k.second);
    return std::hash<std::uint64_t>{}(packed);
}

void QueueManager::ProcessAddItemTask(RE::TESObjectREFR* owner, const AddItemTask& task) {
    const auto item = RE::TESForm::LookupByID<RE::TESBoundObject>(task.item_id);
    if (!item) {
        logger::error("ProcessAddItemTask: Item {} not found.", task.item_id);
        return;
    }
    owner->AddObjectToContainer(item, nullptr, task.count, nullptr);
}

Count QueueManager::ProcessRemoveItemTask(RE::TESObjectREFR* owner, const RemoveItemTask& task,
                                          RE::TESObjectREFR::InventoryItemMap& inventory) {
    const auto item = RE::TESForm::LookupByID<RE::TESBoundObject>(task.item_id);
    if (!item) {
        logger::error("ProcessRemoveItemTask: Item {:x} not found.", task.item_id);
        return 0;
    }
    const auto it = inventory.find(item);
    if (it == inventory.end()) {
        logger::error("ProcessRemoveItemTask: Item {:x} not in inventory.", task.item_id);
        return 0;
    }
    if (it->second.second->IsQuestObject()) {
        return 0;
    }
    const auto count = std::min(it->second.first, task.count);
    if (count <= 0) {
        return 0;
    }

    owner->RemoveItem(item, count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

    it->second.first -= count;

    return count;
}

void QueueManager::ProcessPendingProcess(const int n_tasks) {
    auto to_process = RequestPendingProcess(n_tasks);

    if (to_process.empty()) {
        return;
    }

    for (const auto& [key, transfers] : to_process) {
        const auto from = key.first > 0 ? RE::TESForm::LookupByID<RE::TESObjectREFR>(key.first) : nullptr;
        auto from_handle = from ? from->GetHandle() : RE::ObjectRefHandle{};
        const auto to = key.second > 0 ? RE::TESForm::LookupByID<RE::TESObjectREFR>(key.second) : nullptr;
        auto to_handle = to ? to->GetHandle() : RE::ObjectRefHandle{};
        for (const auto& [what, count, from_refid] : transfers) {
            const auto a_item = what > 0 ? RE::TESForm::LookupByID<RE::TESForm>(what) : nullptr;
            M->UpdateImpl(from_handle.get().get(), to_handle.get().get(), a_item, count, from_refid, false);
        }
        M->UpdateImpl(from_handle.get().get(), nullptr, nullptr, 0, 0, true);
        M->UpdateImpl(to_handle.get().get(), nullptr, nullptr, 0, 0, true);
    }
}

bool QueueManager::ProcessPendingMoves(const int n_tasks) {
    auto move_item_tasks = RequestPendingMoveItem(n_tasks);

    if (move_item_tasks.empty()) {
        return false;
    }

    SKSE::GetTaskInterface()->AddTask([move_item_tasks = std::move(move_item_tasks)]() mutable {
        //ListenGuard lg(Hooks::listen_disable_depth);

        for (const auto& [refid, tasks] : move_item_tasks) {
            if (refid == 0) continue;

            const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid);
            if (!ref) continue;

            auto h = ref->GetHandle();
            const auto owner = h.get().get();
            if (!owner) continue;

            // 1) Snapshot inventory ONCE
            auto inv = owner->GetInventory();

            std::unordered_map<FormID, Count> available;
            available.reserve(inv.size());

            std::unordered_set<FormID> questLocked;
            questLocked.reserve(inv.size());

            for (const auto& [obj, entry] : inv) {
                if (!obj) continue;
                const auto fid = obj->GetFormID();

                available[fid] = static_cast<Count>(entry.first);

                // IMPORTANT: deref entry.second only here, before any RemoveItem()
                if (entry.second && entry.second->IsQuestObject()) {
                    questLocked.insert(fid);
                }
            }

            // 2) Execute tasks without touching InventoryEntryData pointers again
            for (const auto& [add, remove] : tasks) {
                Count removed_count = 0;

                if (remove.count > 0) {
                    if (const FormID rid = remove.item_id; rid != 0 && !questLocked.contains(rid)) {
                        if (const auto item = RE::TESForm::LookupByID<RE::TESBoundObject>(rid)) {
                            if (const Count canRemove = std::min(available[rid], remove.count); canRemove > 0) {
                                owner->RemoveItem(item, canRemove, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                                available[rid] -= canRemove;
                                removed_count = canRemove;
                            }
                        }
                    }
                }

                if (const Count add_count = (remove.count > 0) ? removed_count : add.count; add_count > 0) {
                    if (const auto item = RE::TESForm::LookupByID<RE::TESBoundObject>(add.item_id)) {
                        owner->AddObjectToContainer(item, nullptr, add_count, nullptr);
                    }
                }
            }
        }

        if (Hooks::is_menu_open) {
            RefreshUI();
        }
    });

    return true;
}

void QueueManager::RefreshUI() {
    SKSE::GetTaskInterface()->AddUITask([] {
        RE::SendUIMessage::SendInventoryUpdateMessage(RE::PlayerCharacter::GetSingleton(), nullptr);
        Utils::Menu::UpdateItemList();
    });
}

//void QueueManager::QueueUpdate(const RE::TESObjectREFR* from, const RE::TESObjectREFR* to, const RE::TESForm* what,
//                               const Count count, const RefID from_refid) {
//    {
//        auto from_id = from ? from->GetFormID() : 0;
//        auto to_id = to ? to->GetFormID() : 0;
//        const auto what_id = what ? what->GetFormID() : 0;
//        if (from_id == 0 && to_id == 0) {
//            return;
//        }
//        std::lock_guard lock(mutex_process_);
//        pending_process[{from_id, to_id}].push_back(Transfer{what_id, count, from_refid});
//    }
//}

void QueueManager::QueueAddRemoveItemTask(const AddItemTask& add_task, const RemoveItemTask& remove_task) {
    {
        const auto add_id = add_task.to;
        const auto remove_id = remove_task.from;
        if (add_id != remove_id && add_id != 0 && remove_id != 0) {
            logger::warn(
                "QueueAddRemoveItemTask: add_task.to and remove_task.from are different and non-zero; dropping task");
            return;
        }
        const RefID owner = (add_id != 0) ? add_id : (remove_id != 0) ? remove_id : 0;

        if (owner == 0) {
            logger::warn("QueueAddRemoveItemTask: owner is 0; dropping task");
            return;
        }

        std::lock_guard lock(mutex_moveitem_);
        pending_moveitem_[owner].push_back(AddRemoveItemTask{add_task, remove_task});
    }
}


QueueManager::PendingMap QueueManager::RequestPendingProcess(int n_pending) {
    PendingMap result;
    if (n_pending <= 0) {
        return result;
    }

    {
        std::lock_guard lock(mutex_process_);
        for (auto it = pending_process.begin(); it != pending_process.end() && n_pending > 0;) {
            auto& q = it->second;

            if (q.empty()) {
                it = pending_process.erase(it);
                continue;
            }

            auto& out = result[it->first];

            while (!q.empty() && n_pending > 0) {
                out.push_back(std::move(q.front()));
                q.pop_front();
                --n_pending;
            }

            if (q.empty()) {
                it = pending_process.erase(it);
            } else {
                ++it;
            }
        }
    }

    return result;
}


std::unordered_map<RefID, std::vector<AddRemoveItemTask>> QueueManager::RequestPendingMoveItem(int n_pending) {
    std::unordered_map<RefID, std::vector<AddRemoveItemTask>> result;
    if (n_pending <= 0) return result;
    {
        std::lock_guard lock(mutex_moveitem_);
        for (auto it = pending_moveitem_.begin(); it != pending_moveitem_.end() && n_pending > 0;) {
            auto& q = it->second;
            std::vector<AddRemoveItemTask> batch;
            batch.reserve(static_cast<size_t>(n_pending));
            while (!q.empty() && n_pending > 0) {
                batch.push_back(std::move(q.front()));
                q.pop_front();
                --n_pending;
            }
            if (!batch.empty()) {
                result[it->first] = std::move(batch);
            }
            if (q.empty()) {
                it = pending_moveitem_.erase(it);
            } else {
                ++it;
            }
        }
    }
    return result;
}

bool QueueManager::HasPendingMoveItemTasks() {
    std::lock_guard lock(mutex_moveitem_);
    return !pending_moveitem_.empty();
}