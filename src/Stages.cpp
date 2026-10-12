#include "Stages.h"
#include "Settings.h"
#include "CLibUtilsQTR/FormReader.hpp"

bool StageInstance::operator==(const StageInstance& other) const {
    return no == other.no && count == other.count &&
           // location == other.location &&
           fabs(start_time - other.start_time) < EPSILON && fabs(_elapsed - other._elapsed) < EPSILON &&
           xtra == other.xtra;
}

bool StageInstance::AlmostSameExceptCount(const StageInstance& other, const float curr_time) const {
    // bcs they are in the same inventory they will have same delay magnitude
    // delay starts might be different but if the elapsed times are close enough, we don't care
    return no == other.no &&
           // location == other.location &&
           std::abs(start_time - other.start_time) < 0.015 &&
           std::abs(GetElapsed(curr_time) - other.GetElapsed(curr_time)) < 0.015 && xtra == other.xtra;
}

StageInstance& StageInstance::operator=(const StageInstance& other) {
    if (this != &other) {
        start_time = other.start_time;
        no = other.no;
        count = other.count;
        // location = other.location;

        xtra = other.xtra;

        _elapsed = other._elapsed;
        _delay_start = other._delay_start;
        _delay_mag = other._delay_mag;
        _delay_formid = other._delay_formid;
    }
    return *this;
}

RE::TESBoundObject* StageInstance::GetBound() const {
    return FormReader::GetFormByID<RE::TESBoundObject>(xtra.form_id);
}

float StageInstance::GetElapsed(const float curr_time) const {
    if (fabs(_delay_mag) < EPSILON) return _elapsed;
    return (curr_time - _delay_start) * GetDelaySlope() + _elapsed;
}

float StageInstance::GetDelaySlope() const {
    return std::min(std::max(-Settings::max_modulator_strength, _delay_mag), Settings::max_modulator_strength);
}

void StageInstance::SetNewStart(const float start_t) {
    start_time = start_t;
    _delay_start = start_time;
    _elapsed = 0;
}

void StageInstance::SetNewStart(const float curr_time, const float overshot) {
    // overshot: by how much is the schwelle already ueberschritten
    start_time = curr_time - overshot / (GetDelaySlope() + std::numeric_limits<float>::epsilon());
    _delay_start = start_time;
    _elapsed = 0;
}

void StageInstance::SetDelay(const float time, const float delay, const FormID formid) {
    // yeni steigungla yeni ausgangspunkt yapiyoruz
    // call only from UpdateTimeModulationInInventory
    if (xtra.is_transforming) return;
    if (fabs(_delay_mag - delay) < EPSILON && _delay_formid == formid) return;

    _elapsed = GetElapsed(time);
    _delay_start = time;
    _delay_mag = delay;
    _delay_formid = formid;
}

void StageInstance::SetTransform(const float time, const FormID formid) {
    if (xtra.is_transforming) {
        if (_delay_formid != formid) {
            RemoveTransform(time);
            return SetTransform(time, formid);
        }
        return;
    }
    SetDelay(time, 1, formid);
    xtra.is_transforming = true;
}

void StageInstance::RemoveTransform(const float curr_time) {
    if (!xtra.is_transforming) return;
    xtra.is_transforming = false;
    _delay_start = curr_time;
    _delay_mag = 1;
    _delay_formid = 0;
}

void StageInstance::RemoveTimeMod(const float time) {
    RemoveTransform(time);
    SetDelay(time, 1, 0);
}

StageInstancePlain StageInstance::GetPlain() const {
    StageInstancePlain plain;
    plain.start_time = start_time;
    plain.no = no;
    plain.count = count;

    plain.is_fake = xtra.is_fake;
    plain.is_decayed = xtra.is_decayed;
    plain.is_transforming = xtra.is_transforming;

    plain._elapsed = _elapsed;
    plain._delay_start = _delay_start;
    plain._delay_mag = _delay_mag;
    plain._delay_formid = _delay_formid;

    if (xtra.is_fake) plain.form_id = xtra.form_id;

    return plain;
}

void StageInstance::SetDelay(const StageInstancePlain& plain) {
    _elapsed = plain._elapsed;
    _delay_start = plain._delay_start;
    _delay_mag = plain._delay_mag;
    _delay_formid = plain._delay_formid;
}

RE::TESBoundObject* Stage::GetBound() const { return FormReader::GetFormByID<RE::TESBoundObject>(formid); }

bool Stage::CheckIntegrity() const {
    if (!formid || !GetBound()) {
        logger::error("FormID or bound is null");
        return false;
    }
    if (duration <= 0) {
        logger::error("Duration is 0 or negative");
        return false;
    }
    return true;
}
