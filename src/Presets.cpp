#include "Presets.h"
#include "CLibUtilsQTR/FormReader.hpp"



bool DefaultSettings::CheckIntegrity() {
    if (items.empty() || durations.empty() || stage_names.empty() || effects.empty() || numbers.empty()) {
        logger::error("One of the maps is empty.");
        init_failed = true;
        return false;
    }
    if (items.size() != durations.size() || items.size() != stage_names.size() || items.size() != numbers.size()) {
        logger::error("Sizes do not match.");
        init_failed = true;
        return false;
    }
    for (auto i = 0; i < numbers.size(); i++) {
        if (!std::ranges::contains(numbers, i)) {
            logger::error("Key {} not found in numbers.", i);
            return false;
        }
        if (!items.contains(i) || !crafting_allowed.contains(i) || !durations.contains(i) || !stage_names.contains(i) ||
            !effects.contains(i)) {
            logger::error("Key {} not found in all maps.", i);
            init_failed = true;
            return false;
        }

        if (durations[i] <= 0) {
            logger::error("Duration is less than or equal 0.");
            init_failed = true;
            return false;
        }

        if (!costoverrides.contains(i)) costoverrides[i] = -1;
        if (!weightoverrides.contains(i)) weightoverrides[i] = -1.0f;
    }
    if (!decayed_id) {
        logger::error("Decayed id is 0.");
        init_failed = true;
        return false;
    }
    for (const auto& [a_formID, _transformer] : transformers) {
        const FormID _finalFormEditorID = _transformer.first;
        const Duration _duration = _transformer.second;
        const auto& _allowedStages = transformer_allowed_stages.at(a_formID);
        if (!FormReader::GetFormByID(a_formID) || !FormReader::GetFormByID<RE::TESBoundObject>(_finalFormEditorID)) {
            logger::error("Formid not found.");
            init_failed = true;
            return false;
        }
        if (_duration <= 0) {
            logger::error("Duration is less than or equal 0.");
            init_failed = true;
            return false;
        }
        if (_allowedStages.empty()) {
            logger::error("Allowed stages is empty.");
            init_failed = true;
            return false;
        }
        for (const auto& _stage : _allowedStages) {
            if (!std::ranges::contains(numbers, _stage)) {
                logger::error("Stage {} not found in numbers.", _stage);
                init_failed = true;
                return false;
            }
        }
    }

    for (const auto& a_formID : delayers | std::views::keys) {
        const auto& _allowedStages = delayer_allowed_stages.at(a_formID);
        if (!FormReader::GetFormByID(a_formID)) {
            logger::error("Delayer formid {:x} not found.", a_formID);
            init_failed = true;
            return false;
        }
        if (_allowedStages.empty()) {
            logger::error("Allowed stages is empty for delayer formid {:x}.", a_formID);
            init_failed = true;
            return false;
        }
        for (const auto& _stage : _allowedStages) {
            if (!std::ranges::contains(numbers, _stage)) {
                logger::error("Stage {} not found in numbers for delayer formid {:x}.", _stage, a_formID);
                init_failed = true;
                return false;
            }
        }
    }

    for (const auto& _formID : containers) {
        if (!FormReader::GetFormByID(_formID)) {
            logger::error("Container formid {:x} not found.", _formID);
            init_failed = true;
            return false;
        }
    }
    return true;
}

bool DefaultSettings::IsEmpty() {
    if (numbers.empty()) {
        init_failed = true;
        return true;
    }
    return false;
}

