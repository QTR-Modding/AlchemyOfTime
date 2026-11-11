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

bool ActivationQueue::CanActivate(const RefID refid, const Key& key, const float activation_delay,
                                  const float update_time) {
    if (activation_delay == 0.0f) {
        return true;
    }

    std::lock_guard lock(mutex_);
    const auto [it, inserted] = pending_[refid].try_emplace(key, Entry{activation_delay});
    if (inserted && !isRunning()) {
        last_update_ = RE::Calendar::GetSingleton()->GetHoursPassed();
        Start();
    }
    return it->second.ready_time && update_time >= *it->second.ready_time;
}

void ActivationQueue::Reset() {
    std::lock_guard lock(mutex_);
    Stop();
    pending_.clear();
}

void ActivationQueue::ClearRef(const RefID refid) {
    std::lock_guard lock(mutex_);
    pending_.erase(refid);
    if (pending_.empty()) {
        Stop();
    }
}

void ActivationQueue::Refresh(const RefInfo& ref_info,
                              const std::unordered_map<FormID, std::unique_ptr<Source>>& sources,
                              const Utils::Inventory::Snapshot& inventory) {
    // Matching uses a local cache; refresh does not register inventory watches.
    QueueInfo matching{.ref_info = ref_info};
    for (const auto& key : GetQueuedTriggers(ref_info.ref_id)) {
        const auto& [source_id, base_id, trigger_id, kind] = key;
        bool valid = false;
        const auto source = sources.find(source_id);
        if (source != sources.end() && source->second->IsHealthy()) {
            const auto& src = *source->second;
            if (const auto instances = src.data.find(ref_info.ref_id); instances != src.data.end()) {
                valid = std::ranges::any_of(instances->second, [&](const StageInstance& instance) {
                    return instance.count > 0 && !instance.xtra.is_decayed && instance.xtra.form_id == base_id &&
                           MatchesInventoryTrigger(src.settings, trigger_id, kind, inventory, matching,
                                                   instance, UpdatePhase::kCurrent);
                });
            }
        }
        if (!valid) {
            RemoveTrigger(ref_info.ref_id, key);
        }
    }
}

void ActivationQueue::Refresh(const RefInfo& ref_info, const Source& source) {
    const auto queued = GetQueuedTriggers(ref_info.ref_id);
    if (queued.empty()) {
        return;
    }
    const auto world_ref = ref_info.GetRef();
    if (!world_ref) {
        return;
    }

    const float r = Settings::search_radius;
    WorldTriggerMatchContext context{
        .origin_id = ref_info.ref_id,
        .radius_squared = (r > 0.0f) ? (r * r) : std::numeric_limits<float>::infinity()
    };
    for (const auto& key : queued) {
        const auto& [source_id, base_id, trigger_id, kind] = key;
        bool valid = false;
        if (source.formid == source_id && source.IsHealthy()) {
            if (const auto instances = source.data.find(ref_info.ref_id); instances != source.data.end()) {
                for (const auto& instance : instances->second) {
                    if (instance.count <= 0 || instance.xtra.is_decayed || instance.xtra.form_id != base_id) {
                        continue;
                    }
                    if (const auto stage = source.triggers.find(instance.no); stage != source.triggers.end()) {
                        const auto& candidates = stage->second.ordered;
                        const auto candidate = std::ranges::find_if(candidates, [&](const Triggers::Candidate& entry) {
                            return entry.form->GetFormID() == trigger_id && entry.kind == kind;
                        });
                        valid = candidate != candidates.end() && MatchesWorldTrigger(world_ref, *candidate, context);
                    }
                    if (valid) {
                        break;
                    }
                }
            }
        }
        if (!valid) {
            RemoveTrigger(ref_info.ref_id, key);
        }
    }
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
                if (entry.ready_time) {
                    continue;
                }
                if (random_.generate() < -std::expm1(-elapsed / entry.activation_delay)) {
                    entry.ready_time = now;
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
            !std::ranges::any_of(it->second | std::views::values,
                                [](const Entry& entry) { return entry.ready_time.has_value(); })) {
            continue;
        }
        if (const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(refid)) {
            M->RequestRefUpdate(ref);
        }
    }
}
