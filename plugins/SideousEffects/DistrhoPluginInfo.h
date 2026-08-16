#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Sideous"
#define DISTRHO_PLUGIN_NAME    "Sideous Effects"
#define DISTRHO_PLUGIN_URI     "https://github.com/sideous/sideous-effects"
#define DISTRHO_PLUGIN_CLAP_ID "sideous.sideouseffects"

#define DISTRHO_PLUGIN_BRAND_ID  Side
#define DISTRHO_PLUGIN_UNIQUE_ID Efx1

#define DISTRHO_PLUGIN_HAS_UI           1
#define DISTRHO_PLUGIN_IS_RT_SAFE       1
#define DISTRHO_PLUGIN_IS_SYNTH         0
#define DISTRHO_PLUGIN_NUM_INPUTS       2
#define DISTRHO_PLUGIN_NUM_OUTPUTS      2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  0
#define DISTRHO_PLUGIN_WANT_STATE       0
#define DISTRHO_PLUGIN_WANT_PROGRAMS    0
// needed for the tempo-synced Delay - reads host BPM the same way
// sideous/SideousPlugin.cpp does for its LFO/Arp sync, no MIDI involved
#define DISTRHO_PLUGIN_WANT_TIMEPOS     1

#define DISTRHO_UI_USE_CAIRO             1
#define DISTRHO_UI_DEFAULT_WIDTH         900
#define DISTRHO_UI_DEFAULT_HEIGHT        420
// must match the height ui/UIPainter.hpp's buildLayout() actually lays out to
// (verified via Xvfb screenshot) - keep these two in sync by hand.

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
