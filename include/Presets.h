#pragma once
#include "Stages.h"
#include "tsl/ordered_map.h"


enum class TriggerKind : std::uint32_t { kTransformer, kDelayer };

struct Delayer {
    float magnitude;
    float activation_delay = 0.0f;
    std::vector<RE::BGSPerk*> fx_conditions{};
};

struct Transformer {
    FormID final_form;
    Duration duration;
    float activation_delay = 0.0f;
    std::vector<RE::BGSPerk*> fx_conditions{};
};

struct AddOnSettings {
    std::unordered_set<FormID> containers;
    bool containers_defined = false;

    tsl::ordered_map<FormID, Delayer> delayers;
    std::unordered_map<FormID, uint32_t> delayer_colors;
    std::unordered_map<FormID, FormID> delayer_sounds;
    std::unordered_map<FormID, FormID> delayer_artobjects;
    std::unordered_map<FormID, FormID> delayer_effect_shaders;
    std::unordered_map<FormID, std::unordered_set<FormID>> delayer_containers;
    std::unordered_map<FormID, std::unordered_set<StageNo>> delayer_allowed_stages;

    tsl::ordered_map<FormID, Transformer> transformers;
    std::unordered_map<FormID, uint32_t> transformer_colors;
    std::unordered_map<FormID, FormID> transformer_sounds;
    std::unordered_map<FormID, FormID> transformer_artobjects;
    std::unordered_map<FormID, FormID> transformer_effect_shaders;
    std::unordered_map<FormID, std::unordered_set<FormID>> transformer_containers;
    std::unordered_map<FormID, std::unordered_set<StageNo>> transformer_allowed_stages;

    [[nodiscard]] bool IsHealthy() const { return !init_failed; }

    [[nodiscard]] bool CheckIntegrity();

    void Merge(const AddOnSettings& addon);
    [[nodiscard]] bool HasValidActivationDelays() const;

private:
    bool init_failed = false;
};

struct DefaultSettings : AddOnSettings {
    std::map<StageNo, FormID> items = {};
    std::map<StageNo, Duration> durations = {};
    std::map<StageNo, StageName> stage_names = {};
    std::map<StageNo, bool> crafting_allowed = {};
    std::map<StageNo, int> costoverrides = {};
    std::map<StageNo, float> weightoverrides = {};
    std::map<StageNo, std::vector<StageEffect>> effects = {};
    std::vector<StageNo> numbers = {};
    FormID decayed_id = 0;
    std::map<StageNo, uint32_t> colors = {};
    std::map<StageNo, FormID> sounds = {};
    std::map<StageNo, FormID> artobjects = {};
    std::map<StageNo, FormID> effect_shaders = {};

    [[nodiscard]] bool IsHealthy() const { return !init_failed; }

    [[nodiscard]] bool CheckIntegrity();

    [[nodiscard]] bool IsEmpty();

    void Add(AddOnSettings& addon);
    static void AddHelper(std::unordered_map<FormID, FormID>& dest, const std::unordered_map<FormID, FormID>& src);

private:
    bool init_failed = false;
};

using CustomSettings = std::vector<std::pair<std::vector<std::string>, DefaultSettings>>;