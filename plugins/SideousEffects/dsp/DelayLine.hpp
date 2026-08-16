/*
 * Sideous Effects - tempo-synced feedback delay with an optional
 * "chip-arpeggiated" mode. Mono; the plugin runs one instance per channel.
 *
 * Normal operation (Arp = 0): a standard circular-buffer feedback delay -
 * read pointer trails the write pointer by exactly `delaySamples`.
 *
 * Arp mode (Arp > 0, semitones): thought of in "grains" instead - each
 * grain is exactly one repeat-length (`delaySamples`) long. A grain reads
 * the buffer at a fixed *rate* for its whole duration (linear-interpolated
 * fractional index); the rate cycles through a repeating 3-step pattern
 * once per grain - {1.0, 2^(step/12), 2^(2*step/12)} - so it reads as a
 * genuine repeating arpeggio (root, up a step, up two steps, repeat)
 * instead of an ever-rising/falling pitch drift. The pitched output feeds
 * back into the same buffer, so feedback carries the pitched repeats
 * forward rather than running a separate parallel path - Arp and Feedback
 * compound naturally. No crossfade at grain boundaries: a small
 * discontinuity click each repeat is in-character for this kit's raw/lo-fi
 * aesthetic, not a bug to engineer away.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace sideous {

class DelayLine
{
public:
    // generous enough for any sane Sync division at any sane tempo, with
    // margin for Arp mode's grain read rate temporarily outrunning the
    // write pointer (see the class comment - a rate-4x grain over a long
    // delay time can reach back further than delaySamples alone would need)
    static constexpr float kMaxSeconds = 8.0f;

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

    float process(float in) noexcept
    {
        if (fBuffer.empty())
            return in;

        fGrainSample += 1.0f;
        if (fGrainSample >= fDelaySamples)
        {
            fGrainSample -= fDelaySamples;
            fGrainReadPos = wrapIndex((float)fWritePos - fDelaySamples);
            fTapIndex = (fTapIndex + 1) % 3;
            fGrainRate = fArpSemitones > 0.0f
                       ? std::exp2((float)fTapIndex * fArpSemitones / 12.0f)
                       : 1.0f;
        }

        const float outSample = readInterpolated(fGrainReadPos);
        fGrainReadPos = wrapIndex(fGrainReadPos + fGrainRate);

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
        fGrainRate = 1.0f;
        fTapIndex = 0;
    }

    double fSampleRate = 44100.0;
    std::vector<float> fBuffer;
    int fBufferSize = 0;
    int fWritePos = 0;

    float fDelaySamples = 22050.0f;
    float fFeedback = 0.0f;
    float fArpSemitones = 0.0f;

    float fGrainSample = 0.0f;
    float fGrainReadPos = 0.0f;
    float fGrainRate = 1.0f;
    int fTapIndex = 0;
};

} // namespace sideous
