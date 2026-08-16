/*
 * Sideous Effects - tempo-synced feedback delay with an optional
 * "chip-arpeggiated" mode. Mono; the plugin runs one instance per channel.
 *
 * Normal operation (Arp = 0, Speed = 1x, Ramp = 0): a standard
 * circular-buffer feedback delay - read pointer trails the write pointer
 * by exactly `delaySamples`.
 *
 * Arp mode (Arp > 0, semitones): thought of in "grains" instead - each
 * grain is exactly one repeat-length (`delaySamples`) long. A grain reads
 * the buffer at a *rate* for its whole duration (linear-interpolated
 * fractional index); the rate cycles through a repeating pattern once per
 * grain, `ArpSteps` taps long (2-4), shaped by `ArpDirection` (Up/Down/
 * Up-Down) and `ArpCurve` (Pitch: equal semitone steps between taps, so
 * playback rate compounds multiplicatively; Tempo: equal rate/speed steps
 * instead, taming how much later taps accelerate) - see stepMultFor()/
 * computeRate(). The pitched output feeds back into the same buffer, so
 * feedback carries the pitched repeats forward rather than running a
 * separate parallel path - Arp and Feedback compound naturally. No
 * crossfade at grain boundaries: a small discontinuity click each repeat
 * is in-character for this kit's raw/lo-fi aesthetic, not a bug to
 * engineer away.
 *
 * Speed is a flat rate multiplier applied to *every* grain regardless of
 * Arp - it works even at Arp=0 (a plain delay played back faster/slower,
 * like adjusting tape speed), and stacks with whatever the arp pattern
 * already computed.
 *
 * Ramp adds a *within-grain* rate change on top of that: instead of a
 * fixed rate for the grain's whole duration, the rate ramps linearly from
 * the grain's start rate up to (start rate * 2^(rampSemitones/12)) by the
 * end of that same grain - each individual repeat audibly speeds up as it
 * plays, like a tape/turntable gaining speed, independent of the
 * between-grain arpeggio stepping. Re-derived fresh every grain (the ramp
 * doesn't carry across grain boundaries).
 *
 * Every grain still starts exactly one delaySamples behind the write
 * pointer, same as plain Arp=0 delay - keeping each repeat an echo of what
 * was actually just played, not some more-distant, disconnected slice of
 * the buffer. A rate > 1 grain's read pointer gains on the write pointer
 * over the grain's duration, though, and for rate > 2 (more than an octave
 * up) it would fully catch up before the grain ends; past that point it
 * would wrap into old, not-yet-decayed feedback content instead of a clean
 * echo, which is what was turning a ~3s decay into an 18s+ tail. So the gap
 * is floored at kMinGrainGap: once a fast grain closes in that far, the
 * read pointer is pinned there for the rest of the grain (a short,
 * near-zero-latency tail rather than a wrap into stale history) instead of
 * being let through. This still applies unchanged with Speed/Ramp in the
 * mix - the clamp is evaluated every sample against whatever the
 * instantaneous rate is, so it doesn't care how that rate was derived.
 *
 * Jitter adds a small per-grain random detune on top of whatever rate the
 * steps/curve/direction/speed pattern already computed - a tiny xorshift32
 * PRNG, seeded distinctly per channel instance via setJitterSeed() (same
 * "seed each instance differently" convention DrumEngine.hpp uses for its
 * noise generators), so L/R jitter independently for a bit of natural
 * width. Applied once per grain (not re-rolled mid-ramp), so it shifts the
 * whole grain's rate range without changing the ramp's shape.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sideous {

enum class ArpCurve { Pitch = 0, Tempo = 1 };
enum class ArpDirection { Up = 0, Down = 1, UpDown = 2 };

class DelayLine
{
public:
    static constexpr float kMaxSeconds = 8.0f;

    // floor for how close a fast Arp grain's read pointer is allowed to get
    // to the write pointer (see class comment) - small enough to be a
    // near-inaudible slap rather than a real second pitch, large enough
    // that readInterpolated()'s +1 neighbor lookup never wraps in a way
    // that reads freshly-written (not yet feedback-processed) samples.
    static constexpr float kMinGrainGap = 32.0f;

    // Jitter knob at 1.0 allows up to +/-30 cents of random per-grain
    // detune - enough to read as an analog-ish wobble without meaningfully
    // detuning the arpeggio's musical intervals.
    static constexpr float kMaxJitterCents = 30.0f;

    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        fBufferSize = (int)(kMaxSeconds * fSampleRate) + 4;
        fBuffer.assign((size_t)fBufferSize, 0.0f);
        fWritePos = 0;
        resetGrain();
    }

    void setDelayTime(float seconds) noexcept
    {
        seconds = std::clamp(seconds, 0.001f, kMaxSeconds * 0.5f);
        fDelaySamples = seconds * (float)fSampleRate;
    }

    void setFeedback(float fb01) noexcept { fFeedback = std::clamp(fb01, 0.0f, 0.95f); }
    void setArpSemitones(float st) noexcept { fArpSemitones = std::clamp(st, 0.0f, 12.0f); }
    void setSpeed(float mult) noexcept { fSpeedMultiplier = std::clamp(mult, 0.25f, 4.0f); }
    void setRampSemitones(float st) noexcept { fRampSemitones = std::clamp(st, 0.0f, 12.0f); }
    void setArpSteps(int steps) noexcept { fArpSteps = std::clamp(steps, 2, 4); }
    void setArpCurve(ArpCurve curve) noexcept { fArpCurve = curve; }
    void setArpDirection(ArpDirection dir) noexcept { fArpDirection = dir; }
    void setJitterAmount(float amt01) noexcept { fJitterAmount = std::clamp(amt01, 0.0f, 1.0f); }
    void setJitterSeed(uint32_t seed) noexcept { fRngState = seed != 0 ? seed : 1u; }

    // exposes the raw per-grain cycling phase so EffectsEngine can derive
    // stereo ping-pong panning without this class needing to know which
    // stereo side it's on (stays mono/channel-agnostic, per the class
    // comment - the plugin just runs two independent instances).
    int getTapIndex() const noexcept { return fPhase; }

    float process(float in) noexcept
    {
        if (fBuffer.empty())
            return in;

        fGrainSample += 1.0f;
        if (fGrainSample >= fDelaySamples)
        {
            fGrainSample -= fDelaySamples;
            fGrainReadPos = wrapIndex((float)fWritePos - fDelaySamples);

            float baseRate;
            if (fArpSemitones > 0.0f)
            {
                const int period = periodFor(fArpSteps, fArpDirection);
                fPhase = (fPhase + 1) % period;
                const int stepMult = stepMultFor(fPhase, fArpSteps, fArpDirection);
                baseRate = computeRate(stepMult, fArpSteps, fArpSemitones, fArpCurve);
            }
            else
            {
                baseRate = 1.0f;
            }

            baseRate *= fSpeedMultiplier;

            if (fJitterAmount > 0.0f)
                baseRate *= std::exp2(nextRandomBipolar() * kMaxJitterCents * fJitterAmount / 1200.0f);

            fGrainRateStart = baseRate;
            fGrainRateEnd = fRampSemitones > 0.0f
                          ? baseRate * std::exp2(fRampSemitones / 12.0f)
                          : baseRate;
        }

        const float progress = fDelaySamples > 0.0f ? (fGrainSample / fDelaySamples) : 0.0f;
        const float rate = fGrainRateStart + (fGrainRateEnd - fGrainRateStart) * progress;

        const float outSample = readInterpolated(fGrainReadPos);
        float nextReadPos = wrapIndex(fGrainReadPos + rate);

        // floor the gap instead of letting a fast grain's read pointer
        // wrap past the write pointer into stale history (see class
        // comment) - engages whenever the instantaneous rate is fast
        // enough to close the gap before the grain ends, whether that's
        // from Arp, Speed, Ramp, or Jitter (or several stacked together).
        float gap = (float)fWritePos - nextReadPos;
        if (gap < 0.0f)
            gap += (float)fBufferSize;
        if (gap < kMinGrainGap)
            nextReadPos = wrapIndex((float)fWritePos - kMinGrainGap);

        fGrainReadPos = nextReadPos;

        fBuffer[(size_t)fWritePos] = in + outSample * fFeedback;
        fWritePos = (fWritePos + 1) % fBufferSize;

        return outSample;
    }

private:
    float readInterpolated(float pos) const noexcept
    {
        const int i0 = (int)pos;
        const int i1 = (i0 + 1) % fBufferSize;
        const float frac = pos - (float)i0;
        return fBuffer[(size_t)i0] + (fBuffer[(size_t)i1] - fBuffer[(size_t)i0]) * frac;
    }

    float wrapIndex(float pos) const noexcept
    {
        const float size = (float)fBufferSize;
        while (pos < 0.0f) pos += size;
        while (pos >= size) pos -= size;
        return pos;
    }

    void resetGrain() noexcept
    {
        fGrainSample = 0.0f;
        fGrainReadPos = 0.0f;
        fGrainRateStart = 1.0f;
        fGrainRateEnd = 1.0f;
        fPhase = 0;
    }

    // real-time length (in grains) of one full arp cycle before it repeats
    static int periodFor(int steps, ArpDirection dir) noexcept
    {
        if (dir == ArpDirection::UpDown)
            return steps <= 2 ? steps : 2 * (steps - 1);
        return steps;
    }

    // signed step-count for the current phase within the cycle - e.g. Up
    // with steps=3 gives 0,1,2,repeat; Down gives 0,-1,-2,repeat; Up-Down
    // with steps=3 gives 0,1,2,1,repeat (triangle-wave over phase)
    static int stepMultFor(int phase, int steps, ArpDirection dir) noexcept
    {
        switch (dir)
        {
        case ArpDirection::Down:
            return -phase;
        case ArpDirection::UpDown:
            return phase < steps - 1 ? phase : 2 * (steps - 1) - phase;
        case ArpDirection::Up:
        default:
            return phase;
        }
    }

    // playback rate for a given signed step-count. Pitch curve: equal
    // semitone steps (today's formula, `2^(stepMult*semitonesPerStep/12)`).
    // Tempo curve: rate interpolates linearly from 1.0 toward the same
    // endpoint the Pitch curve reaches at the extreme tap in this
    // direction - both curves agree at the extreme tap, Tempo just spaces
    // the taps in between evenly in speed instead of in pitch.
    static float computeRate(int stepMult, int steps, float semitonesPerStep, ArpCurve curve) noexcept
    {
        if (stepMult == 0)
            return 1.0f;

        if (curve == ArpCurve::Pitch)
            return std::exp2((float)stepMult * semitonesPerStep / 12.0f);

        const int maxMult = steps - 1;
        if (maxMult <= 0)
            return 1.0f;
        const int extremeMult = stepMult > 0 ? maxMult : -maxMult;
        const float extremeRate = std::exp2((float)extremeMult * semitonesPerStep / 12.0f);
        const float t = (float)stepMult / (float)extremeMult; // always positive: same sign as extremeMult
        return 1.0f + t * (extremeRate - 1.0f);
    }

    float nextRandomBipolar() noexcept
    {
        fRngState ^= fRngState << 13;
        fRngState ^= fRngState >> 17;
        fRngState ^= fRngState << 5;
        return ((float)(fRngState & 0x00FFFFFFu) / (float)0x00FFFFFFu) * 2.0f - 1.0f;
    }

    double fSampleRate = 44100.0;
    std::vector<float> fBuffer;
    int fBufferSize = 0;
    int fWritePos = 0;

    float fDelaySamples = 22050.0f;
    float fFeedback = 0.0f;
    float fArpSemitones = 0.0f;
    float fSpeedMultiplier = 1.0f;
    float fRampSemitones = 0.0f;
    int fArpSteps = 3;
    ArpCurve fArpCurve = ArpCurve::Pitch;
    ArpDirection fArpDirection = ArpDirection::Up;
    float fJitterAmount = 0.0f;
    uint32_t fRngState = 0x9E3779B9u;

    float fGrainSample = 0.0f;
    float fGrainReadPos = 0.0f;
    float fGrainRateStart = 1.0f;
    float fGrainRateEnd = 1.0f;
    int fPhase = 0;
};

} // namespace sideous
