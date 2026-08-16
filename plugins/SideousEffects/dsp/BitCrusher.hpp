/*
 * Sideous Effects - bit-depth + sample-rate reduction, the classic digital
 * "crunch" pair. Bits quantizes amplitude to 2^bits levels; Rate holds each
 * sample for N samples (sample-and-hold downsampling), producing the
 * stair-stepped aliasing character of a low-resolution chip's DAC.
 */

#pragma once

#include <cmath>
#include <algorithm>

namespace sideous {

class BitCrusher
{
public:
    void setBits(float bits) noexcept { fBits = std::clamp(bits, 1.0f, 16.0f); }
    void setRate(float holdSamples) noexcept
    {
        fHoldSamples = std::max(1, (int)std::lround(holdSamples));
    }

    float process(float in) noexcept
    {
        if (fCounter == 0)
        {
            const float levels = std::exp2(fBits);
            const float step = 2.0f / levels;
            fHeld = std::round(in / step) * step;
            fHeld = std::clamp(fHeld, -1.0f, 1.0f);
        }
        fCounter = (fCounter + 1) % fHoldSamples;
        return fHeld;
    }

private:
    float fBits = 8.0f;
    int fHoldSamples = 1;
    int fCounter = 0;
    float fHeld = 0.0f;
};

} // namespace sideous