// check it
void DefaultSettings::Add(AddOnSettings& addon) {
    // containers
    if (addon.containers_defined) containers = addon.containers;

    // delayers
    for (const auto& [a_formID, _delay] : addon.delayers) {
        if (!a_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        delayers[a_formID] = _delay;

        if (addon.delayer_allowed_stages.contains(a_formID)) {
            delayer_allowed_stages[a_formID].insert_range(addon.delayer_allowed_stages.at(a_formID));
        }
        if (delayer_allowed_stages.at(a_formID).empty()) {
            delayer_allowed_stages[a_formID] = std::unordered_set(numbers.begin(), numbers.end());
        }
    }
    // transformers
    for (const auto& [a_formID, _transformer] : addon.transformers) {
        if (!a_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        transformers[a_formID] = _transformer;
        if (addon.transformer_allowed_stages.contains(a_formID)) {
            transformer_allowed_stages[a_formID].insert_range(addon.transformer_allowed_stages.at(a_formID));
        }
        if (transformer_allowed_stages.at(a_formID).empty()) {
            transformer_allowed_stages.at(a_formID) = std::unordered_set(numbers.begin(), numbers.end());
        }
    }

    AddHelper(delayer_colors, addon.delayer_colors);
    AddHelper(delayer_sounds, addon.delayer_sounds);
    AddHelper(delayer_artobjects, addon.delayer_artobjects);
    AddHelper(delayer_effect_shaders, addon.delayer_effect_shaders);
    AddHelper(transformer_colors, addon.transformer_colors);
    AddHelper(transformer_sounds, addon.transformer_sounds);
    AddHelper(transformer_artobjects, addon.transformer_artobjects);
    AddHelper(transformer_effect_shaders, addon.transformer_effect_shaders);

    // delayer/transformer containers
    for (const auto& [a_formID, _containers] : addon.delayer_containers) {
        if (!a_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        for (const auto& _container : _containers) {
            if (!_container) {
                logger::critical("AddOn has null formid.");
                continue;
            }
            delayer_containers[a_formID].insert(_container);
        }
    }
    for (const auto& [_formID, _containers] : addon.transformer_containers) {
        if (!_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        for (const auto& _container : _containers) {
            if (!_container) {
                logger::critical("AddOn has null formid.");
                continue;
            }
            transformer_containers[_formID].insert(_container);
        }
    }
}

void DefaultSettings::AddHelper(std::unordered_map<FormID, FormID>& dest,
                                const std::unordered_map<FormID, FormID>& src) {
    for (const auto& [_formID, _art] : src) {
        if (!_formID) {
            logger::critical("AddOn has null formid.");
            continue;
        }
        dest[_formID] = _art;
    }
}


// check it
void AddOnSettings::Merge(const AddOnSettings& addon) {
    if (addon.containers_defined) {
        containers = addon.containers;
        containers_defined = true;
    }
    for (const auto& [id, value] : addon.delayers) delayers[id] = value;
    for (const auto& [id, value] : addon.transformers) transformers[id] = value;

    // A redefined trigger replaces its optional fields too, including omitted ones.
    const auto replace = [](auto& dest, const auto& src, const auto& triggers) {
        for (const auto id : triggers | std::views::keys) {
            if (const auto it = src.find(id); it != src.end())
                dest[id] = it->second;
            else
                dest.erase(id);
        }
    };
    replace(delayer_colors, addon.delayer_colors, addon.delayers);
    replace(delayer_sounds, addon.delayer_sounds, addon.delayers);
    replace(delayer_artobjects, addon.delayer_artobjects, addon.delayers);
    replace(delayer_effect_shaders, addon.delayer_effect_shaders, addon.delayers);
    replace(delayer_containers, addon.delayer_containers, addon.delayers);
    replace(delayer_allowed_stages, addon.delayer_allowed_stages, addon.delayers);
    replace(transformer_colors, addon.transformer_colors, addon.transformers);
    replace(transformer_sounds, addon.transformer_sounds, addon.transformers);
    replace(transformer_artobjects, addon.transformer_artobjects, addon.transformers);
    replace(transformer_effect_shaders, addon.transformer_effect_shaders, addon.transformers);
    replace(transformer_containers, addon.transformer_containers, addon.transformers);
    replace(transformer_allowed_stages, addon.transformer_allowed_stages, addon.transformers);
}

bool AddOnSettings::CheckIntegrity() {
    for (const auto& _formID : containers) {
        if (!FormReader::GetFormByID(_formID)) {
            logger::error("Container form {} not found.", _formID);
            init_failed = true;
            return false;
        }
    }

    for (const auto& a_formID : delayers | std::views::keys) {
        if (!FormReader::GetFormByID(a_formID)) {
            logger::error("Delayer form {} not found.", a_formID);
            init_failed = true;
            return false;
        }
    }

    for (const auto& [a_formid, _transformer] : transformers) {
        const FormID _finalFormEditorID = _transformer.first;
        const Duration _duration = _transformer.second;
        if (!FormReader::GetFormByID(a_formid) || !FormReader::GetFormByID<RE::TESBoundObject>(_finalFormEditorID)) {
            logger::error("Form not found.");
            init_failed = true;
            return false;
        }
        if (_duration <= 0) {
            logger::error("Duration is less than or equal 0.");
            init_failed = true;
            return false;
        }
    }
    return true;
}
