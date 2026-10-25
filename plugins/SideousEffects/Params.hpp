/*
 * Sideous Effects - shared parameter definitions used by both the DSP
 * plugin side and the UI, so ranges/units/labels can't drift between the
 * two. Same pattern as every sibling's Params.hpp. All Mix knobs default to
 * 0 so a freshly-loaded instance is a clean pass-through - the correct
 * default for an effect, unlike an instrument.
 */

#pragma once

#include <algorithm>
#include <cmath>
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
    kParamDelaySpeed,
    kParamDelayRamp,
    kParamDelayArpSteps,
    kParamDelayArpCurve,
    kParamDelayArpDirection,
    kParamDelayPingPong,
    kParamDelayJitter,
    kParamDelayMix,

    kParamMasterOutput,
    kParamMasterDrive,

    // chain routing - one "position in the signal chain" slot per
    // reorderable stage (Master is not reorderable, it's always last).
    // Processing order is derived by stable-sorting these 5 by value (see
    // resolveChainOrder() below), not stored as an explicit permutation -
    // so there's no "invalid" state a host/preset could put it in.
    kParamChainPosBitcrush,
    kParamChainPosFold,
    kParamChainPosRingMod,
    kParamChainPosLofi,
    kParamChainPosDelay,

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

        { "Delay Sync",          "delay_sync",          "",   0.0f, 16.0f, 9.0f,  ParamShape::Linear },
        { "Delay Feedback",      "delay_feedback",       "",   0.0f, 0.95f, 0.35f, ParamShape::Linear },
        { "Delay Arp",           "delay_arp",             "st", 0.0f, 12.0f, 0.0f,  ParamShape::Linear },
        { "Delay Speed",         "delay_speed",           "x",  0.25f, 4.0f, 1.0f,  ParamShape::Logarithmic },
        { "Delay Ramp",          "delay_ramp",            "st", 0.0f, 12.0f, 0.0f,  ParamShape::Linear },
        { "Delay Arp Steps",     "delay_arp_steps",       "",   2.0f, 4.0f,  3.0f,  ParamShape::Linear },
        { "Delay Arp Curve",     "delay_arp_curve",       "",   0.0f, 1.0f,  0.0f,  ParamShape::Linear },
        { "Delay Arp Direction", "delay_arp_direction",   "",   0.0f, 2.0f,  0.0f,  ParamShape::Linear },
        { "Delay Ping-Pong",     "delay_pingpong",        "",   0.0f, 1.0f,  0.0f,  ParamShape::Linear },
        { "Delay Jitter",        "delay_jitter",          "",   0.0f, 1.0f,  0.0f,  ParamShape::Linear },
        { "Delay Mix",           "delay_mix",              "",   0.0f, 1.0f,  0.0f,  ParamShape::Linear },

        { "Master Output", "master_output", "", 0.0f, 1.0f, 0.85f, ParamShape::Linear },
        { "Master Drive",  "master_drive",  "", 0.0f, 1.0f, 0.0f,  ParamShape::Linear },

        { "Chain Position: Bitcrush", "chain_pos_bitcrush", "", 0.0f, 4.0f, 0.0f, ParamShape::Linear },
        { "Chain Position: Fold",     "chain_pos_fold",     "", 0.0f, 4.0f, 1.0f, ParamShape::Linear },
        { "Chain Position: Ring Mod", "chain_pos_ringmod",  "", 0.0f, 4.0f, 2.0f, ParamShape::Linear },
        { "Chain Position: Lo-fi",    "chain_pos_lofi",     "", 0.0f, 4.0f, 3.0f, ParamShape::Linear },
        { "Chain Position: Delay",    "chain_pos_delay",    "", 0.0f, 4.0f, 4.0f, ParamShape::Linear },
    };
    return table[index];
}

// Hosts automate a parameter's range linearly (VST3 normalisation, CLAP
// plain values; DPF passes the logarithmic hint on to LV2 only), which crams
// most of a 20 Hz..20 kHz or 1 ms..5 s range into the first few percent of an
// automation lane. So every Logarithmic parameter is exposed to the host as
// a 0..100 % position along its log curve (for a cutoff, ~10 % per octave).
// The DSP, the UI and preset files keep working in real units: the plugin
// and the UI convert only where values cross to and from the host.
inline bool isHostPosition(uint32_t index) noexcept
{
    return index < kParamCount && getParamInfo(index).shape == ParamShape::Logarithmic;
}

inline float paramToHost(uint32_t index, float value) noexcept
{
    if (!isHostPosition(index))
        return value;
    const ParamInfo& info = getParamInfo(index);
    const float lo = std::max(info.min, 1e-6f);
    const float v = std::clamp(value, lo, info.max);
    return 100.0f * std::log(v / lo) / std::log(info.max / lo);
}

inline float paramFromHost(uint32_t index, float value) noexcept
{
    if (!isHostPosition(index))
        return value;
    const ParamInfo& info = getParamInfo(index);
    const float lo = std::max(info.min, 1e-6f);
    return lo * std::pow(info.max / lo, std::clamp(value, 0.0f, 100.0f) * 0.01f);
}

// -----------------------------------------------------------------------------------------------------------
// chain order

// the 5 reorderable tonal stages - Master (Drive/Output) is not part of
// this, it's always the final stage after the loop
enum StageKind { kStageBitcrush = 0, kStageFold, kStageRingMod, kStageLofi, kStageDelay, kStageCount };

// stable-sorts the 5 stages by their ChainPos* param value into `order` -
// shared by EffectsEngine.hpp (DSP side) and UIPainter.hpp (UI side), which
// both already include this header, so the two can't disagree on ordering.
// Plain insertion sort: kStageCount is fixed at 5, not worth pulling in
// <algorithm> for, and insertion sort is trivially stable (equal-valued
// stages keep their relative order rather than swapping past each other).
inline void resolveChainOrder(const float* values, int order[kStageCount]) noexcept
{
    struct Ranked { float pos; int stage; };
    Ranked ranked[kStageCount] = {
        { values[kParamChainPosBitcrush], kStageBitcrush },
        { values[kParamChainPosFold],     kStageFold },
        { values[kParamChainPosRingMod],  kStageRingMod },
        { values[kParamChainPosLofi],     kStageLofi },
        { values[kParamChainPosDelay],    kStageDelay },
    };
    for (int i = 1; i < kStageCount; ++i)
    {
        Ranked key = ranked[i];
        int j = i - 1;
        while (j >= 0 && ranked[j].pos > key.pos)
        {
            ranked[j + 1] = ranked[j];
            --j;
        }
        ranked[j + 1] = key;
    }
    for (int i = 0; i < kStageCount; ++i)
        order[i] = ranked[i].stage;
}

} // namespace sideous
