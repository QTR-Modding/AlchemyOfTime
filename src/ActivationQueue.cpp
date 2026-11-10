#include "ActivationQueue.h"
#include "Manager.h"

ActivationQueue::ActivationQueue()
    : Ticker([this]() { UpdateLoop(); }, std::chrono::seconds(1)) {
}

ActivationQueue::~ActivationQueue() {
    Join();
}

std::size_t ActivationQueue::KeyHash::operator()(const Key& key) const noexcept {
    const auto& [source_id, base_id, trigger_id, kind] = key;
    const std::uint32_t ids[]{source_id, base_id, trigger_id, std::to_underlying(kind)};
    return RE::BSCRC32<std::string_view>{}(
        std::string_view(reinterpret_cast<const char*>(ids), sizeof(ids)));
}

bool ActivationQueue::CanActivate(const RefID refid, const Key& key, const float activation_delay) {
    if (activation_delay == 0.0f) {
        return true;
    }

    std::lock_guard lock(mutex_);
    const auto [it, inserted] = pending_[refid].try_emplace(key, Entry{activation_delay});
    if (inserted && !isRunning()) {
        last_update_ = RE::Calendar::GetSingleton()->GetHoursPassed();
        Start();
    }
    return it->second.ready;
}

void ActivationQueue::Reset() {
    std::lock_guard lock(mutex_);
    Stop();
    pending_.clear();
}

std::vector<ActivationQueue::Key> ActivationQueue::GetQueuedTriggers(const RefID refid) {
    std::lock_guard lock(mutex_);
    std::vector<Key> result;
    if (const auto ref = pending_.find(refid); ref != pending_.end()) {
        result.reserve(ref->second.size());
        for (const auto& key : ref->second | std::views::keys) {
            result.push_back(key);
        }
    }
    return result;
}

void ActivationQueue::RemoveTrigger(const RefID refid, const Key& key) {
    std::lock_guard lock(mutex_);
    const auto ref = pending_.find(refid);
    if (ref == pending_.end()) {
        return;
    }
    ref->second.erase(key);
    if (ref->second.empty()) {
        pending_.erase(ref);
    }
    if (pending_.empty()) {
        Stop();
    }
}

void ActivationQueue::UpdateLoop() {
    std::vector<RefID> activated;
    {
        std::lock_guard lock(mutex_);
        if (pending_.empty() || M->isLoading.load() || M->isUninstalled.load()) {
            return;
        }
        if (const auto ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
            return;
        }
        const auto calendar = RE::Calendar::GetSingleton();
        if (!calendar) {
            return;
        }
        const auto now = calendar->GetHoursPassed();
        const auto elapsed = now - std::exchange(last_update_, now);
        if (elapsed <= 0.0f) {
            return;
        }

        bool waiting = false;
        for (auto& [refid, triggers] : pending_) {
            bool hit = false;
            for (auto& entry : triggers | std::views::values) {
                if (entry.ready) {
                    continue;
                }
                if (random_.generate() < -std::expm1(-elapsed / entry.activation_delay)) {
                    entry.ready = true;
                    hit = true;
                } else {
                    waiting = true;
                }
            }
            if (hit) {
                activated.push_back(refid);
            }
        }
        if (!waiting) {
            Stop();
        }
    }

    if (activated.empty()) {
        return;
    }
    SKSE::GetTaskInterface()->AddTask([this, activated = std::move(activated)] {
        RequestUpdates(activated);
    });
}

void ActivationQueue::RequestUpdates(const std::vector<RefID>& refids) {
    std::lock_guard lock(mutex_);
    for (const auto refid : refids) {
        const auto it = pending_.find(refid);
        if (it == pending_.end() ||
            !std::ranges::any_of(it->second | std::views::values, &Entry::ready)) {
            continue;
        }
        if (const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid)) {
            M->RequestRefUpdate(ref);
        }
    }
}
