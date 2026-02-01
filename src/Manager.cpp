#pragma once
#include "Manager.h"
#include <unordered_set>
#include "Data.h"
#include <shared_mutex>
#include <cassert>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include "CellScan.h"
#include "Hooks.h"
#include "Queue.h"

#ifndef NDEBUG
namespace {
    // Tags to distinguish per-mutex state
    struct SourceMutexTag {
        static constexpr auto name = "Manager::sourceMutex_";
    };

    struct QueueMutexTag {
        static constexpr auto name = "Manager::queueMutex_";
    };

    template <typename Tag>
    struct DebugLockState {
        static inline thread_local int sharedDepth = 0;
        static inline thread_local int uniqueDepth = 0;
    };

    // Debug metadata per mutex tag
    template <typename Tag>
    struct DebugMeta {
        static inline std::atomic<size_t> uniqueOwner{0}; // hash(tid) of current unique owner or 0
        static inline const char* uniqueFile = nullptr;
        static inline int uniqueLine = 0;
        static inline const char* uniqueFunc = nullptr;

        static inline std::mutex metaMutex; // guards sharedOwners and unique site fields
        static inline std::unordered_map<size_t, int> sharedOwners; // tid hash -> depth
    };

    size_t dbg_tid() {
        return std::hash<std::thread::id>{}(std::this_thread::get_id());
    }

    template <typename Tag>
    [[noreturn]] void ReportAndAbort(const char* reason) {
        logger::critical("[LockAssert] mutex={} reason={} thread_id={}", Tag::name, reason, dbg_tid());
        assert(false && "Lock invariant violation");
        std::abort();
    }

    template <typename Tag>
    void CheckLockOrder() {
        // Enforce single consistent order: source -> queue
        // Allow acquiring queue while already holding source.
        if constexpr (std::is_same_v<Tag, SourceMutexTag>) {
            // Disallow acquiring source while holding queue (deadlock risk)
            if (DebugLockState<QueueMutexTag>::sharedDepth > 0 || DebugLockState<QueueMutexTag>::uniqueDepth > 0) {
                ReportAndAbort<Tag>("Lock order violation: acquiring source while holding queue");
            }
        }
        // Acquiring queue while holding source is allowed by design.
    }

    template <typename Tag>
    struct DebugSharedLock {
        std::shared_mutex* m{};
        bool active{false};

        explicit DebugSharedLock(std::shared_mutex* mutex, const char* file = nullptr, int line = 0,
                                 const char* func = nullptr)
            : m(mutex) {
            if (!m) ReportAndAbort<Tag>("null mutex pointer");
            // Only assert real misuse
            if (DebugLockState<Tag>::uniqueDepth != 0)
                ReportAndAbort<Tag>(
                    "illegal upgrade: shared while holding unique");
            if (DebugLockState<Tag>::sharedDepth != 0) ReportAndAbort<Tag>("re-entrant shared acquisition");
            CheckLockOrder<Tag>();

            // Normal contention: block, do not assert/log
            if (!m->try_lock_shared()) {
                m->lock_shared();
            }

            // register this thread as shared owner (for optional diagnostics we already have)
            {
                std::scoped_lock g(DebugMeta<Tag>::metaMutex);
                DebugMeta<Tag>::sharedOwners[dbg_tid()] += 1;
            }

            active = true;
            DebugLockState<Tag>::sharedDepth = 1;
        }

        DebugSharedLock(const DebugSharedLock&) = delete;
        DebugSharedLock& operator=(const DebugSharedLock&) = delete;
        DebugSharedLock(DebugSharedLock&&) = delete;
        DebugSharedLock& operator=(DebugSharedLock&&) = delete;

        void unlock() {
            if (!active) ReportAndAbort<Tag>("shared unlock without ownership");
            if (DebugLockState<Tag>::sharedDepth <= 0) ReportAndAbort<Tag>("shared depth underflow (unlock)");

            // unregister shared holder
            {
                std::scoped_lock g(DebugMeta<Tag>::metaMutex);
                auto it = DebugMeta<Tag>::sharedOwners.find(dbg_tid());
                if (it != DebugMeta<Tag>::sharedOwners.end()) {
                    if (--it->second == 0) DebugMeta<Tag>::sharedOwners.erase(it);
                }
            }

            if (--DebugLockState<Tag>::sharedDepth == 0) {
                m->unlock_shared();
                active = false;
            }
        }

        ~DebugSharedLock() {
            if (!active) return;
            if (DebugLockState<Tag>::sharedDepth <= 0) ReportAndAbort<Tag>("shared depth underflow (dtor)");

            // unregister shared holder
            {
                std::scoped_lock g(DebugMeta<Tag>::metaMutex);
                auto it = DebugMeta<Tag>::sharedOwners.find(dbg_tid());
                if (it != DebugMeta<Tag>::sharedOwners.end()) {
                    if (--it->second == 0) DebugMeta<Tag>::sharedOwners.erase(it);
                }
            }

            if (--DebugLockState<Tag>::sharedDepth == 0) {
                m->unlock_shared();
                active = false;
            }
        }
    };

    template <typename Tag>
    struct DebugUniqueLock {
        std::shared_mutex* m{};
        bool owns{false};

        explicit DebugUniqueLock(std::shared_mutex* mutex, const char* file = nullptr, int line = 0,
                                 const char* func = nullptr)
            : m(mutex) {
            if (!m) ReportAndAbort<Tag>("null mutex pointer");
            // Only assert real misuse
            if (DebugLockState<Tag>::sharedDepth != 0)
                ReportAndAbort<Tag>(
                    "illegal upgrade: unique while holding shared");
            if (DebugLockState<Tag>::uniqueDepth != 0) ReportAndAbort<Tag>("re-entrant unique acquisition");
            CheckLockOrder<Tag>();

            // Normal contention: block, do not assert/log
            if (!m->try_lock()) {
                m->lock();
            }

            // mark unique owner and site (for diagnostics)
            {
                std::scoped_lock g(DebugMeta<Tag>::metaMutex);
                DebugMeta<Tag>::uniqueOwner.store(dbg_tid(), std::memory_order_relaxed);
                DebugMeta<Tag>::uniqueFile = file;
                DebugMeta<Tag>::uniqueLine = line;
                DebugMeta<Tag>::uniqueFunc = func;
            }

            owns = true;
            DebugLockState<Tag>::uniqueDepth = 1;
        }

        DebugUniqueLock(const DebugUniqueLock&) = delete;
        DebugUniqueLock& operator=(const DebugUniqueLock&) = delete;
        DebugUniqueLock(DebugUniqueLock&&) = delete;
        DebugUniqueLock& operator=(DebugUniqueLock&&) = delete;

        void unlock() {
            if (!owns) ReportAndAbort<Tag>("unique unlock without ownership");
            if (DebugLockState<Tag>::uniqueDepth != 1) ReportAndAbort<Tag>("unique depth corruption (unlock)");

            // clear unique owner
            {
                std::scoped_lock g(DebugMeta<Tag>::metaMutex);
                DebugMeta<Tag>::uniqueOwner.store(0, std::memory_order_relaxed);
                DebugMeta<Tag>::uniqueFile = nullptr;
                DebugMeta<Tag>::uniqueLine = 0;
                DebugMeta<Tag>::uniqueFunc = nullptr;
            }

            m->unlock();
            owns = false;
            DebugLockState<Tag>::uniqueDepth = 0;
        }

        ~DebugUniqueLock() {
            if (!owns) return;
            if (DebugLockState<Tag>::uniqueDepth != 1) ReportAndAbort<Tag>("unique depth corruption (dtor)");

            // clear unique owner
            {
                std::scoped_lock g(DebugMeta<Tag>::metaMutex);
                DebugMeta<Tag>::uniqueOwner.store(0, std::memory_order_relaxed);
                DebugMeta<Tag>::uniqueFile = nullptr;
                DebugMeta<Tag>::uniqueLine = 0;
                DebugMeta<Tag>::uniqueFunc = nullptr;
            }

            m->unlock();
            owns = false;
            DebugLockState<Tag>::uniqueDepth = 0;
        }
    };

    // Helpers to create unique variable names in macros
    #define AOT_CONCAT_INNER(a,b) a##b
    #define AOT_CONCAT(a,b) AOT_CONCAT_INNER(a,b)
}

// Debug macros (per-mutex) - pass source location for better logs
#define SRC_SHARED_GUARD  DebugSharedLock<SourceMutexTag> AOT_CONCAT(src_slock_, __COUNTER__)(&sourceMutex_, __FILE__, __LINE__, __func__)
#define SRC_UNIQUE_GUARD  DebugUniqueLock<SourceMutexTag> AOT_CONCAT(src_ulock_, __COUNTER__)(&sourceMutex_, __FILE__, __LINE__, __func__)
#define QUE_SHARED_GUARD  DebugSharedLock<QueueMutexTag>  AOT_CONCAT(que_slock_, __COUNTER__)(&queueMutex_,  __FILE__, __LINE__, __func__)
#define QUE_UNIQUE_GUARD  DebugUniqueLock<QueueMutexTag>  AOT_CONCAT(que_ulock_, __COUNTER__)(&queueMutex_,  __FILE__, __LINE__, __func__)

#else
// Release macros map to std locks with CTAD
#define AOT_CONCAT_INNER(a,b) a##b
#define AOT_CONCAT(a,b) AOT_CONCAT_INNER(a,b)
#define SRC_SHARED_GUARD  std::shared_lock  AOT_CONCAT(src_slock_, __COUNTER__){sourceMutex_}
#define SRC_UNIQUE_GUARD  std::unique_lock  AOT_CONCAT(src_ulock_, __COUNTER__){sourceMutex_}
#define QUE_SHARED_GUARD  std::shared_lock  AOT_CONCAT(que_slock_, __COUNTER__){queueMutex_}
#define QUE_UNIQUE_GUARD  std::unique_lock  AOT_CONCAT(que_ulock_, __COUNTER__){queueMutex_}
#endif

