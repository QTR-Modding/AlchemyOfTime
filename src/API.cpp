#include "API.h"
#include "Manager.h"

namespace {
    template <class T>
    AoTAPI::Array<T> View(const std::vector<T>& values) {
        return {values.data(), static_cast<std::uint32_t>(values.size())};
    }

    AoTAPI::InstanceInfo ReadInstance(const Source& source, RefID location, const StageInstance& instance, float now) {
        AoTAPI::InstanceInfo result{};
        result.source = source.formid;
        result.location = location;
        result.item = instance.xtra.form_id;
        result.count = instance.count;
        result.stage = instance.no;
        result.trigger = instance.GetDelayerFormID();
        result.sampledAt = now;
        result.startedAt = instance.start_time;
        result.transforming = instance.xtra.is_transforming;
        result.elapsed = result.transforming ? instance.GetTransformElapsed(now) : instance.GetElapsed(now);
        result.duration = source.GetStageDuration(instance.no);
        if (result.transforming) {
            const auto transform = source.settings.transformers.find(result.trigger);
            result.duration = transform == source.settings.transformers.end() ? 0.f : transform->second.second;
        }
        result.speed = instance.GetDelaySlope();
        result.nextUpdate = source.GetNextUpdateTime(&instance);
        result.timing = AoTAPI::TimingState::NoTransition;
        const bool terminal = !result.transforming &&
                              ((source.settings.numbers.size() < 2 && source.settings.decayed_id == result.item) ||
                               (result.speed < 0.f && result.stage == 0) ||
                               (!result.trigger && result.duration > Settings::critical_stage_dur));
        if (!instance.xtra.is_decayed && !terminal) {
            if (std::abs(result.speed) < EPSILON)
                result.timing = AoTAPI::TimingState::Paused;
            else if (result.nextUpdate > 0.f && std::isfinite(result.nextUpdate))
                result.timing = AoTAPI::TimingState::Running;
        }
        result.remainingHours = result.timing == AoTAPI::TimingState::Running
                                    ? std::max(0.f, result.nextUpdate - now)
                                    : std::numeric_limits<float>::infinity();
        result.progressPercentage = source.GetProgressPercentage(instance, now);
        result.dynamic = instance.xtra.is_fake;
        result.decayed = instance.xtra.is_decayed;
        return result;
    }

    struct RuleStorage {
        std::vector<AoTAPI::StageInfo> stages;
        std::vector<RE::FormID> containers;
        std::vector<AoTAPI::TransformationInfo> transformations;
        std::vector<AoTAPI::TimeModifierInfo> timeModifiers;
        std::vector<std::vector<RE::FormID>> triggerContainers;
        std::vector<std::vector<std::uint32_t>> allowedStages;

        AoTAPI::Filters ReadFilters(FormID trigger,
                                   const std::unordered_map<FormID, std::unordered_set<FormID>>& containerFilters,
                                   const std::unordered_map<FormID, std::unordered_set<StageNo>>& stageFilters) {
            triggerContainers.emplace_back();
            if (const auto found = containerFilters.find(trigger); found != containerFilters.end())
                triggerContainers.back().assign(found->second.begin(), found->second.end());
            const auto& stagesForTrigger = stageFilters.at(trigger);
            allowedStages.emplace_back(stagesForTrigger.begin(), stagesForTrigger.end());
            return {View(triggerContainers.back()), View(allowedStages.back())};
        }

        AoTAPI::SourceInfo Read(const Source& source) {
            const auto& settings = source.settings;
            for (const auto number : settings.numbers) {
                const auto stage = source.TryGetStage(number);
                const auto item = stage ? stage->formid : settings.items.at(number);
                stages.push_back({number, item, settings.stage_names.at(number).c_str(),
                                  source.GetStageDuration(number), source.IsFakeStage(number),
                                  settings.crafting_allowed.at(number)});
            }
            containers.assign(settings.containers.begin(), settings.containers.end());
            for (const auto& [trigger, transform] : settings.transformers) {
                transformations.push_back({trigger, transform.first, transform.second,
                                           ReadFilters(trigger, settings.transformer_containers,
                                                       settings.transformer_allowed_stages)});
            }
            for (const auto& [trigger, magnitude] : settings.delayers) {
                timeModifiers.push_back({trigger, magnitude,
                                         ReadFilters(trigger, settings.delayer_containers,
                                                     settings.delayer_allowed_stages)});
            }
            return {source.formid, source.editorid.c_str(), source.qFormType.c_str(), settings.decayed_id,
                    View(stages), View(containers), View(transformations), View(timeModifiers)};
        }
    };

    bool VisitInstances(RE::FormID location, RE::FormID item,
                        bool (*visitor)(const AoTAPI::InstanceInfo*, void*), void* context) {
        if (!M || M->isUninstalled.load() || !visitor) return false;
        const auto sources = location ? M->GetSourcesByStageAndOwner(item, location) : M->GetSources();
        const auto now = RE::Calendar::GetSingleton()->GetHoursPassed();
        for (const auto& source : sources) {
            if (!source.IsHealthy()) continue;
            for (const auto& [owner, instances] : source.data) {
                if (location && owner != location) continue;
                for (const auto& instance : instances) {
                    if (instance.count <= 0 || (item && instance.xtra.form_id != item)) continue;
                    const auto snapshot = ReadInstance(source, owner, instance, now);
                    if (!visitor(&snapshot, context)) return true;
                }
            }
        }
        return true;
    }

    bool VisitSources(RE::FormID sourceID, bool (*visitor)(const AoTAPI::SourceInfo*, void*), void* context) {
        if (!M || M->isUninstalled.load() || !visitor) return false;
        const auto sources = M->GetSources(sourceID);
        for (const auto& source : sources) {
            if (!source.IsHealthy()) continue;
            RuleStorage storage;
            const auto snapshot = storage.Read(source);
            if (!visitor(&snapshot, context)) break;
        }
        return true;
    }

    bool VisitLocations(bool (*visitor)(RE::FormID, void*), void* context) {
        if (!M || M->isUninstalled.load() || !visitor) return false;
        const auto sources = M->GetSources();
        std::unordered_set<RefID> locations;
        for (const auto& source : sources) {
            if (!source.IsHealthy()) continue;
            for (const auto& [location, instances] : source.data) {
                if (!std::ranges::any_of(instances, [](const StageInstance& instance) { return instance.count > 0; }))
                    continue;
                if (locations.insert(location).second && !visitor(location, context)) return true;
            }
        }
        return true;
    }
}

extern "C" __declspec(dllexport) const AoTAPI::Interface* AoT_RequestAPI(std::uint32_t version) {
    if (version != AoTAPI::interface_version) return nullptr;
    static const AoTAPI::Interface api{VisitInstances, VisitSources, VisitLocations};
    return &api;
}
