/*
 * Sideous Effects - wavefolder: gains the input then reflects it back into
 * -1..1 at each boundary crossing (classic "West Coast" triangle-fold),
 * instead of clipping - each reflection adds harmonics, so louder input/
 * higher Drive means more folds means a denser, more inharmonic timbre
 * rather than just flat-topped clipping.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace sideous {

class Fold
{
public:
    void setDrive(float drive01) noexcept { fDrive = std::clamp(drive01, 0.0f, 1.0f); }

    float process(float in) noexcept
    {
        float x = in * (1.0f + fDrive * 9.0f);

        // bounded iteration count: pathological input could in principle
        // keep bouncing between the two boundaries indefinitely under
        // floating-point rounding, this guarantees the audio thread can
        // never spin
        for (int i = 0; i < 16 && (x > 1.0f || x < -1.0f); ++i)
        {
            if (x > 1.0f)
                x = 2.0f - x;
            else if (x < -1.0f)
                x = -2.0f - x;
        }

        return std::clamp(x, -1.0f, 1.0f);
    }

private:
    float fDrive = 0.0f;
};

} // namespace sideous