void Manager::AddLocationIndex(const RefID location_id, const FormID source_formid) {
    if (!location_id || !source_formid) {
        return;
    }
    loc_to_sources[location_id].insert(source_formid);
}

void Manager::RemoveLocationIndex(const RefID location_id, const FormID source_formid) {
    if (!location_id || !source_formid) {
        return;
    }
    const auto it = loc_to_sources.find(location_id);
    if (it == loc_to_sources.end()) {
        return;
    }
    it->second.erase(source_formid);
    if (it->second.empty()) {
        loc_to_sources.erase(it);
    }
}

void Manager::UpdateLocationIndexForSource(const Source& src, const RefID location_id) {
    if (!location_id) {
        return;
    }

    const auto it = src.data.find(location_id);
    if (it == src.data.end() || it->second.empty()) {
        RemoveLocationIndex(location_id, src.formid);
        return;
    }

    AddLocationIndex(location_id, src.formid);
}

void Manager::RefreshLocationIndex(const RefID location_id) {
    if (!location_id) {
        return;
    }

    loc_to_sources.erase(location_id);
    for (const auto& src : sources | std::views::values) {
        if (!src || !src->IsHealthy()) {
            continue;
        }
        UpdateLocationIndexForSource(*src, location_id);
    }
}

std::vector<Manager::ScanRequest> Manager::BuildCellScanRequests_(
    const std::vector<RefInfo>& refStopsCopy) {
    std::vector<ScanRequest> out;
    out.reserve(refStopsCopy.size());

    // Only touches plugin-owned data => guarded by sourceMutex_.
    SRC_SHARED_GUARD;

    for (const auto& ref_info : refStopsCopy) {
        const auto refid = ref_info.ref_id;
        if (!refid) {
            continue;
        }

        const auto src = GetSourceByLocation(refid);
        if (!src || !src->IsHealthy()) {
            continue;
        }

        const auto it = src->data.find(refid);
        if (it == src->data.end() || it->second.empty()) {
            continue;
        }

        const auto& inst = it->second.front();
        if (inst.count <= 0) {
            continue;
        }

        const StageNo no = inst.no;

        std::vector<FormID> bases;
        bases.reserve(src->settings.transformers_order.size() + src->settings.delayers_order.size());

        // Include all bases we want CellScanner to collect for this WO.
        for (const auto trns : src->settings.transformers_order) {
            if (src->settings.transformer_allowed_stages.at(trns).contains(no)) {
                bases.push_back(trns);
            }
        }
        for (const auto dlyr : src->settings.delayers_order) {
            if (src->settings.delayer_allowed_stages.at(dlyr).contains(no)) {
                bases.push_back(dlyr);
            }
        }

        if (!bases.empty()) {
            out.emplace_back(ref_info, std::move(bases));
        }
    }

    return out;
}

bool Manager::LocHasStage(Source* src, const RefID loc, const FormID stage_formid) {
    if (!src) return false;
    const auto it = src->data.find(loc);
    if (it == src->data.end()) return false;

    for (const auto& inst : it->second) {
        if (inst.count > 0 && inst.xtra.form_id == stage_formid) {
            return true;
        }
    }
    return false;
}

Source* Manager::UpdateGetSource(const FormID stage_formid, const RefID owner_refid) {
    if (!stage_formid) return nullptr;
    if (do_not_register.contains(stage_formid)) return nullptr;

    if (owner_refid) {
        // 1) If this owner belongs to exactly 1 source, just take it (no scan)
        const auto lit = loc_to_sources.find(owner_refid);
        if (lit != loc_to_sources.end() && lit->second.size() == 1) {
            const FormID src_formid = *lit->second.begin();
            if (const auto it = sources.find(src_formid); it != sources.end()) {
                const auto s = it->second.get();
                if (s && s->IsHealthy() && s->IsStage(stage_formid)) {
                    return s;
                }
            }
        }

        // 2) Otherwise fall back to your intersection logic,
        // and ONLY then use LocHasStage() as a tie-breaker.

        const auto sit = stage_to_sources.find(stage_formid);

        if (lit != loc_to_sources.end() && sit != stage_to_sources.end()) {
            // exact match: intersection candidates where this owner already has this stage
            for (const FormID src_formid : lit->second) {
                if (!sit->second.contains(src_formid)) continue;
                const auto srcIt = sources.find(src_formid);
                if (srcIt == sources.end()) continue;
                const auto src = srcIt->second.get();
                if (!src || !src->IsHealthy()) continue;
                if (LocHasStage(src, owner_refid, stage_formid)) return src;
            }

            // if none has it yet, pick any healthy source from the intersection
            for (const FormID src_formid : lit->second) {
                if (!sit->second.contains(src_formid)) continue;
                const auto srcIt = sources.find(src_formid);
                if (srcIt == sources.end()) continue;
                const auto src = srcIt->second.get();
                if (src && src->IsHealthy()) return src;
            }
        }
    }

    // Stage-only fallback
    return GetSource(stage_formid);
}

std::optional<float> Manager::GetNextUpdateTime(const RefInfo& a_info) {
    const auto refid = a_info.ref_id;
    const auto lit = loc_to_sources.find(refid);
    if (lit == loc_to_sources.end()) return std::nullopt;

    float best = std::numeric_limits<float>::infinity();
    bool found = false;

    for (FormID src_formid : lit->second) {
        const auto sit = sources.find(src_formid);
        if (sit == sources.end()) continue;

        auto& src = *sit->second;
        if (!src.IsHealthy()) continue;

        const auto dit = src.data.find(refid);
        if (dit == src.data.end()) continue;

        for (auto& inst : dit->second) {
            if (inst.xtra.is_decayed || !src.IsStageNo(inst.no)) continue;
            const float t = src.GetNextUpdateTime(&inst);
            if (t > 0.0f && t < best) {
                best = t;
                found = true;
            }
        }
    }
    if (!found) return std::nullopt;
    return best;
}

void Manager::UpdateImpl(RE::TESObjectREFR* from, RE::TESObjectREFR* to, const RE::TESForm* what, const Count count,
                         const RefID from_refid, const bool refreshRefs) {
    UpdateCtx ctx{from, to, what, count, from_refid, refreshRefs};

    NormalizeWorldObjectCount_(ctx);
    QueueDeleteIfFromIsWorldObject_(ctx);
    ApplyBarterMenuSemantics_(ctx);
    ApplyAlchemyNullSkip_(ctx);
    RecalcCtx_(ctx);

    if (!ctx.what || ctx.count <= 0) {
        RefreshRefs_(ctx);
        return;
    }

    Source* src;
    const auto loc =
        ctx.from ? ctx.from->GetFormID() : (ctx.from_refid ? ctx.from_refid : (ctx.to ? ctx.to->GetFormID() : 0));

    {
        SRC_SHARED_GUARD;
        src = UpdateGetSource(ctx.what->GetFormID(), loc);
    }
    if (!src) {
        return;
    }

    {
        SRC_UNIQUE_GUARD;
        if (src = UpdateGetSource(ctx.what->GetFormID(), loc); src) {
            ApplyTransferToSource_(*src, ctx, ctx.to && ctx.to->HasContainer() ? ctx.to->GetInventory() : InvMap{});
            SplitWorldObjectStackIfNeeded_(*src, ctx);
        }
    }

    RefreshRefs_(ctx);
}

void Manager::MarkDirty_(RE::TESObjectREFR* r) {
    if (!r) return;
    if (std::shared_lock lk(dirty_mtx_);
        dirty_refs_.contains(r->GetFormID())) {
        return;
    }
    const auto h = r->GetHandle();
    if (!h) return;
    std::unique_lock lk(dirty_mtx_);
    dirty_refs_[r->GetFormID()] = h;
}

void Manager::ProcessDirtyRefs_() {
    if (std::shared_lock lk(dirty_mtx_);
        dirty_refs_.empty()) {
        return;
    }
    std::unordered_map<RefID, RE::ObjectRefHandle> local;
    {
        std::unique_lock lk(dirty_mtx_);
        local.swap(dirty_refs_);
    }

    for (const auto& h : local | std::views::values) {
        if (const auto ref = h.get().get()) {
            SRC_UNIQUE_GUARD;
            UpdateRef(ref);
        }
    }
}

void Manager::InstanceCountUpdate(const int32_t delta) { n_instances_.fetch_add(delta, std::memory_order_relaxed); }


Manager::UpdateCtx::UpdateCtx(RE::TESObjectREFR* from, RE::TESObjectREFR* to, const RE::TESForm* what, const Count count,
                              const RefID from_refid, const bool refreshRefs):
    from(from), to(to), what(what), 
    count(count), from_refid(from_refid), 
    refreshRefs(refreshRefs) {
    

    to_is_world_object = to && !to->HasContainer();
    is_player_owned = from && from->IsPlayerRef();
    what_formid = what ? what->GetFormID() : 0;
    to_refid = to ? to->GetFormID() : 0;
    curr_time = RE::Calendar::GetSingleton()->GetHoursPassed();

    to_base_id = to ? to->GetBaseObject()->GetFormID() : 0;
}

