/*
 * Sideous Effects - the actual effect chain: params, and per-sample
 * processing for one stereo pair. Deliberately DPF-agnostic (no
 * DistrhoPlugin.hpp dependency), same convention as the family's DrumEngine
 * classes. 5 tonal stages (Bitcrush, Fold, RingMod, Lofi Filter, Delay) run
 * in a user-reorderable order (see Params.hpp's resolveChainOrder()/
 * StageKind - shared with the UI side so the two can't disagree on
 * ordering), then Master Drive -> Master Output always run last. Every
 * reorderable stage has its own Mix knob (dry/wet blend into the next
 * stage); Master applies to the finished stereo pair.
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
        // distinct seeds so L/R jitter independently instead of in lockstep,
        // same "seed each instance differently" convention DrumEngine.hpp
        // uses for its noise generators
        fL.delay.setJitterSeed(0x6A09E667u);
        fR.delay.setJitterSeed(0xBB67AE85u);
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
        case kParamDelaySpeed:
            fL.delay.setSpeed(value); fR.delay.setSpeed(value);
            break;
        case kParamDelayRamp:
            fL.delay.setRampSemitones(value); fR.delay.setRampSemitones(value);
            break;
        case kParamDelayArpSteps:
        {
            const int steps = (int)(value + 0.5f);
            fL.delay.setArpSteps(steps); fR.delay.setArpSteps(steps);
            break;
        }
        case kParamDelayArpCurve:
        {
            const ArpCurve curve = value < 0.5f ? ArpCurve::Pitch : ArpCurve::Tempo;
            fL.delay.setArpCurve(curve); fR.delay.setArpCurve(curve);
            break;
        }
        case kParamDelayArpDirection:
        {
            const ArpDirection dir = (ArpDirection)(int)(value + 0.5f);
            fL.delay.setArpDirection(dir); fR.delay.setArpDirection(dir);
            break;
        }
        case kParamDelayPingPong: fDelayPingPong = value; break;
        case kParamDelayJitter:
            fL.delay.setJitterAmount(value); fR.delay.setJitterAmount(value);
            break;
        case kParamDelayMix: fDelayMix = value; break;

        case kParamMasterOutput: fMasterOutput = value; break;
        case kParamMasterDrive:  fDistortion.setDrive(value); break;

        case kParamChainPosBitcrush:
        case kParamChainPosFold:
        case kParamChainPosRingMod:
        case kParamChainPosLofi:
        case kParamChainPosDelay:
            resolveChainOrder(fValues, fChainOrder);
            break;

        default: break;
        }
    }

    void process(float inL, float inR, float& outL, float& outR) noexcept
    {
        outL = processChannel(inL, fL, false);
        outR = processChannel(inR, fR, true);

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

    float applyStage(int stage, float x, Channel& ch, bool isRightChannel) noexcept
    {
        switch (stage)
        {
        case kStageBitcrush:
        {
            const float bc = ch.crusher.process(x);
            return x + (bc - x) * fBitcrushMix;
        }
        case kStageFold:
        {
            const float fo = ch.fold.process(x);
            return x + (fo - x) * fFoldMix;
        }
        case kStageRingMod:
        {
            const float rm = ch.ring.process(x);
            return x + (rm - x) * fRingMix;
        }
        case kStageLofi:
        {
            const float lf = ch.filter.process(x, fLofiCutoffHz);
            return x + (lf - x) * fLofiMix;
        }
        case kStageDelay:
        {
            float dl = ch.delay.process(x);
            if (fDelayPingPong > 0.0f)
            {
                // alternate taps favor L/R - even tap index favors Left,
                // odd favors Right; the "off" channel for this tap gets
                // attenuated by the Ping-Pong amount instead of hard-panned,
                // so it stays usable as a subtle width control too
                const bool tapFavorsRight = (ch.delay.getTapIndex() % 2) != 0;
                if (tapFavorsRight != isRightChannel)
                    dl *= (1.0f - fDelayPingPong);
            }
            return x + (dl - x) * fDelayMix;
        }
        default:
            return x;
        }
    }

    float processChannel(float in, Channel& ch, bool isRightChannel) noexcept
    {
        float x = in;
        for (int i = 0; i < kStageCount; ++i)
            x = applyStage(fChainOrder[i], x, ch, isRightChannel);
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

    int fChainOrder[kStageCount] = { kStageBitcrush, kStageFold, kStageRingMod, kStageLofi, kStageDelay };

    float fBitcrushMix = 0.0f;
    float fFoldMix = 0.0f;
    float fRingMix = 0.0f;
    float fLofiCutoffHz = 3000.0f;
    float fLofiMix = 0.0f;
    float fDelaySync = 9.0f;
    float fDelayPingPong = 0.0f;
    float fDelayMix = 0.0f;
    float fMasterOutput = 0.85f;
};

} // namespace sideous
