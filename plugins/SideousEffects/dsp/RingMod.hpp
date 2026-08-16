/*
 * Sideous Effects - ring modulator: multiplies the input by a free-running
 * carrier oscillator (Oscillator.hpp, same PolyBLEP saw/pulse/triangle used
 * throughout the family). Straight amplitude multiplication, no filtering -
 * the inharmonic sum/difference sidebands it produces *are* the effect.
 */

#pragma once

#include "Oscillator.hpp"

namespace sideous {

class RingMod
{
public:
    void setSampleRate(double sampleRate) noexcept { fOsc.setSampleRate(sampleRate); }
    void setWaveform(Waveform w) noexcept { fOsc.setWaveform(w); }
    void setFrequency(float hz) noexcept { fOsc.setFrequency(hz); }

    float process(float in) noexcept
    {
        return in * fOsc.process();
    }

private:
    Oscillator fOsc;
};

} // namespace sideous