void Manager::RecalcCtx_(UpdateCtx& c) {
    c.to_is_world_object = c.to && !c.to->HasContainer();
    c.is_player_owned = c.from && c.from->IsPlayerRef();
    c.what_formid = c.what ? c.what->GetFormID() : 0;
    c.to_refid = c.to ? c.to->GetFormID() : 0;
    c.to_base_id = c.to ? c.to->GetBaseObject()->GetFormID() : 0;
    c.from_refid = c.from ? c.from->GetFormID() : c.from_refid;
}

void Manager::NormalizeWorldObjectCount_(UpdateCtx& ctx) {
    if (ctx.to_is_world_object) {
        ctx.count = ctx.to->extraList.GetCount();
    }
}

void Manager::QueueDeleteIfFromIsWorldObject_(const UpdateCtx& ctx) {
    if (ctx.from && ctx.to && !ctx.from->HasContainer()) {
        const auto id = ctx.from->GetFormID();
        QUE_UNIQUE_GUARD;
        queue_delete_.insert(id);
    }
}

void Manager::ApplyBarterMenuSemantics_(UpdateCtx& ctx) {
    if (RE::UI::GetSingleton()->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
        if (ctx.from && ctx.from->IsPlayerRef())
            ctx.to = nullptr;
        else if (ctx.to && ctx.to->IsPlayerRef())
            ctx.from = nullptr;
    }
}

void Manager::ApplyAlchemyNullSkip_(UpdateCtx& ctx) {
    if (!ctx.to && ctx.what && ctx.what->Is(RE::FormType::AlchemyItem)) {
        ctx.count = 0;
    }
}

void Manager::ApplyTransferToSource_(Source& src, UpdateCtx& ctx, const InvMap& to_inv) {
    if (src.data.contains(ctx.from_refid)) {
        ctx.count = src.MoveInstances(ctx.from_refid, ctx.to_refid, ctx.what_formid, ctx.count, true);
    }

    if (ctx.count > 0) {
        Register(ctx.what_formid, ctx.count, {ctx.to_refid, ctx.to_base_id}, ctx.curr_time, to_inv);
    }

    CleanUpSourceData(&src, ctx.from_refid);
    if (ctx.to_refid > 0) {
        CleanUpSourceData(&src, ctx.to_refid);
    }

    if (!src.data.contains(ctx.from_refid)) {
        QUE_UNIQUE_GUARD;
        queue_delete_.insert(ctx.from_refid);
    }
}

void Manager::SplitWorldObjectStackIfNeeded_(Source& src, const UpdateCtx& ctx) {
    if (!ctx.to_is_world_object || !ctx.to_refid) return;

    const auto it = src.data.find(ctx.to_refid);
    if (it == src.data.end()) return;

    auto& v = it->second;
    if (v.empty()) return;

    // Ensure first stack matches v[0]
    {
        const auto c0 = v[0].count;
        if (ctx.to->extraList.GetCount() != c0) ctx.to->extraList.SetCount(static_cast<uint16_t>(c0));
        if (ctx.is_player_owned) ctx.to->extraList.SetOwner(RE::TESForm::LookupByID(0x07));
    }

    // Split remaining instances by repeatedly moving index 1
    while (v.size() > 1) {
        const auto inst_count = v[1].count;
        if (inst_count <= 0) {
            v.erase(v.begin() + 1);
            continue;
        }

        const auto new_ref = WorldObject::DropObjectIntoTheWorld(v[1].GetBound(), inst_count, ctx.is_player_owned);
        if (!new_ref) break;

        // Move by pointer to element at index 1 (safe: we won't keep it after the call)
        if (!src.MoveInstanceAt(ctx.to_refid, new_ref->GetFormID(), 1)) break;

        UpdateLocationIndexForSource(src, ctx.to_refid);
        UpdateLocationIndexForSource(src, new_ref->GetFormID());

        if (const auto a_ref = new_ref->GetHandle().get().get()) {
            UpdateRef(a_ref);
        }
    }
}

void Manager::RefreshRefs_(const UpdateCtx& ctx) {
    if (!ctx.refreshRefs) return;

    if (ctx.to) {
        if (const auto a_ref = ctx.to->GetHandle().get().get()) {
            MarkDirty_(a_ref);
        }
    }

    if (ctx.from && (ctx.from->HasContainer() || !ctx.to)) {
        if (const auto a_ref = ctx.from->GetHandle().get().get()) {
            MarkDirty_(a_ref);
        }
    }
}

void Manager::PreDeleteRefStop(RefStop& a_ref_stop) {
    a_ref_stop.RemoveTint();
    a_ref_stop.RemoveArtObject();
    a_ref_stop.RemoveShader();
    a_ref_stop.RemoveSound();
}

void Manager::UpdateLoop() {
    if (!Settings::world_objects_evolve.load()) {
        ClearWOUpdateQueue();
    } else if (QUE_UNIQUE_GUARD;
        !queue_delete_.empty() || !Settings::placed_objects_evolve.load()) {
        for (auto it = _ref_stops_.begin(); it != _ref_stops_.end();) {
            if (const auto ref = it->second.GetRef();
                queue_delete_.contains(it->first) ||
                ref && !Settings::placed_objects_evolve.load() && WorldObject::IsPlacedObject(ref)) {
                PreDeleteRefStop(it->second);
                it = _ref_stops_.erase(it);
            } else ++it;
        }
        queue_delete_.clear();
    }

    bool should_stop = false;
    {
        QUE_SHARED_GUARD;
        if (_ref_stops_.empty()) {
            should_stop = true;
        }
    }
    if (should_stop) {
        Stop();
        QUE_UNIQUE_GUARD;
        queue_delete_.clear();
        return;
    }

    if (const auto ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) return;

    const auto ref_stops_copy = GetRefStops();

    const auto scanReq = BuildCellScanRequests_(ref_stops_copy);
    CellScanner::GetSingleton()->RequestRefresh(scanReq);

    float curr_time = -1.0f;
    if (const auto cal = RE::Calendar::GetSingleton()) {
        // make copy with only stops
        curr_time = cal->GetHoursPassed();
        std::vector<RefID> ref_stops_due;
        ref_stops_due.reserve(ref_stops_copy.size());
        for (
            QUE_UNIQUE_GUARD;
            const auto& key : ref_stops_copy) {
            auto it = _ref_stops_.find(key.ref_id);
            if (it == _ref_stops_.end()) continue;

            if (auto& val = it->second; val.IsDue(curr_time)) {
                ref_stops_due.push_back(key.ref_id);
            } else if (const auto ref = val.GetRef()) {
                val.ApplyTint(ref);
                val.ApplyArtObject(ref);
                val.ApplyShader(ref);
                val.ApplySound();
            }
        }

        for (QUE_UNIQUE_GUARD; const auto refid : ref_stops_due) {
            if (auto it = _ref_stops_.find(refid); it != _ref_stops_.end()) {
                auto& val = it->second;
                PreDeleteRefStop(val);
                _ref_stops_.erase(it);
            }
        }
    }

    //SKSE::GetTaskInterface()->AddTask([ref_stops_copy = std::move(ref_stops_copy)]() mutable {
    if (curr_time > 0.f) {
        for (const auto& ref_info : ref_stops_copy) {
            M->UpdateQueuedWO(ref_info, curr_time);
        }
    }
    //});
}

void Manager::QueueWOUpdate(const RefStop& a_refstop) {
    if (!Settings::world_objects_evolve.load()) return;

    bool needStart;
    {
        const auto refid = a_refstop.ref_info.ref_id;
        QUE_UNIQUE_GUARD;
        if (auto [it, inserted] = _ref_stops_.try_emplace(refid, a_refstop); !inserted) {
            it->second.Update(a_refstop);
        }
        needStart = !isRunning();
    }

    if (needStart) Start();
}

void Manager::UpdateRefStop(const Source& src, const StageInstance& wo_inst, RefStop& a_ref_stop, const float stop_t) {
    const auto delayer = wo_inst.GetDelayerFormID();
    const bool is_transformer = src.settings.transformers.contains(delayer);
    const bool is_delayer = !is_transformer && src.settings.delayers.contains(delayer);

    const auto pick = [&](const std::unordered_map<FormID, uint32_t>& transformer_map,
                          const std::unordered_map<FormID, uint32_t>& delayer_map,
                          const std::map<StageNo, uint32_t>& stage_map) {
        if (is_transformer) {
            if (const auto it = transformer_map.find(delayer); it != transformer_map.end()) {
                return it->second;
            }
        }
        if (is_delayer) {
            if (const auto it = delayer_map.find(delayer); it != delayer_map.end()) {
                return it->second;
            }
        }
        if (const auto it = stage_map.find(wo_inst.no); it != stage_map.end()) {
            return it->second;
        }
        return static_cast<uint32_t>(0);
    };

    a_ref_stop.features.tint_color.id =
        pick(src.settings.transformer_colors, src.settings.delayer_colors, src.settings.colors);
    a_ref_stop.features.art_object.id =
        pick(src.settings.transformer_artobjects, src.settings.delayer_artobjects, src.settings.artobjects);
    a_ref_stop.features.effect_shader.id =
        pick(src.settings.transformer_effect_shaders, src.settings.delayer_effect_shaders, src.settings.effect_shaders);
    a_ref_stop.features.sound.id =
        pick(src.settings.transformer_sounds, src.settings.delayer_sounds, src.settings.sounds);

    a_ref_stop.stop_time = stop_t;
}


uint32_t Manager::GetNInstances() {
    uint32_t n = 0;
    for (const auto& src : sources | std::views::values) {
        const auto& source = *src;
        for (const auto& loc : source.data | std::views::keys) {
            n += static_cast<uint32_t>(source.data.at(loc).size());
        }
    }
    return n;
}

