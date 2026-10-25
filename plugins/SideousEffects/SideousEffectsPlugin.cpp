/*
 * Sideous Effects - a raw, chip-vibe audio effects box: Bitcrusher, Fold
 * (wavefolder), Ring Mod, Lo-fi Filter, and a tempo-synced Delay with a
 * "chip-arpeggiated" mode, chained in that fixed order. See
 * EffectsEngine.hpp for the actual signal chain. Stereo audio in/out, no
 * MIDI - the first pure-effect plugin in the family (every sibling so far
 * is an instrument).
 */

#include "DistrhoPlugin.hpp"
#include "Params.hpp"
#include "EffectsEngine.hpp"

START_NAMESPACE_DISTRHO

using namespace sideous;

// -----------------------------------------------------------------------------------------------------------

class SideousEffectsPlugin : public Plugin
{
public:
    SideousEffectsPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        fEngine.setSampleRate(getSampleRate());
    }

protected:
    // ---------------------------------------------------------------------
    // Information

    const char* getLabel() const override { return "SideousEffects"; }
    const char* getDescription() const override
    {
        return "Raw SID/POKEY-chip-style effects box: bitcrusher, wavefolder, ring modulator, "
               "lo-fi filter, and a tempo-synced delay with a chip-arpeggiated repeat mode.";
    }
    const char* getMaker() const override { return "Sideous"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    // ---------------------------------------------------------------------
    // Init

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        const ParamInfo& info = getParamInfo(index);

        parameter.name = info.name;
        parameter.symbol = info.symbol;
        parameter.unit = info.unit;
        parameter.ranges.min = info.min;
        parameter.ranges.max = info.max;
        parameter.ranges.def = info.def;

        // log-curved parameters reach the host as a 0..100 % position (see
        // paramToHost() in Params.hpp), so its automation sweeps them evenly
        if (isHostPosition(index))
        {
            parameter.unit = "%";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 100.0f;
            parameter.ranges.def = paramToHost(index, info.def);
        }
        parameter.hints = kParameterIsAutomatable;
    }

    // ---------------------------------------------------------------------
    // Internal data

    float getParameterValue(uint32_t index) const override
    {
        return paramToHost(index, getEngineeringValue(index));
    }

    // a parameter in real units (Hz, s, ...), as the DSP sees it
    float getEngineeringValue(uint32_t index) const
    {
        return fEngine.getParameterValue(index);
    }

    void setParameterValue(uint32_t index, float value) override
    {
        value = paramFromHost(index, value); // host position -> real units (Params.hpp)

        fEngine.setParameterValue(index, value);
    }

    // ---------------------------------------------------------------------
    // Audio Processing

    void activate() override
    {
        fEngine.setSampleRate(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fEngine.setSampleRate(newSampleRate);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const float* inL = inputs[0];
        const float* inR = inputs[1];
        float* outL = outputs[0];
        float* outR = outputs[1];

        const TimePosition& timePos = getTimePosition();
        const double bpm = timePos.bbt.valid && timePos.bbt.beatsPerMinute > 0.0
                          ? timePos.bbt.beatsPerMinute : 120.0;
        fEngine.setBpm(bpm);

        for (uint32_t frame = 0; frame < frames; ++frame)
            fEngine.process(inL[frame], inR[frame], outL[frame], outR[frame]);
    }

private:
    EffectsEngine fEngine;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideousEffectsPlugin)
};

// -----------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new SideousEffectsPlugin();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
