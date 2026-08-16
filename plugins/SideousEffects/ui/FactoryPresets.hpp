/*
 * Sideous Effects - a handful of factory example presets, seeded into the
 * user's preset directory (see ui/PresetStore.hpp) the first time the UI
 * ever runs with an empty preset library. Compiled in (rather than shipped
 * as separate data files) so they exist identically across every build/
 * install of the plugin regardless of platform or plugin format - no
 * bundle-path discovery needed, they just get written out as ordinary,
 * user-editable ".sdpreset" files on first run and behave exactly like any
 * preset the user saves themselves from that point on. Same pattern as
 * every sibling's FactoryPresets.hpp.
 */

#pragma once

#include "PresetStore.hpp"

#include <cstring>
#include <utility>
#include <vector>

namespace sideous {
namespace ui {

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> overrides; // symbol -> value; everything else stays at that param's Params.hpp default
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        // narrow, muffled band + heavy quantization noise - an old phone line
        { "Telephone Crush", {
            { "bitcrush_bits", 5.0f }, { "bitcrush_rate", 8.0f }, { "bitcrush_mix", 0.8f },
            { "lofi_cutoff", 1200.0f }, { "lofi_reso", 0.3f }, { "lofi_mix", 0.9f },
            { "master_drive", 0.15f },
        }},
        // ring-modulated buzz over a slightly crushed signal - a classic
        // "robot speaking" texture
        { "Robot Voice", {
            { "ringmod_wave", 1.0f }, { "ringmod_freq", 120.0f }, { "ringmod_mix", 0.7f },
            { "bitcrush_bits", 8.0f }, { "bitcrush_rate", 2.0f }, { "bitcrush_mix", 0.2f },
        }},
        // the chip-arpeggiated delay mode front and center, with a touch of
        // crush to keep the repeats gritty
        { "Arcade Echo", {
            { "delay_sync", 9.0f }, { "delay_feedback", 0.55f }, { "delay_arp", 7.0f }, { "delay_mix", 0.6f },
            { "bitcrush_bits", 6.0f }, { "bitcrush_rate", 4.0f }, { "bitcrush_mix", 0.3f },
        }},
        // heavy wavefolder drive plus a dulled top end - a blown-out little
        // speaker cone
        { "Broken Speaker", {
            { "fold_drive", 0.9f }, { "fold_mix", 0.7f }, { "master_drive", 0.6f },
            { "bitcrush_bits", 6.0f }, { "bitcrush_mix", 0.15f },
            { "lofi_cutoff", 4000.0f }, { "lofi_mix", 0.2f },
        }},
        // gentle lo-fi filtering with a soft, slightly-detuning delay - a
        // worn cassette losing pitch stability
        { "Tape Warble", {
            { "lofi_cutoff", 2500.0f }, { "lofi_reso", 0.4f }, { "lofi_mix", 0.5f },
            { "delay_sync", 6.0f }, { "delay_feedback", 0.3f }, { "delay_arp", 1.0f }, { "delay_mix", 0.35f },
            { "master_drive", 0.1f },
        }},
    };
    return presets;
}

inline void seedFactoryPresets()
{
    for (const FactoryPreset& fp : factoryPresets())
    {
        float values[kParamCount];
        for (uint32_t i = 0; i < kParamCount; ++i)
            values[i] = getParamInfo(i).def;

        for (const auto& kv : fp.overrides)
        {
            for (uint32_t i = 0; i < kParamCount; ++i)
            {
                if (std::strcmp(getParamInfo(i).symbol, kv.first) == 0)
                {
                    values[i] = kv.second;
                    break;
                }
            }
        }

        savePreset(fp.name, values);
    }
}

} // namespace ui
} // namespace sideous