uint32_t Manager::GetNInstancesFast() const {
    const int32_t v = n_instances_.load(std::memory_order_relaxed);
    const auto result = v > 0 ? static_cast<uint32_t>(v) : 0;
#ifndef NDEBUG
    /*const uint32_t actual = M->GetNInstances();
    if (result != actual) {
        logger::critical("Instance count mismatch: cached={} actual={}", result, actual);
    }*/
#endif
    return result;
}

Source* Manager::MakeSource(const FormID source_formid, const DefaultSettings* settings) {
    if (!source_formid) return nullptr;
    if (IsDynamicFormID(source_formid)) return nullptr;
    // Source new_source(source_formid, "", empty_mgeff, settings);
    auto new_source = std::make_unique<Source>(source_formid, "", settings);
    if (!new_source->IsHealthy()) return nullptr;
    const auto source_id = new_source->formid;
    auto [it, inserted] = sources.try_emplace(source_id, std::move(new_source));
    if (inserted) {
        IndexSourceStages(*it->second);
    }
    return it->second.get();
}

void Manager::IndexStage(const FormID stage_formid, const FormID source_formid) {
    if (!stage_formid || !source_formid) {
        return;
    }

    stage_to_sources[stage_formid].insert(source_formid);
}

void Manager::IndexSourceStages(const Source& source) {
    stage_to_sources[source.formid].insert(source.formid);
    StageNo stage_no = 0;
    while (source.IsStageNo(stage_no)) {
        if (const auto stage = source.TryGetStage(stage_no)) {
            if (stage->formid) {
                stage_to_sources[stage->formid].insert(source.formid);
            }
        }
        ++stage_no;
    }
}

void Manager::CleanUpSourceData(Source* src) {
    if (!src) return;
    std::vector<RefID> previous_locations;
    previous_locations.reserve(src->data.size());
    for (const auto& loc : src->data | std::views::keys) {
        previous_locations.push_back(loc);
    }

    src->CleanUpData();

    for (const auto loc : previous_locations) {
        UpdateLocationIndexForSource(*src, loc);
    }
}

void Manager::CleanUpSourceData(Source* src, const RefID a_loc) {
    if (!src) return;
    src->CleanUpData(a_loc);
    UpdateLocationIndexForSource(*src, a_loc);
}

Source* Manager::GetSource(const FormID some_formid) {
    if (const auto it = sources.find(some_formid); it != sources.end()) {
        return it->second.get();
    }

    if (const auto it = stage_to_sources.find(some_formid); it != stage_to_sources.end() && !it->second.empty()) {
        const FormID chosen_source_formid = *it->second.begin();
        if (const auto sit = sources.find(chosen_source_formid); sit != sources.end()) {
            return sit->second.get();
        }
    }

    if (do_not_register.contains(some_formid)) return nullptr;

    return nullptr;
}

Source* Manager::GetSourceByLocation(const RefID location_id) {
    if (!location_id) {
        return nullptr;
    }

    if (const auto it = loc_to_sources.find(location_id); it != loc_to_sources.end()) {
        for (const auto src_formid : it->second) {
            const auto sit = sources.find(src_formid);
            if (sit == sources.end()) {
                continue;
            }
            const auto src = sit->second.get();
            if (!src || !src->IsHealthy()) {
                continue;
            }
            const auto dit = src->data.find(location_id);
            if (dit == src->data.end() || dit->second.empty()) {
                continue;
            }
            if (dit->second.size() == 1 && dit->second.front().count > 0) {
                return src;
            }
            if (std::ranges::any_of(dit->second, [](const StageInstance& inst) { return inst.count > 0; })) {
                return src;
            }
        }
    }

    return nullptr;
}

Source* Manager::ForceGetSource(const FormID some_formid) {
    if (!some_formid) return nullptr;
    if (const auto src = GetSource(some_formid)) return src;

    const auto some_form = FormReader::GetFormByID(some_formid);
    if (!some_form) {
        logger::warn("Form not found.");
        return nullptr;
    }

    const std::string_view qft = Settings::GetQFormType(some_form);
    if (qft.empty()) return nullptr;

    // exclude check once (no more FormReader lookups inside)
    if (Settings::IsInExclude(some_form, qft)) return nullptr;

    if (const auto custom = Settings::GetCustomSetting(some_form, qft)) {
        return MakeSource(some_formid, custom);
    }

    if (const auto def = Settings::GetDefaultSetting(qft)) {
        const bool hasAddon = (Settings::GetAddOnSettings(some_formid, qft) != nullptr);
        if (def->durations.at(0) < Settings::critical_stage_dur || hasAddon) {
            return MakeSource(some_formid, def);
        }
    }

    // stage item olarak dusunulduyse, custom a baslangic itemi olarak koymali
    return nullptr;
}


bool Manager::IsSource(const FormID some_formid) {
    if (!some_formid) return false;
    const auto some_form = FormReader::GetFormByID(some_formid);
    if (!some_form) {
        logger::warn("Form not found.");
        return false;
    }
    if (Settings::GetCustomSetting(some_form)) return true;
    if (Settings::GetDefaultSetting(some_formid)) return true;
    return false;
}

StageInstance* Manager::GetWOStageInstance(const RE::TESObjectREFR* wo_ref) {
    if (sources.empty()) return nullptr;
    const auto wo_refid = wo_ref->GetFormID();
    if (const auto src = GetSourceByLocation(wo_refid)) {
        auto& source = *src;
        if (!source.data.contains(wo_refid)) {
            logger::error("Stage instance not found.");
            return nullptr;
        }
        auto& instances = source.data.at(wo_refid);
        if (instances.size() == 1)
            return instances.data();
        if (instances.empty()) {
            logger::error("Stage instance found but empty.");
        } else if (instances.size() > 1) {
            logger::error("Multiple stage instances found.");
        }
    }
    return nullptr;
}

inline void Manager::ApplyStageInWorld_Fake(RE::TESObjectREFR* wo_ref, const char* xname) {
    if (!xname) {
        logger::error("ExtraTextDisplayData is null.");
        return;
    }
    wo_ref->extraList.RemoveByType(RE::ExtraDataType::kTextDisplayData);
    const auto xText = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
    xText->SetName(xname);
    wo_ref->extraList.Add(xText);
}

void Manager::ApplyStageInWorld(RE::TESObjectREFR* wo_ref, const Stage& stage, RE::TESBoundObject* source_bound) {
    if (!source_bound) {
        WorldObject::SwapObjects(wo_ref, stage.GetBound());
        wo_ref->extraList.RemoveByType(RE::ExtraDataType::kTextDisplayData);
    } else {
        WorldObject::SwapObjects(wo_ref, source_bound);
        ApplyStageInWorld_Fake(wo_ref, stage.GetExtraText());
    }
}

bool Manager::ApplyEvolutionInInventory(const RefInfo& a_info,
                                        Count update_count, const FormID old_item, const FormID new_item) {
    if (!old_item || !new_item) {
        logger::error("Item is null.");
        return false;
    }
    if (update_count <= 0) {
        logger::error("Update count is 0 or less {}.", update_count);
        return false;
    }
    if (old_item == new_item) {
        return false;
    }

    const auto refid = a_info.ref_id;
    QueueManager::GetSingleton()->QueueAddRemoveItemTask(
        AddItemTask{refid, 0, new_item, update_count},
        RemoveItemTask{refid, old_item, update_count});

    return true;
}


void Manager::RemoveItem(const RefInfo& moveFromInfo, const FormID item_id, const Count count) {
    if (!moveFromInfo.ref_id) {
        logger::warn("RemoveItem: moveFrom is null.");
        return;
    }
    if (count <= 0) {
        logger::warn("RemoveItem: Count is 0 or less.");
        return;
    }
    if (!item_id) {
        logger::warn("RemoveItem: item_id is null.");
        return;
    }
    QueueManager::GetSingleton()->QueueAddRemoveItemTask({}, RemoveItemTask{moveFromInfo.ref_id, item_id, count});
}

void Manager::AddItem(const RefInfo& addToInfo, const RefInfo& addFromInfo, const FormID item_id,
                      const Count count) {
    if (count <= 0) {
        logger::error("Count is 0 or less.");
        return;
    }
    const auto to_id = addToInfo.ref_id;
    if (!to_id) {
        logger::error("AddItem: to_id is null.");
        return;
    }
    const auto from_id = addFromInfo.ref_id;
    QueueManager::GetSingleton()->QueueAddRemoveItemTask(AddItemTask{to_id, from_id, item_id, count}, {});
}


void Manager::Init() {
    if (Settings::INI_settings.contains("Other Settings")) {
        if (Settings::INI_settings["Other Settings"].contains("bReset")) {
            should_reset = Settings::INI_settings["Other Settings"]["bReset"];
        } else logger::warn("bReset not found.");
    } else logger::critical("Other Settings not found.");

    _instance_limit = Settings::nMaxInstances;

    logger::info("Manager initialized with instance limit {}", _instance_limit);
}

std::set<float> Manager::GetUpdateTimes(const RE::TESObjectREFR* inventory_owner) {
    std::set<float> queued_updates;

    const auto inventory_owner_refid = inventory_owner->GetFormID();

    const auto lit = loc_to_sources.find(inventory_owner_refid);
    if (lit == loc_to_sources.end()) {
        return queued_updates;
    }

    for (const auto src_formid : lit->second) {
        const auto sit = sources.find(src_formid);
        if (sit == sources.end()) continue;

        auto& source = *sit->second;
        if (!source.IsHealthy()) {
            logger::error("_UpdateTimeModulators: Source is not healthy.");
            continue;
        }

        if (!source.data.contains(inventory_owner_refid)) continue;

        for (auto& st_inst : source.data.at(inventory_owner_refid)) {
            if (st_inst.xtra.is_decayed || !source.IsStageNo(st_inst.no)) continue;
            if (const auto hitting_time = source.GetNextUpdateTime(&st_inst); hitting_time > 0) {
                queued_updates.insert(hitting_time);
            }
        }
    }

    return queued_updates;
}

