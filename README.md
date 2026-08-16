# Sideous Effects

> **⚠️ VIBE CODED SLOP.** This entire plugin — DSP, GUI — was built through
> conversational back-and-forth with an AI, not hand-engineered from a spec.
> It works, it's been tested, but go in with appropriate expectations.

A raw, chip-vibe audio effects box (VST3 / LV2 / CLAP / JACK standalone),
built on [DPF](https://github.com/DISTRHO/DPF) with a hand-drawn Cairo-based
UI — the first pure-effect plugin in the Sideous family (every sibling so
far is an instrument). Six stages chained in a fixed order: Bitcrusher,
Wavefolder, Ring Modulator, Lo-fi Filter, a tempo-synced Delay with a
"chip-arpeggiated" repeat mode, and a Master drive/output stage. "Broken
cassette deck" palette instead of any sibling's brighter retro-synth or
neon-chip look.

![screenshot](docs/screenshot.png)

## Signal chain

input → Bitcrusher → Wavefolder → Ring Mod → Lo-fi Filter → Delay →
Master Drive → Master Output → output. No reordering, no per-stage bypass
switches - every stage has its own **Mix** knob (dry/wet blend into the
next stage), and **every Mix knob defaults to 0**, so a freshly-loaded
instance is a clean pass-through until you dial a stage in.

- **BITCRUSH** - Bits (1-16, bit-depth quantization), Rate (1-40x
  sample-and-hold downsample factor), Mix.
- **FOLD** - a wavefolder: Drive (pre-gain into a fold-back waveshaper),
  Mix.
- **RING MOD** - Wave (Saw/Pulse/Triangle carrier), Freq (20-4000Hz),
  Mix.
- **LOFI FILTER** - a resonant lowpass: Cutoff (200-15000Hz), Reso, Mix.
- **DELAY** - Sync (tempo-locked divisions, Free + 16 divisions incl.
  dotted/triplet down to 1/128), Feedback, Arp (0-12 semitones - see
  below), Mix.
- **MASTER** - Output level, Drive (post saturation stage).

## Chip-arpeggiated delay

Turning the Delay's **Arp** knob above 0 changes how the delay repeats.
Normally (Arp = 0) it's a plain fixed-lag feedback delay. With Arp > 0,
each repeat plays back at a fixed pitch for its whole length, cycling
through a repeating 3-step arpeggio (root, up a step, up two steps, repeat)
- Arp sets the step size in semitones. The pitched repeats feed back into
the same buffer, so Feedback and Arp compound naturally: low feedback gives
a few pitched slaps, high feedback turns into a rising/falling arpeggio
trail. There's no crossfading at grain boundaries - a small click on each
repeat is in-character for this box's raw/lo-fi aesthetic, not a bug.

## Presets

A small file-based preset library, entirely UI-side so it works identically
across every host/format including the JACK standalone: `<` / `>` step
through presets, click the name field to rename, `SAVE`/`DELETE` manage the
library. Presets are plain-text `.sdpreset` files (one `symbol=value` line
per parameter) in `~/.local/share/sideous-effects/presets` (Linux/XDG),
`~/Library/Application Support/sideous-effects/presets` (macOS), or
`%APPDATA%\sideous-effects\presets` (Windows).

Five factory presets (Telephone Crush, Robot Voice, Arcade Echo, Broken
Speaker, Tape Warble) are seeded into that folder the first time the UI runs
with an empty library - from then on they're just ordinary presets, editable
and deletable like anything you save yourself.

## Building

```sh
git clone --recursive <repo-url>
cd sideous-effects
./install.sh --all
```

(If you already cloned without `--recursive`, run
`git submodule update --init --recursive` first.)

Built plugins land in `build/bin/`:
`sideous-effects.vst3` / `.lv2` / `.clap` / `sideous-effects` (JACK/native
standalone).
