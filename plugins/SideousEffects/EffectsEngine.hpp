/*
 * Sideous Effects - the actual effect chain: params, and per-sample
 * processing for one stereo pair. Deliberately DPF-agnostic (no
 * DistrhoPlugin.hpp dependency), same convention as the family's DrumEngine
 * classes. Fixed signal order: Bitcrush -> Fold -> RingMod -> LofiFilter ->
 * Delay -> Master Drive -> Master Output. Every stage before Master has its
 * own Mix knob (dry/wet blend into the next stage); Master applies to the
 * finished stereo pair.
 */

#pragma once

#include <cmath>
#include <cstdint>

#include "Params.hpp"
#include "dsp/BitCrusher.hpp"
#include "dsp/Fold.hpp"
#include "dsp/RingMod.hpp"
#include "dsp/Filter.hpp"
#include "dsp/DelayLine.hpp"
#include "dsp/Distortion.hpp"
#include "dsp/Sync.hpp"

namespace sideous {

// delay time in seconds for a given Sync param value (0 = Free -> a fixed
// fallback, since this effect has no separate free-running Time knob) and
// host BPM - same shape as sideous's own syncedHz() but returns seconds
inline float delaySecondsFromSync(float syncParam, double bpm) noexcept
{
    const int sync = (int)(syncParam + 0.5f);
    if (sync <= 0)
        return 0.5f; // Free: a reasonable fixed default, no free-time knob in this plugin
    const float beats = syncDivisionBeats((SyncDivision)(sync - 1));
    return (float)(beats * 60.0 / bpm);
}

class EffectsEngine
{
public:
    EffectsEngine() noexcept
    {
        for (uint32_t i = 0; i < kParamCount; ++i)
            setParameterValue(i, getParamInfo(i).def);
    }

    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        fL.filter.setSampleRate(sampleRate);
        fR.filter.setSampleRate(sampleRate);
        fL.filter.setType(FilterType::Lowpass);
        fR.filter.setType(FilterType::Lowpass);
        fL.ring.setSampleRate(sampleRate);
        fR.ring.setSampleRate(sampleRate);
        fL.delay.setSampleRate(sampleRate);
        fR.delay.setSampleRate(sampleRate);
        updateDelayTime();
    }

    // called once per block (or once per frame - cheap enough either way);
    // the delay's actual time in seconds depends on both the Sync param and
    // the live host tempo, so it's recomputed whenever either changes
    void setBpm(double bpm) noexcept
    {
        if (bpm != fBpm)
        {
            fBpm = bpm;
            updateDelayTime();
        }
    }

    float getParameterValue(uint32_t index) const noexcept { return fValues[index]; }

    void setParameterValue(uint32_t index, float value) noexcept
    {
        fValues[index] = value;

        switch (index)
        {
        case kParamBitcrushBits:
            fL.crusher.setBits(value); fR.crusher.setBits(value);
            break;
        case kParamBitcrushRate:
            fL.crusher.setRate(value); fR.crusher.setRate(value);
            break;
        case kParamBitcrushMix: fBitcrushMix = value; break;

        case kParamFoldDrive:
            fL.fold.setDrive(value); fR.fold.setDrive(value);
            break;
        case kParamFoldMix: fFoldMix = value; break;

        case kParamRingModWave:
        {
            const Waveform w = (Waveform)(int)(value + 0.5f);
            fL.ring.setWaveform(w); fR.ring.setWaveform(w);
            break;
        }
        case kParamRingModFreq:
            fL.ring.setFrequency(value); fR.ring.setFrequency(value);
            break;
        case kParamRingModMix: fRingMix = value; break;

        case kParamLofiCutoff: fLofiCutoffHz = value; break;
        case kParamLofiReso:
            fL.filter.setResonance(value); fR.filter.setResonance(value);
            break;
        case kParamLofiMix: fLofiMix = value; break;

        case kParamDelaySync: fDelaySync = value; updateDelayTime(); break;
        case kParamDelayFeedback:
            fL.delay.setFeedback(value); fR.delay.setFeedback(value);
            break;
        case kParamDelayArp:
            fL.delay.setArpSemitones(value); fR.delay.setArpSemitones(value);
            break;
        case kParamDelayMix: fDelayMix = value; break;

        case kParamMasterOutput: fMasterOutput = value; break;
        case kParamMasterDrive:  fDistortion.setDrive(value); break;

        default: break;
        }
    }

    void process(float inL, float inR, float& outL, float& outR) noexcept
    {
        outL = processChannel(inL, fL);
        outR = processChannel(inR, fR);

        outL = fDistortion.process(outL) * fMasterOutput;
        outR = fDistortion.process(outR) * fMasterOutput;
    }

private:
    struct Channel
    {
        BitCrusher crusher;
        Fold fold;
        RingMod ring;
        Filter filter;
        DelayLine delay;
    };

    float processChannel(float in, Channel& ch) noexcept
    {
        float x = in;

        const float bc = ch.crusher.process(x);
        x += (bc - x) * fBitcrushMix;

        const float fo = ch.fold.process(x);
        x += (fo - x) * fFoldMix;

        const float rm = ch.ring.process(x);
        x += (rm - x) * fRingMix;

        const float lf = ch.filter.process(x, fLofiCutoffHz);
        x += (lf - x) * fLofiMix;

        const float dl = ch.delay.process(x);
        x += (dl - x) * fDelayMix;

        return x;
    }

    void updateDelayTime() noexcept
    {
        const float seconds = delaySecondsFromSync(fDelaySync, fBpm > 0.0 ? fBpm : 120.0);
        fL.delay.setDelayTime(seconds);
        fR.delay.setDelayTime(seconds);
    }

    float fValues[kParamCount] {};
    double fSampleRate = 44100.0;
    double fBpm = 120.0;

    Channel fL, fR;
    Distortion fDistortion; // stateless per-call, one instance covers both channels

    float fBitcrushMix = 0.0f;
    float fFoldMix = 0.0f;
    float fRingMix = 0.0f;
    float fLofiCutoffHz = 3000.0f;
    float fLofiMix = 0.0f;
    float fDelaySync = 9.0f;
    float fDelayMix = 0.0f;
    float fMasterOutput = 0.85f;
};

} // namespace sideous