bool Manager::UpdateInventory(const RefInfo& a_info, const float t, const InvMap& inv) {
    bool update_took_place = false;
    const auto refid = a_info.ref_id;

    std::vector<FormID> candidate_sources;
    const auto it0 = loc_to_sources.find(refid);
    if (it0 == loc_to_sources.end()) {
        return false;
    }
    candidate_sources.reserve(it0->second.size());
    candidate_sources.insert(candidate_sources.end(), it0->second.begin(), it0->second.end());

    for (const auto src_formid : candidate_sources) {
        const auto sit = sources.find(src_formid);
        if (sit == sources.end()) {
            RemoveLocationIndex(refid, src_formid);
            continue;
        }

        auto& source = *sit->second;
        if (!source.IsHealthy()) {
            continue;
        }

        const auto dit = source.data.find(refid);
        if (dit == source.data.end() || dit->second.empty()) {
            UpdateLocationIndexForSource(source, refid);
            continue;
        }

        const auto& updates = source.UpdateAllStages(refid, t);
        if (!updates.empty()) {
            update_took_place = true;
        }

        CleanUpSourceData(&source, refid);

        for (const auto& update : updates) {
            if (ApplyEvolutionInInventory(a_info, update.count, update.oldstage->formid, update.newstage->formid) &&
                source.IsDecayedItem(update.newstage->formid)) {
                Register(update.newstage->formid, update.count, a_info, t, inv);
            }
        }
    }

    // Time modulation: only re-snapshot if loc_to_sources[refid] changed (size or membership)
    const auto it1 = loc_to_sources.find(refid);
    if (it1 == loc_to_sources.end()) {
        return update_took_place;
    }

    const auto& curr = it1->second;

    bool unchanged = (curr.size() == candidate_sources.size());
    if (unchanged) {
        for (const auto sid : candidate_sources) {
            if (!curr.contains(sid)) {
                unchanged = false;
                break;
            }
        }
    }

    if (unchanged) {
        for (const auto src_formid : candidate_sources) {
            const auto sit = sources.find(src_formid);
            if (sit == sources.end()) {
                RemoveLocationIndex(refid, src_formid);
                continue;
            }
            sit->second->UpdateTimeModulationInInventory(a_info, t, inv);
        }
    } else {
        std::vector<FormID> mod_sources;
        mod_sources.reserve(curr.size());
        mod_sources.insert(mod_sources.end(), curr.begin(), curr.end());

        for (const auto src_formid : mod_sources) {
            const auto sit = sources.find(src_formid);
            if (sit == sources.end()) {
                RemoveLocationIndex(refid, src_formid);
                continue;
            }
            sit->second->UpdateTimeModulationInInventory(a_info, t, inv);
        }
    }

    return update_took_place;
}

void Manager::UpdateInventory(const RefInfo& a_info, const InvMap& inv) {

    SyncWithInventory(a_info, inv);

    const auto curr = RE::Calendar::GetSingleton()->GetHoursPassed();
    for (;;) {
        auto next = GetNextUpdateTime(a_info);
        if (!next) break;
        const float t = *next + 0.000028f;
        if (t >= curr) break;
        if (!UpdateInventory(a_info, t, inv)) break;
    }

    UpdateInventory(a_info, curr, inv);
}

void Manager::SyncWithInventory(const RefInfo& a_info, const InvMap& inv) {
    const RefID loc = a_info.ref_id;
    const bool needHandling = locs_to_be_handled.contains(loc);
    const float now = RE::Calendar::GetSingleton()->GetHoursPassed();

    // Inventory formids for O(1) membership checks
    std::unordered_set<FormID> inv_fids;
    inv_fids.reserve(inv.size());
    for (const auto& bound : inv | std::views::keys) {
        inv_fids.insert(bound->GetFormID());
    }

    // Registry totals per stage formid, and whether ANY fake exists for that formid at this loc
    std::unordered_map<FormID, Count> reg_total;
    std::unordered_map<FormID, bool> reg_has_fake;
    reg_total.reserve(inv.size());

    if (auto lit = loc_to_sources.find(loc); lit != loc_to_sources.end()) {
        for (FormID sid : lit->second) {
            auto sit = sources.find(sid);
            if (sit == sources.end()) continue;
            auto& src = *sit->second;

            auto dit = src.data.find(loc);
            if (dit == src.data.end()) continue;

            for (auto& inst : dit->second) {
                if (inst.xtra.is_decayed || inst.count <= 0) continue;
                reg_total[inst.xtra.form_id] += inst.count;
                if (inst.xtra.is_fake) reg_has_fake[inst.xtra.form_id] = true;
            }
        }
    }

    if (needHandling) {
        for (auto& [bound, entry] : inv) {
            if (bound->IsDynamicForm()) {
                const auto name = bound->GetName();
                if (!name || std::strlen(name) == 0) {
                    QueueManager::GetSingleton()->QueueAddRemoveItemTask(
                        {}, RemoveItemTask(loc, bound->GetFormID(), std::max(1, entry.first)));
                }
            }
        }
    }

    // how much we need to remove from registry for each formid (when we are NOT "adding back" due to fake)
    std::unordered_map<FormID, Count> remove_from_reg;
    remove_from_reg.reserve(inv.size());

    // reconcile inventory -> registry
    for (auto& [bound, entry] : inv) {
        const FormID fid = bound->GetFormID();
        const Count invCount = entry.first;

        auto rt = reg_total.find(fid);
        if (rt == reg_total.end()) {
            if (invCount > 0) Register(fid, invCount, a_info, now, inv);
            continue;
        }

        const Count regCount = rt->second;
        if (regCount < invCount) {
            Register(fid, invCount - regCount, a_info, now, inv);
        } else if (regCount > invCount) {
            const Count diff = regCount - invCount;
            if (needHandling && reg_has_fake[fid]) {
                AddItem(a_info, {0, 0}, fid, diff);  // fix inventory, keep registry
            } else {
                remove_from_reg[fid] = diff;  // later decrement instances in one pass
            }
        }
    }

    // single pass over instances to apply:
    //  - leftover registry entries not in inventory
    //  - remove_from_reg decrements
    if (auto lit = loc_to_sources.find(loc); lit != loc_to_sources.end()) {
        for (FormID sid : lit->second) {
            auto sit = sources.find(sid);
            if (sit == sources.end()) continue;
            auto& src = *sit->second;

            auto dit = src.data.find(loc);
            if (dit == src.data.end()) continue;

            for (auto& inst : dit->second) {
                if (inst.xtra.is_decayed || inst.count <= 0) continue;

                const FormID fid = inst.xtra.form_id;

                if (!inv_fids.contains(fid)) {
                    // registry item not present in inventory
                    if (needHandling && inst.xtra.is_fake)
                        AddItem(a_info, {0, 0}, fid, inst.count);
                    else
                        inst.count = 0;
                    continue;
                }

                if (auto it = remove_from_reg.find(fid); it != remove_from_reg.end() && it->second > 0) {
                    const Count take = std::min<Count>(inst.count, it->second);
                    inst.count -= take;
                    it->second -= take;
                    if (it->second == 0) remove_from_reg.erase(it);
                }
            }
        }
    }

    locs_to_be_handled.erase(loc);
}


void Manager::UpdateQueuedWO(const RefInfo& ref_info, const float curr_time) {
    // Called from UpdateLoop task.

    const auto refid = ref_info.ref_id;

    SRC_UNIQUE_GUARD;

    RE::TESObjectREFR* ref = ref_info.GetRef();
    if (!ref) {
        QUE_UNIQUE_GUARD;
        queue_delete_.insert(refid);
        return;
    }

    HandleDynamicWO(ref);

    if (!RefIsUpdatable(ref)) {
        DeRegisterRef(refid);
        QUE_UNIQUE_GUARD;
        queue_delete_.insert(refid);
        return;
    }

    const auto base = ref->GetObjectReference();
    const FormID base_id = base ? base->GetFormID() : 0;

    const auto count = ref->extraList.GetCount();

    Source* source = GetSourceByLocation(refid);

    if (!source) {
        if (GetSource(base_id)) {
            Register(base_id, count, refid, curr_time);
            return;
        }

        // Not a stage item => deregister/delete
        DeRegisterRef(refid);
        QUE_UNIQUE_GUARD;
        queue_delete_.insert(refid);
        return;
    }

    // Handle base change the same way UpdateWO does:
    HandleWOBaseChange(ref);

    // Re-fetch after HandleWOBaseChange because it may have zeroed the instance
    {
        if (const auto it = source->data.find(refid);
            it == source->data.end() || it->second.empty() || it->second.front().count <= 0) {
            if (it == source->data.end() || it->second.empty()) {
                UpdateLocationIndexForSource(*source, refid);
            }
            QUE_UNIQUE_GUARD;
            queue_delete_.insert(refid);
            return;
        }
    }

    if (const auto updated_stages = source->UpdateAllStages(refid, curr_time);
        !updated_stages.empty()) {
        if (updated_stages.size() > 1) {
            logger::error("UpdateQueuedWO: Multiple updates for the same ref.");
        }
        const auto& update = updated_stages.front();
        const auto src_bound = source->IsFakeStage(update.newstage->no) ? source->GetBoundObject() : nullptr;
        ApplyStageInWorld(ref, *update.newstage, src_bound);
        if (source->IsDecayedItem(update.newstage->formid)) {
            Register(update.newstage->formid, update.count, refid, update.update_time);
        }
    }

    const auto it = source->data.find(refid);
    if (it == source->data.end() || it->second.empty()) {
        Register(ref->GetBaseObject()->GetFormID(), ref->extraList.GetCount(), refid, curr_time);
        return;
    }

    auto& wo_inst = it->second.front();
    if (wo_inst.count <= 0) {
        source->data.erase(it);
        UpdateLocationIndexForSource(*source, refid);
        Register(ref->GetBaseObject()->GetFormID(), ref->extraList.GetCount(), refid, curr_time);
        return;
    }

    if (wo_inst.xtra.is_fake) {
        ApplyStageInWorld(ref, source->GetStage(wo_inst.no), source->GetBoundObject());
    }

    source->UpdateTimeModulationInWorld(ref, wo_inst, curr_time);
    if (const auto next_update = source->GetNextUpdateTime(&wo_inst); next_update > curr_time) {
        RefStop a_ref_stop(refid);
        UpdateRefStop(*source, wo_inst, a_ref_stop, next_update);
        QueueWOUpdate(a_ref_stop);
    }

    CleanUpSourceData(source, refid);
}

