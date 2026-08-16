/*
 * Sideous Effects - shared parameter definitions used by both the DSP
 * plugin side and the UI, so ranges/units/labels can't drift between the
 * two. Same pattern as every sibling's Params.hpp. All Mix knobs default to
 * 0 so a freshly-loaded instance is a clean pass-through - the correct
 * default for an effect, unlike an instrument.
 */

#pragma once

#include <cstdint>

namespace sideous {

enum Params : uint32_t {
    kParamBitcrushBits = 0,
    kParamBitcrushRate,
    kParamBitcrushMix,

    kParamFoldDrive,
    kParamFoldMix,

    kParamRingModWave,
    kParamRingModFreq,
    kParamRingModMix,

    kParamLofiCutoff,
    kParamLofiReso,
    kParamLofiMix,

    kParamDelaySync,
    kParamDelayFeedback,
    kParamDelayArp,
    kParamDelayMix,

    kParamMasterOutput,
    kParamMasterDrive,

    kParamCount
};

enum class ParamShape { Linear, Logarithmic };

struct ParamInfo
{
    const char* name;
    const char* symbol;
    const char* unit;
    float min;
    float max;
    float def;
    ParamShape shape;
};

inline const ParamInfo& getParamInfo(uint32_t index) noexcept
{
    static constexpr ParamInfo table[kParamCount] = {
        { "Bitcrush Bits", "bitcrush_bits", "",   1.0f,   16.0f,   8.0f,  ParamShape::Linear },
        { "Bitcrush Rate", "bitcrush_rate", "x",  1.0f,   40.0f,   6.0f,  ParamShape::Logarithmic },
        { "Bitcrush Mix",  "bitcrush_mix",  "",   0.0f,   1.0f,    0.0f,  ParamShape::Linear },

        { "Fold Drive", "fold_drive", "",   0.0f,   1.0f,    0.4f, ParamShape::Linear },
        { "Fold Mix",   "fold_mix",   "",   0.0f,   1.0f,    0.0f, ParamShape::Linear },

        { "Ring Mod Wave", "ringmod_wave", "",   0.0f,     2.0f,    1.0f,  ParamShape::Linear },
        { "Ring Mod Freq", "ringmod_freq", "Hz", 20.0f,  4000.0f,  200.0f, ParamShape::Logarithmic },
        { "Ring Mod Mix",  "ringmod_mix",  "",   0.0f,     1.0f,    0.0f,  ParamShape::Linear },

        { "Lo-fi Cutoff", "lofi_cutoff", "Hz",  200.0f, 15000.0f, 3000.0f, ParamShape::Logarithmic },
        { "Lo-fi Reso",   "lofi_reso",   "",    0.0f,   1.0f,     0.15f,   ParamShape::Linear },
        { "Lo-fi Mix",    "lofi_mix",    "",    0.0f,   1.0f,     0.0f,    ParamShape::Linear },

        { "Delay Sync",     "delay_sync",     "",  0.0f, 16.0f, 9.0f,  ParamShape::Linear },
        { "Delay Feedback", "delay_feedback", "",  0.0f, 0.95f, 0.35f, ParamShape::Linear },
        { "Delay Arp",      "delay_arp",      "st",0.0f, 12.0f, 0.0f,  ParamShape::Linear },
        { "Delay Mix",      "delay_mix",      "",  0.0f, 1.0f,  0.0f,  ParamShape::Linear },

        { "Master Output", "master_output", "", 0.0f, 1.0f, 0.85f, ParamShape::Linear },
        { "Master Drive",  "master_drive",  "", 0.0f, 1.0f, 0.0f,  ParamShape::Linear },
    };
    return table[index];
}

} // namespace sideous