void Manager::UpdateWO(RE::TESObjectREFR* ref) {
    HandleDynamicWO(ref);

    const RefID refid = ref->GetFormID();
    if (!RefIsUpdatable(ref)) {
        DeRegisterRef(refid);
        QUE_UNIQUE_GUARD;
        queue_delete_.insert(refid);
        return;
    }

    {
        QUE_SHARED_GUARD;
        if (_ref_stops_.contains(refid)) {
            return;
        }
    }

    const auto curr_time = RE::Calendar::GetSingleton()->GetHoursPassed();

    Source* source = nullptr;

    for (auto& src : sources | std::views::values) {
        auto& a_source = *src;
        if (!a_source.IsHealthy()) continue;
        auto it = a_source.data.find(refid);
        if (it == a_source.data.end() || it->second.empty()) continue;
        HandleWOBaseChange(ref);
        if (it->second.front().count <= 0) {
            QUE_UNIQUE_GUARD;
            queue_delete_.insert(refid);
            continue;
        }
        source = &a_source;
        break;
    }

    if (!source) {
        Register(ref->GetBaseObject()->GetFormID(), ref->extraList.GetCount(), refid, curr_time);
        return;
    }

    if (const auto updated_stages = source->UpdateAllStages(refid, curr_time);
        !updated_stages.empty()) {
        if (updated_stages.size() > 1) {
            logger::error("UpdateWO: Multiple updates for the same ref.");
        }
        const auto& update = updated_stages.front();
        const auto bound = source->IsFakeStage(update.newstage->no) ? source->GetBoundObject() : nullptr;
        ApplyStageInWorld(ref, *update.newstage, bound);
        if (source->IsDecayedItem(update.newstage->formid)) {
            Register(update.newstage->formid, update.count, refid, update.update_time);
        }
    }

    const auto it = source->data.find(refid);
    if (it == source->data.end() || it->second.empty()) {
        logger::error("UpdateWO: RefID {:x} not found in source data.", refid);
        Register(ref->GetBaseObject()->GetFormID(), ref->extraList.GetCount(), refid, curr_time);
        return;
    }
    auto& wo_inst = it->second.front();
    if (wo_inst.xtra.is_fake) ApplyStageInWorld(ref, source->GetStage(wo_inst.no), source->GetBoundObject());
    source->UpdateTimeModulationInWorld(ref, wo_inst, curr_time);
    if (const auto next_update = source->GetNextUpdateTime(&wo_inst); next_update > curr_time) {
        RefStop a_ref_stop(refid);
        UpdateRefStop(*source, wo_inst, a_ref_stop, next_update);
        QueueWOUpdate(a_ref_stop);
    }

    CleanUpSourceData(source, refid);
}

void Manager::UpdateRef(RE::TESObjectREFR* loc) {
    if (loc->HasContainer()) {
        const auto base = loc->GetBaseObject();
        const RefInfo info(loc->GetFormID(), base ? base->GetFormID() : 0);
        const auto inv = loc->GetInventory();
        UpdateInventory(info, inv);
    } else {
        UpdateWO(loc);
    }
}

RefStop* Manager::GetRefStop(const RefID refid) {
    const auto it = _ref_stops_.find(refid);
    return it == _ref_stops_.end() ? nullptr : &it->second;
}

bool Manager::RefIsUpdatable(const RE::TESObjectREFR* ref) {
    if (!Settings::world_objects_evolve.load()) return false;
    if (ref->IsDeleted() || ref->IsDisabled() || ref->IsMarkedForDeletion()) return false;
    if (ref->IsActivationBlocked()) return false;
    if (!Settings::unowned_objects_evolve.load() && RE::PlayerCharacter::GetSingleton()->WouldBeStealing(ref)) {
        return false;
    }
    if (!ref->GetObjectReference()) return false;
    return true;
}

bool Manager::DeRegisterRef(const RefID refid) {
    bool found = false;
    for (auto& src : sources | std::views::values) {
        auto& source = *src;
        if (auto it = source.data.find(refid); it != source.data.end()) {
            M->InstanceCountUpdate(-static_cast<int>(it->second.size()));
            source.data.erase(it);
            RemoveLocationIndex(refid, source.formid);
            found = true;
        }
    }
    return found;
}

void Manager::ClearWOUpdateQueue() {
    QUE_UNIQUE_GUARD;
    for (auto& val : _ref_stops_ | std::views::values) {
        PreDeleteRefStop(val);
    }
    _ref_stops_.clear();
}

void Manager::Register(const FormID some_formid, const Count count, const RefID location_refid,
                       const Duration register_time) {
    if (do_not_register.contains(some_formid)) {
        return;
    }
    if (!some_formid) {
        logger::warn("FormID is null.");
        return;
    }
    if (!count) {
        logger::warn("Count is 0.");
        return;
    }
    if (!location_refid) {
        logger::warn("Location RefID is null.");
        return;
    }
    const auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(location_refid);
    if (!ref) {
        logger::warn("Location ref is null. FormID: {:x}", some_formid);
        return;
    }
    if (!Settings::IsItem(some_formid, "", true)) {
        return;
    }

    if (GetNInstancesFast() > _instance_limit) {
        logger::warn("Instance limit reached.");
        MsgBoxesNotifs::InGame::CustomMsg(
            std::format("The mod is tracking over {} instances. It is advised to check your memory usage and "
                        "skse co-save sizes.",
                        _instance_limit));
    }

    // make new registry
    Source* const src = ForceGetSource(some_formid);  // also saves it to sources if it was created new
    if (!src) {
        do_not_register.insert(some_formid);
        return;
    }
    if (!src->IsStage(some_formid)) {
        logger::critical("Register: some_formid is not a stage.");
        do_not_register.insert(some_formid);
        return;
    }

    const auto stage_no = src->formid == some_formid ? 0 : src->GetStageNo(some_formid);

    if (const auto inserted_instance = src->InitInsertInstanceWO(stage_no, count, location_refid, register_time);
        !inserted_instance) {
        logger::error("Register: InsertNewInstance failed 2.");
    } else {
        const auto bound = src->IsFakeStage(stage_no) ? src->GetBoundObject() : nullptr;
        ApplyStageInWorld(ref, src->GetStage(stage_no), bound);
        // add to the queue
        const auto hitting_time = src->GetNextUpdateTime(inserted_instance);
        RefStop a_ref_stop(location_refid);
        UpdateRefStop(*src, *inserted_instance, a_ref_stop, hitting_time);
        QueueWOUpdate(a_ref_stop);
        UpdateLocationIndexForSource(*src, location_refid);
    }
}

void Manager::Register(const FormID some_formid, const Count count, const RefInfo& ref_info, const Duration register_time, const InvMap& a_inv) {
    if (do_not_register.contains(some_formid)) {
        return;
    }
    if (!some_formid) {
        logger::warn("FormID is null.");
        return;
    }
    if (!count) {
        logger::warn("Count is 0.");
        return;
    }
    const auto location_refid = ref_info.ref_id;
    if (!location_refid) {
        return;
    }
    if (Inventory::IsQuestItem(some_formid, a_inv)) {
        return;
    }
    if (!Settings::IsItem(some_formid, "", true)) {
        return;
    }

    if (GetNInstancesFast() > _instance_limit) {
        logger::warn("Instance limit reached.");
        MsgBoxesNotifs::InGame::CustomMsg(
            std::format("The mod is tracking over {} instances. It is advised to check your memory usage and "
                        "skse co-save sizes.",
                        _instance_limit));
    }

    // make new registry
    Source* const src = ForceGetSource(some_formid);  // also saves it to sources if it was created new
    if (!src) {
        do_not_register.insert(some_formid);
        return;
    }
    if (!src->IsStage(some_formid)) {
        logger::critical("Register: some_formid is not a stage.");
        do_not_register.insert(some_formid);
        return;
    }

    const auto stage_no = src->formid == some_formid ? 0 : src->GetStageNo(some_formid);

    if (!src->InitInsertInstanceInventory(stage_no, count, ref_info, register_time, a_inv)) {
        logger::error("Register: InsertNewInstance failed 1.");
    } else {
        UpdateLocationIndexForSource(*src, location_refid);
    }
}

void Manager::HandleCraftingEnter(const unsigned int bench_type) {
    if (!handle_crafting_instances.empty()) {
        logger::warn("HandleCraftingEnter: Crafting instances already exist.");
        return;
    }

    if (!Settings::qform_bench_map.contains(bench_type)) {
        logger::warn("HandleCraftingEnter: Bench type not found.");
        return;
    }

    UpdateRef(player_ref);
    ListenGuard lg(Hooks::listen_disable_depth);

    const auto& q_form_types = Settings::qform_bench_map.at(bench_type);

    // trusting that the player will leave the crafting menu at some point and everything will be reverted

    const auto player_inventory = player_ref->GetInventory();

    for (SRC_SHARED_GUARD; auto& a_source : sources | std::views::values) {
        auto& src = *a_source;
        if (!src.IsHealthy()) continue;
        if (!src.data.contains(player_refid)) continue;

        if (!std::ranges::contains(q_form_types, src.qFormType)) {
            continue;
        }

        for (const auto& st_inst : src.data.at(player_refid)) {
            const auto stage_formid = st_inst.xtra.form_id;
            if (!stage_formid) {
                logger::error("HandleCraftingEnter: Stage FormID is null!!!");
                continue;
            }

            if (st_inst.count <= 0 || st_inst.xtra.is_decayed) continue;
            if (Inventory::IsQuestItem(stage_formid, player_inventory)) continue;
            if (stage_formid != src.formid && !st_inst.xtra.crafting_allowed) continue;

            if (const Types::FormFormID temp = {src.formid, stage_formid}; !handle_crafting_instances.contains(temp)) {
                const auto it = player_inventory.find(src.GetBoundObject());
                const auto count_src = it != player_inventory.end() ? it->second.first : 0;
                handle_crafting_instances[temp] = {st_inst.count, count_src};
            } else {
                handle_crafting_instances.at(temp).first += st_inst.count;
            }

            const auto stage_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(stage_formid);
            if (!stage_bound) {
                logger::error("HandleCraftingEnter: Stage bound object not found for FormID {:x}", stage_formid);
                continue;
            }
            if (auto it = faves_list.find(stage_formid); it == faves_list.end()) {
                faves_list[stage_formid] = IsFavorited(stage_bound, player_inventory);
            } else if (!it->second) {
                it->second = IsFavorited(stage_bound, player_inventory);
            }

            if (auto it = equipped_list.find(stage_formid); it == equipped_list.end()) {
                equipped_list[stage_formid] = IsEquipped(stage_bound, player_inventory);
            } else if (!it->second) {
                it->second = IsEquipped(stage_bound, player_inventory);
            }
        }
    }

    for (const auto& [formids, counts] : handle_crafting_instances) {
        const auto& [src_formid, st_formid] = formids;
        const auto& [count_st, count_src] = counts;
        if (src_formid == st_formid) continue;
        const auto st_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(st_formid);
        player_ref->RemoveItem(st_bound, count_st, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        const auto source_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(src_formid);
        player_ref->AddObjectToContainer(source_bound, nullptr, count_st, nullptr);
    }
}

void Manager::HandleCraftingExit() {
    if (handle_crafting_instances.empty()) {
        faves_list.clear();
        equipped_list.clear();
        return;
    }

    {
        ListenGuard lg(Hooks::listen_disable_depth);

        // need to figure out how many items were used up in crafting and how many were left
        const auto player_inventory = player_ref->GetInventory();
        std::unordered_map<FormID, Count> actual_counts;
        for (auto& [formids, counts] : handle_crafting_instances) {
            const auto& [src_formid, st_formid] = formids;
            const auto& [st_count, src_count] = counts;
            if (src_formid == st_formid) continue;

            Count actual_count_src;
            {
                if (const auto it = actual_counts.find(src_formid); it != actual_counts.end()) {
                    actual_count_src = it->second;
                } else {
                    const auto it_inv = player_inventory.find(FormReader::GetFormByID<RE::TESBoundObject>(src_formid));
                    actual_count_src = it_inv != player_inventory.end() ? it_inv->second.first : 0;
                    actual_counts[src_formid] = actual_count_src;
                }
            }

            if (const auto revert = std::min(st_count, actual_count_src); revert > 0) {
                const auto src_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(src_formid);
                if (!src_bound) {
                    logger::error("HandleCraftingExit: Source bound object not found for FormID {:x}", src_formid);
                    continue;
                }
                player_ref->RemoveItem(src_bound, revert, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                const auto st_bound = RE::TESForm::LookupByID<RE::TESBoundObject>(st_formid);
                if (!st_bound) {
                    logger::error("HandleCraftingExit: Stage bound object not found for FormID {:x}", st_formid);
                    continue;
                }
                player_ref->AddObjectToContainer(st_bound, nullptr, revert, nullptr);
                if (auto it = faves_list.find(st_formid); it != faves_list.end() && it->second) {
                    FavoriteItem(st_bound, player_ref);
                }
                if (auto it = equipped_list.find(st_formid); it != equipped_list.end() && it->second) {
                    EquipItem(st_bound, false);
                }
                actual_counts.at(src_formid) -= revert;
            }
        }
    }

    handle_crafting_instances.clear();
    faves_list.clear();
    equipped_list.clear();

    Update(player_ref);
}

void Manager::Update(RE::TESObjectREFR* from, RE::TESObjectREFR* to, const RE::TESForm* what, const Count count,
                     const RefID from_refid) {
    UpdateImpl(from, to, what, count, from_refid, true);
}

void Manager::SwapWithStage(RE::TESObjectREFR* wo_ref) {
    if (!wo_ref) {
        logger::critical("Ref is null.");
        return;
    }

    RE::TESBoundObject* toSwap;
    {
        SRC_SHARED_GUARD;
        if (const auto st_inst = GetWOStageInstance(wo_ref)) {
            toSwap = st_inst->GetBound();
        } else {
            return;
        }
    }

    WorldObject::SwapObjects(wo_ref, toSwap, false);
}

void Manager::Reset() {
    logger::info("Resetting manager...");
    Stop();
    ClearWOUpdateQueue();
    {
        SRC_UNIQUE_GUARD;
        for (const auto& src : sources | std::views::values) src->Reset();
        sources.clear();
        stage_to_sources.clear();
        loc_to_sources.clear();
    }

    // external_favs.clear();         // we will update this in ReceiveData
    handle_crafting_instances.clear();
    faves_list.clear();
    equipped_list.clear();
    locs_to_be_handled.clear();
    Clear();
    isUninstalled.store(false);

    n_instances_.store(0, std::memory_order_relaxed);

    logger::info("Manager reset.");
}

bool Manager::HandleFormDelete(const FormID a_refid) {
    SRC_UNIQUE_GUARD;
    return DeRegisterRef(a_refid);
}

void Manager::SendData() {
    logger::info("--------Sending data---------");
    Print();
    Clear();

    if (QueueManager::GetSingleton()->HasPendingMoveItemTasks()) {
        logger::critical("SendData: There are pending move item tasks!");
    }

    for (SRC_UNIQUE_GUARD; auto& src : sources | std::views::values) {
        CleanUpSourceData(src.get());
    }

    const auto player_inv = player_ref->GetInventory();
    int n_instances = 0;
    SRC_SHARED_GUARD;
    for (const auto& src : sources | std::views::values) {
        const auto& source = *src;
        if (source.GetStageDuration(0) >= 10000.f) {
            if (source.settings.transformers_order.size() == 0 && source.settings.delayers_order.size() == 0) {
                continue;
            }
        }
        for (const auto& [loc, instances] : source.data) {
            if (instances.empty()) continue;
            const SaveDataLHS lhs{{source.formid, source.editorid}, loc};
            SaveDataRHS rhs;
            for (const auto& st_inst : instances) {
                auto plain = st_inst.GetPlain();
                if (plain.is_fake && loc == 20) {
                    plain.is_faved = IsFavorited(st_inst.GetBound(), player_inv);
                    plain.is_equipped = IsEquipped(st_inst.GetBound(), player_inv);
                }
                rhs.push_back(plain);
                n_instances++;
            }
            if (!rhs.empty()) SetData(lhs, rhs);
        }
    }
    logger::info("Data sent. Number of instances: {}", n_instances);
}

void Manager::HandleLoc(RE::TESObjectREFR* loc_ref) {
    SRC_UNIQUE_GUARD;
    ListenGuard lg(Hooks::listen_disable_depth);

    if (!loc_ref) {
        logger::error("Loc ref is null.");
        return;
    }
    const auto loc_refid = loc_ref->GetFormID();

    if (!locs_to_be_handled.contains(loc_refid)) {
        return;
    }

    if (!loc_ref->HasContainer()) {
        // remove the loc refid key from locs_to_be_handled map
        locs_to_be_handled.erase(loc_refid);
        return;
    }

    const auto loc_inventory_temp = loc_ref->GetInventory();
    for (const auto& [bound, entry] : loc_inventory_temp) {
        if (bound && IsDynamicFormID(bound->GetFormID()) && std::strlen(bound->GetName()) == 0) {
            QueueManager::GetSingleton()->QueueAddRemoveItemTask(
                {}, 
                RemoveItemTask(loc_refid, bound->GetFormID(), std::max(1, entry.first))
            );
        }
    }

    const auto loc_base = loc_ref->GetObjectReference()->GetFormID();
    SyncWithInventory(RefInfo(loc_refid, loc_base), loc_inventory_temp);
    Update(loc_ref);
    locs_to_be_handled.erase(loc_refid);
}

StageInstance* Manager::RegisterAtReceiveData(const FormID source_formid, const RefID loc,
                                              const StageInstancePlain& st_plain) {
    if (!source_formid) {
        logger::warn("Formid is null.");
        return nullptr;
    }

    if (const auto count = st_plain.count; !count) {
        logger::warn("Count is 0.");
        return nullptr;
    }
    if (!loc) {
        logger::warn("loc is 0.");
        return nullptr;
    }

    if (GetNInstancesFast() > _instance_limit) {
        logger::warn("Instance limit reached.");
        MsgBoxesNotifs::InGame::CustomMsg(
            std::format("The mod is tracking over {} instances. Maybe it is not bad to check your memory usage and "
                        "skse co-save sizes.",
                        _instance_limit));
    }

    // make new registry

    const auto src = ForceGetSource(source_formid);
    if (!src) {
        logger::warn("Source could not be obtained for formid {:x}.", source_formid);
        return nullptr;
    }

    //src->UpdateAddons();
    if (!src->IsHealthy()) {
        logger::warn("RegisterAtReceiveData: Source is not healthy.");
        return nullptr;
    }

    const auto stage_no = st_plain.no;
    if (!src->IsStageNo(stage_no)) {
        logger::warn("Stage not found.");
        return nullptr;
    }

    StageInstance new_instance(st_plain.start_time, stage_no, st_plain.count);
    const auto& stage_temp = src->GetStage(stage_no);
    new_instance.xtra.form_id = stage_temp.formid;
    new_instance.xtra.editor_id = clib_util::editorID::get_editorID(stage_temp.GetBound());
    new_instance.xtra.crafting_allowed = stage_temp.crafting_allowed;
    if (src->IsFakeStage(stage_no)) new_instance.xtra.is_fake = true;

    new_instance.SetDelay(st_plain);
    new_instance.xtra.is_transforming = st_plain.is_transforming;

    const auto instance = src->InsertNewInstance(new_instance, loc);

    if (!instance) {
        logger::warn("RegisterAtReceiveData: InsertNewInstance failed.");
        return nullptr;
    }

    UpdateLocationIndexForSource(*src, loc);

    return instance;
}

void Manager::ReceiveData() {
    logger::info("-------- Receiving data (Manager) ---------");

    if (m_Data.empty()) {
        logger::warn("ReceiveData: No data to receive.");
        return;
    }
    if (should_reset) {
        logger::info("ReceiveData: User wants to reset.");
        Reset();
        MsgBoxesNotifs::InGame::CustomMsg(
            "The mod has been reset. Please save and close the game. Do not forget to set bReset back to false in "
            "the INI before loading your save.");
        return;
    }

    // I need to deal with the fake forms from last session
    // trying to make sure that the fake forms in bank will be used when needed
    const auto DFT = DynamicFormTracker::GetSingleton();
    for (const auto source_forms = DFT->GetSourceForms(); const auto& [source_formid, source_editorid] : source_forms) {
        if (IsSource(source_formid)) {
            for (const auto dynamic_formid : DFT->GetFormSet(source_formid, source_editorid)) {
                DFT->Reserve(source_formid, source_editorid, dynamic_formid);
            }
        }
    }

    DFT->ApplyMissingActiveEffects();

    /////////////////////////////////


    for (const auto& [lhs, rhs] : m_Data) {
        const auto& [form_id, editor_id] = lhs.first;
        auto source_formid = form_id;
        const auto& source_editorid = editor_id;
        const auto loc = lhs.second;
        if (!source_formid) {
            logger::error("ReceiveData: FormID is null.");
            continue;
        }
        if (source_editorid.empty()) {
            logger::error("ReceiveData: EditorID is empty.");
            continue;
        }
        const auto source_form = FormReader::GetFormByID(0, source_editorid);
        if (!source_form) {
            logger::critical("ReceiveData: Source form not found. Saved FormID: {:x}, EditorID: {}", source_formid,
                             source_editorid);
            continue;
        }
        if (source_form->GetFormID() != source_formid) {
            logger::warn("ReceiveData: Source FormID does not match. Saved FormID: {:x}, EditorID: {}", source_formid,
                         source_editorid);
            source_formid = source_form->GetFormID();
        }

        SRC_UNIQUE_GUARD;
        for (const auto& st_plain : rhs) {
            if (st_plain.is_fake) locs_to_be_handled[loc].push_back(st_plain.form_id);
            if (const auto inserted_instance = RegisterAtReceiveData(source_formid, loc, st_plain);
                !inserted_instance) {
                logger::warn("ReceiveData: could not insert instance: FormID: {:x}, loc: {:x}", source_formid, loc);
                continue;
            }
        }
    }

    {
        ListenGuard lg(Hooks::listen_disable_depth);
        DFT->DeleteInactives();
    }
    if (DFT->GetNDeleted() > 0) {
        logger::warn("ReceiveData: Deleted forms exist. User is required to restart.");
        MsgBoxesNotifs::InGame::CustomMsg(
            "It seems the configuration has changed from your previous session"
            " that requires you to restart the game."
            "DO NOT IGNORE THIS:"
            "1. Save your game."
            "2. Exit the game."
            "3. Restart the game."
            "4. Load the saved game."
            "JUST DO IT! NOW! BEFORE DOING ANYTHING ELSE!");

        return;
    }

    HandleLoc(player_ref);
    SRC_UNIQUE_GUARD;
    locs_to_be_handled.erase(player_refid);
    Print();

    logger::info("--------Data received. Number of instances: {}---------", GetNInstancesFast());
}

void Manager::Print() {
    /*logger::info("Printing sources...Current time: {}", RE::Calendar::GetSingleton()->GetHoursPassed());
    for (auto& src : sources) {
        if (src.data.empty()) continue;
        src.PrintData();
    }*/
}

std::vector<Source> Manager::GetSources() {
    SRC_SHARED_GUARD;
    std::vector<Source> sources_copy;
    sources_copy.reserve(sources.size());
    for (const auto& src : sources | std::views::values) {
        sources_copy.push_back(*src);
    }
    return sources_copy;
}

std::vector<Source> Manager::GetSourcesByStageAndOwner(const FormID stage_formid, const RefID location_id) {
    std::vector<Source> out;
    if (!stage_formid || !location_id) {
        return out;
    }

    SRC_SHARED_GUARD;

    const auto lit = loc_to_sources.find(location_id);
    const size_t nLoc = (lit == loc_to_sources.end()) ? 0 : lit->second.size();
    if (nLoc == 0) {
        return out;
    }

    const auto stIt = stage_to_sources.find(stage_formid);
    const bool haveStageSet = (stIt != stage_to_sources.end() && !stIt->second.empty());
    const size_t nStage = haveStageSet ? stIt->second.size() : SIZE_MAX;

    out.reserve(haveStageSet ? std::min(nLoc, nStage) : nLoc);

    auto try_add_if_match = [&](const FormID src_formid) {
        const auto sit = sources.find(src_formid);
        if (sit == sources.end()) return;

        Source* src = sit->second.get();
        if (!src || !src->IsHealthy()) return;

        const auto dit = src->data.find(location_id);
        if (dit == src->data.end() || dit->second.empty()) return;

        for (const auto& inst : dit->second) {
            if (inst.count > 0 && inst.xtra.form_id == stage_formid) {
                out.push_back(*src);
                return;
            }
        }
    };

    // Iterate the smaller candidate set
    if (haveStageSet && nStage <= nLoc) {
        for (const FormID src_formid : stIt->second) {
            if (!lit->second.contains(src_formid)) continue;
            try_add_if_match(src_formid);
        }
        return out;
    }

    const auto stageSet = haveStageSet ? &stIt->second : nullptr;

    for (const FormID src_formid : lit->second) {
        if (stageSet && !stageSet->contains(src_formid)) continue;
        try_add_if_match(src_formid);
    }

    return out;
}

std::unordered_map<RefID, float> Manager::GetUpdateQueue() {
    std::unordered_map<RefID, float> _ref_stops_copy;
    QUE_SHARED_GUARD;
    for (const auto& [key, value] : _ref_stops_) {
        _ref_stops_copy[key] = value.stop_time;
    }
    return _ref_stops_copy;
}

void Manager::HandleDynamicWO(RE::TESObjectREFR* ref) {
    // if there is an object in the world that is a dynamic base form and comes from this mod, swap it back to the main stage form
    if (!ref) return;

    if (const auto bound = ref->GetObjectReference()) {
        if (!bound->IsDynamicForm()) return;
        const auto src = GetSourceByLocation(ref->GetFormID());
        if (!src) return;
        WorldObject::SwapObjects(ref, src->GetBoundObject(), false);
    }
}

void Manager::HandleWOBaseChange(RE::TESObjectREFR* ref) {
    if (!ref) return;
    if (const auto bound = ref->GetObjectReference()) {
        if (bound->IsDynamicForm()) return HandleDynamicWO(ref);
        const auto a_refid = ref->GetFormID();
        const auto src = GetSourceByLocation(a_refid);
        if (!src || !src->IsHealthy()) return;
        const auto it = src->data.find(a_refid);
        if (it == src->data.end() || it->second.empty() || it->second.size() > 1 || it->second.front().count <= 0) {
            return;
        }
        const auto st_inst = &it->second.front();
        if (!st_inst || st_inst->count <= 0) return;
        if (const auto bound_expected = src->IsFakeStage(st_inst->no) ? src->GetBoundObject() : st_inst->GetBound();
            bound_expected->GetFormID() != bound->GetFormID()) {
            st_inst->count = 0;
        }
    }
}

std::vector<RefInfo> Manager::GetRefStops() {
    std::vector<RefInfo> ref_stops_copy;
    QUE_SHARED_GUARD;
    ref_stops_copy.reserve(_ref_stops_.size());
    for (const auto& refstop : _ref_stops_ | std::views::values) {
        ref_stops_copy.emplace_back(refstop.ref_info);
    }
    return ref_stops_copy;
}