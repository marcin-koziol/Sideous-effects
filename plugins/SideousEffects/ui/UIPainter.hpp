/*
 * Sideous Effects - GUI layout and Cairo painting, ported from sideous's
 * UIPainter.hpp: same Knob/Selector/Dropdown widget model, same
 * paintKnob/paintSelector/paintDropdown and hit-test-rect helpers, same
 * preset bar.
 *
 * Laid out as a 2-column x 3-row grid: the 5 reorderable tonal stages
 * (Bitcrush, Fold, Ring Mod, Lofi Filter, Delay) fill cells 0-4 in
 * signal-flow reading order (top-left -> top-right -> mid-left ->
 * mid-right -> bottom-left) according to their current chain position (see
 * Params.hpp's resolveChainOrder()), and can be dragged by their header
 * strip to reorder the chain. Master (Drive/Output) is pinned to cell 5
 * (bottom-right) always - it's the output gain stage, not a reorderable
 * block. All 6 cells are uniformly sized, big enough for Delay's control
 * count (the densest stage), since any stage can land in any cell.
 *
 * "Broken cassette deck" palette - muted, dusty warm greys/ambers instead of
 * any sibling's brighter retro-synth or neon-chip look, fitting a lo-fi
 * effects box rather than an instrument.
 */

#pragma once

#include <cairo.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

#include "../Params.hpp"

namespace sideous {
namespace ui {

// -----------------------------------------------------------------------------------------------------------
// palette - "broken cassette deck": muted, dusty warm greys/ambers

struct Color { double r, g, b; };

static constexpr Color kBg        { 0.050, 0.045, 0.040 };
static constexpr Color kPanelBg   { 0.110, 0.100, 0.090 };
static constexpr Color kPanelEdge { 0.45,  0.38,  0.28  };
static constexpr Color kTextMain  { 0.88,  0.84,  0.76  };
static constexpr Color kTextDim   { 0.50,  0.45,  0.38  };
static constexpr Color kKnobBody  { 0.070, 0.060, 0.055 };
static constexpr Color kKnobRing  { 0.28,  0.24,  0.20  };
static constexpr Color kBtnBg     { 0.130, 0.115, 0.100 };

// per-panel accents - dusty/desaturated rather than neon, but still six
// distinct hues so the six stages read apart at a glance
static constexpr Color kBitcrushAccent { 0.85, 0.55, 0.20 }; // dusty amber (VU meter)
static constexpr Color kFoldAccent     { 0.80, 0.35, 0.25 }; // muted rust
static constexpr Color kRingModAccent  { 0.35, 0.65, 0.60 }; // faded teal
static constexpr Color kLofiAccent     { 0.80, 0.70, 0.30 }; // dusty mustard
static constexpr Color kDelayAccent    { 0.60, 0.50, 0.65 }; // muted violet-grey
static constexpr Color kMasterAccent   { 0.85, 0.80, 0.70 }; // warm tape-white

inline void setColor(cairo_t* cr, Color c, double a = 1.0) noexcept
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

// -----------------------------------------------------------------------------------------------------------
// layout model

// height of a panel's title strip - also the drag-handle hit zone for the
// 5 reorderable stage panels (Master's header doesn't respond to drag)
static constexpr float kPanelHeaderH = 28.0f;

struct Knob
{
    uint32_t param;
    float cx, cy, radius;
    const char* label;
    Color accent;
};

struct SelectorOption
{
    float value;
    const char* label;
};

struct Selector
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
    const char* caption = nullptr;
};

// like a Selector, but the options are hidden behind a closed combo-box
// until clicked open - used where there are too many options for a button
// row (Delay's Sync control, 17 entries)
struct Dropdown
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
    const char* caption = nullptr;
};

struct PanelBox
{
    float x, y, w, h;
    const char* title;
    Color accent;
    bool draggable = false; // true for the 5 reorderable stage panels
    int stageIndex = -1;    // StageKind (see Params.hpp), -1 for Master
};

// preset bar: prev/next/save/delete buttons plus a name field. Deliberately
// plain rectangles rather than reusing the Selector/Knob widgets - it isn't
// a parameter, it's file-system CRUD.
struct Button { float x, y, w, h; const char* label; Color accent; };

struct PresetBarLayout
{
    Button prev, next, save, del, scale; // scale = UI scale (UiScale.hpp), label set at paint time
    float nameX, nameY, nameW, nameH;
    Color accent;
};

struct Layout
{
    float width, height;
    std::vector<PanelBox> panels; // 6 entries: [0..4] reorderable stages in current cell order, [5] Master
    std::vector<Knob> knobs;
    std::vector<Selector> selectors;
    std::vector<Dropdown> dropdowns;
    PresetBarLayout presetBar;
};

// -----------------------------------------------------------------------------------------------------------
// layout builder

inline void addKnobRow(std::vector<Knob>& knobs, float panelX, float panelW, float rowY, float radius,
                        Color accent, std::initializer_list<std::pair<uint32_t, const char*>> items)
{
    const float slot = panelW / (float)items.size();
    size_t i = 0;
    for (const auto& item : items)
    {
        const float cx = panelX + slot * ((float)i + 0.5f);
        knobs.push_back({ item.first, cx, rowY, radius, item.second, accent });
        ++i;
    }
}

// same tempo-division set as sideous's own LFO/Arp sync dropdowns, copied
// verbatim (see dsp/Sync.hpp) - 0 = Free (a fixed fallback time, this plugin
// has no separate free-running Time knob), 1..16 = tempo note divisions
inline std::vector<SelectorOption> syncOptions()
{
    return {
        {  0.0f, "Free" },
        {  1.0f, "1/1" },
        {  2.0f, "1/2." },
        {  3.0f, "1/2" },
        {  4.0f, "1/2T" },
        {  5.0f, "1/4." },
        {  6.0f, "1/4" },
        {  7.0f, "1/4T" },
        {  8.0f, "1/8." },
        {  9.0f, "1/8" },
        { 10.0f, "1/8T" },
        { 11.0f, "1/16." },
        { 12.0f, "1/16" },
        { 13.0f, "1/16T" },
        { 14.0f, "1/32" },
        { 15.0f, "1/64" },
        { 16.0f, "1/128" },
    };
}

inline Layout buildLayout(float width, float height, const float* values)
{
    Layout L;
    L.width = width;
    L.height = height;

    const float margin = 12.0f;
    const float gap = 8.0f;
    const float rowGap = 10.0f;
    const float colW = (width - margin * 2.0f - gap) / 2.0f;
    const float col2X = margin + colW + gap;

    const float presetBarY = 48.0f, presetBarH = 40.0f;
    const float rowH = 260.0f; // fits Delay's 2-row/7-knob content, the tallest of any reorderable stage
    const float row1Y = presetBarY + presetBarH + rowGap;
    const float row2Y = row1Y + rowH + rowGap;
    const float row3Y = row2Y + rowH + rowGap;

    {
        L.presetBar.accent = kBitcrushAccent;
        const float y = presetBarY, h = presetBarH;
        const float barLeftX = margin;
        const float barRightX = width - margin;

        L.presetBar.prev = { barLeftX, y, 40.0f, h, "<", kBitcrushAccent };
        L.presetBar.next = { barLeftX + 40.0f + 8.0f, y, 40.0f, h, ">", kBitcrushAccent };

        const float delX = barRightX - 100.0f;
        const float saveX = delX - 8.0f - 90.0f;
        L.presetBar.save = { saveX, y, 90.0f, h, "SAVE", kBitcrushAccent };
        L.presetBar.del  = { delX, y, 100.0f, h, "DELETE", kFoldAccent };

        L.presetBar.nameX = L.presetBar.next.x + 40.0f + 14.0f;
        L.presetBar.nameY = y;
        L.presetBar.scale = { L.presetBar.save.x - 8.0f - 96.0f, L.presetBar.save.y, 96.0f, L.presetBar.save.h, "",
                              L.presetBar.save.accent };
        L.presetBar.nameW = L.presetBar.scale.x - 14.0f - L.presetBar.nameX;
        L.presetBar.nameH = h;
    }

    // 6 grid cells, in signal-flow reading order - cells 0..4 hold whichever
    // stage currently occupies that chain position, cell 5 is always Master
    struct CellRect { float x, y, w, h; };
    const CellRect cells[6] = {
        { margin, row1Y, colW, rowH },
        { col2X,  row1Y, colW, rowH },
        { margin, row2Y, colW, rowH },
        { col2X,  row2Y, colW, rowH },
        { margin, row3Y, colW, rowH },
        { col2X,  row3Y, colW, rowH },
    };

    int order[kStageCount];
    resolveChainOrder(values, order);

    static constexpr const char* kStageTitles[kStageCount] = { "BITCRUSH", "FOLD", "RING MOD", "LOFI FILTER", "DELAY" };
    static constexpr Color kStageAccents[kStageCount] = { kBitcrushAccent, kFoldAccent, kRingModAccent, kLofiAccent, kDelayAccent };

    PanelBox stagePanels[kStageCount]; // indexed by StageKind, not cell
    for (int cellIdx = 0; cellIdx < kStageCount; ++cellIdx)
    {
        const int stage = order[cellIdx];
        const CellRect& c = cells[cellIdx];
        PanelBox p{ c.x, c.y, c.w, c.h, kStageTitles[stage], kStageAccents[stage], true, stage };
        stagePanels[stage] = p;
        L.panels.push_back(p);
    }

    // Master - always cell 5, not draggable
    {
        const CellRect& c = cells[5];
        L.panels.push_back({ c.x, c.y, c.w, c.h, "MASTER", kMasterAccent, false, -1 });
    }

    const float firstRowOffset = kPanelHeaderH + 28.0f; // no-extras panels: header -> straight to knob row

    // --- bitcrush ---
    {
        const PanelBox& p = stagePanels[kStageBitcrush];
        addKnobRow(L.knobs, p.x, p.w, p.y + firstRowOffset, 20.0f, p.accent,
                   {
                       { kParamBitcrushBits, "BITS" },
                       { kParamBitcrushRate, "RATE" },
                       { kParamBitcrushMix,  "MIX" },
                   });
    }

    // --- fold ---
    {
        const PanelBox& p = stagePanels[kStageFold];
        addKnobRow(L.knobs, p.x, p.w, p.y + firstRowOffset, 20.0f, p.accent,
                   {
                       { kParamFoldDrive, "DRIVE" },
                       { kParamFoldMix,   "MIX" },
                   });
    }

    // --- ring mod ---
    {
        const PanelBox& p = stagePanels[kStageRingMod];

        Selector wave;
        wave.param = kParamRingModWave;
        wave.accent = p.accent;
        wave.x = p.x + 12.0f; wave.y = p.y + kPanelHeaderH + 4.0f; wave.w = p.w - 24.0f; wave.h = 24.0f;
        wave.options = { { 0.0f, "SAW" }, { 1.0f, "PULSE" }, { 2.0f, "TRI" } };
        L.selectors.push_back(wave);

        addKnobRow(L.knobs, p.x, p.w, wave.y + wave.h + 34.0f, 20.0f, p.accent,
                   {
                       { kParamRingModFreq, "FREQ" },
                       { kParamRingModMix,  "MIX" },
                   });
    }

    // --- lofi filter ---
    {
        const PanelBox& p = stagePanels[kStageLofi];
        addKnobRow(L.knobs, p.x, p.w, p.y + firstRowOffset, 20.0f, p.accent,
                   {
                       { kParamLofiCutoff, "CUTOFF" },
                       { kParamLofiReso,   "RESO" },
                       { kParamLofiMix,    "MIX" },
                   });
    }

    // --- delay --- (the dense one: Sync + 3 arp-shape selectors on two
    // lines, then a 5-knob row)
    {
        const PanelBox& p = stagePanels[kStageDelay];
        const float usable = p.w - 24.0f;
        // line1's caption ("SYNC"/"STEPS", drawn 6px above its own row)
        // needs clearance below the panel title text, not just kPanelHeaderH
        const float line1Y = p.y + kPanelHeaderH + 14.0f;
        // line2's caption ("CURVE"/"DIR") needs the same clearance below
        // line1's controls
        const float line2Y = line1Y + 24.0f + 16.0f;

        Dropdown sync;
        sync.param = kParamDelaySync;
        sync.accent = p.accent;
        sync.caption = "SYNC";
        sync.x = p.x + 12.0f; sync.y = line1Y; sync.h = 24.0f;
        sync.w = usable * 0.42f;
        sync.options = syncOptions();
        L.dropdowns.push_back(sync);

        Selector steps;
        steps.param = kParamDelayArpSteps;
        steps.accent = p.accent;
        steps.caption = "STEPS";
        steps.x = sync.x + sync.w + 8.0f; steps.y = line1Y; steps.h = 24.0f;
        steps.w = usable - sync.w - 8.0f;
        steps.options = { { 2.0f, "2" }, { 3.0f, "3" }, { 4.0f, "4" } };
        L.selectors.push_back(steps);

        Selector curve;
        curve.param = kParamDelayArpCurve;
        curve.accent = p.accent;
        curve.caption = "CURVE";
        curve.x = p.x + 12.0f; curve.y = line2Y; curve.h = 24.0f;
        curve.w = usable * 0.42f;
        curve.options = { { 0.0f, "PITCH" }, { 1.0f, "TEMPO" } };
        L.selectors.push_back(curve);

        Selector direction;
        direction.param = kParamDelayArpDirection;
        direction.accent = p.accent;
        direction.caption = "DIR";
        direction.x = curve.x + curve.w + 8.0f; direction.y = line2Y; direction.h = 24.0f;
        direction.w = usable - curve.w - 8.0f;
        direction.options = { { 0.0f, "UP" }, { 1.0f, "DOWN" }, { 2.0f, "UP-DN" } };
        L.selectors.push_back(direction);

        // 7 knobs total, split across two rows so none get too cramped:
        // row 1 groups the "what pitch/rate does each repeat play at"
        // fundamentals, row 2 groups mix/character controls.
        const float knobRow1Y = line2Y + 24.0f + 34.0f;
        const float knobRow2Y = knobRow1Y + 64.0f;

        addKnobRow(L.knobs, p.x, p.w, knobRow1Y, 20.0f, p.accent,
                   {
                       { kParamDelayFeedback, "FDBK" },
                       { kParamDelayArp,      "ARP" },
                       { kParamDelaySpeed,    "SPEED" },
                       { kParamDelayRamp,     "RAMP" },
                   });
        addKnobRow(L.knobs, p.x, p.w, knobRow2Y, 20.0f, p.accent,
                   {
                       { kParamDelayPingPong, "P-PONG" },
                       { kParamDelayJitter,   "JITTER" },
                       { kParamDelayMix,      "MIX" },
                   });
    }

    // --- master ---
    {
        const PanelBox& p = L.panels[5];
        addKnobRow(L.knobs, p.x, p.w, p.y + firstRowOffset, 20.0f, p.accent,
                   {
                       { kParamMasterOutput, "OUTPUT" },
                       { kParamMasterDrive,  "DRIVE" },
                   });
    }

    return L;
}

// -----------------------------------------------------------------------------------------------------------
// value <-> knob-rotation mapping (shared by drawing and mouse-drag hit logic)

inline float paramToNormalized(uint32_t param, float value) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    float t;
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        const float v = value < lo ? lo : (value > hi ? hi : value);
        t = (float)(std::log(v / lo) / std::log(hi / lo));
    }
    else
    {
        t = (value - info.min) / (info.max - info.min);
    }
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

inline float normalizedToParam(uint32_t param, float t) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        return lo * std::pow(hi / lo, t);
    }
    return info.min + (info.max - info.min) * t;
}

// snaps a value to the nearest whole semitone step, for knobs where that's
// musically meaningful - Arp (already in semitones, just rounds) and Speed
// (a rate multiplier, rounded in log2-semitone-space so it lands on the
// same chromatic grid a tape/turntable pitch control would use). A no-op
// for every other param. Used when the user holds Shift while dragging or
// scrolling a knob.
inline float quantizeToSemitoneStep(uint32_t param, float value) noexcept
{
    if (param == kParamDelayArp)
        return std::round(value);

    if (param == kParamDelaySpeed)
    {
        const ParamInfo& info = getParamInfo(param);
        const float snapped = std::exp2(std::round(std::log2(value) * 12.0f) / 12.0f);
        return snapped < info.min ? info.min : (snapped > info.max ? info.max : snapped);
    }

    return value;
}

// selector/dropdown option rects are needed both for drawing and for mouse
// hit-testing; compute them in one place so the two can't drift apart.
inline void getSelectorOptionRect(const Selector& sel, size_t i, double& ox, double& ow) noexcept
{
    const size_t n = sel.options.size();
    const double gap = 6.0;
    ow = (sel.w - gap * (double)(n - 1)) / (double)n;
    ox = sel.x + (ow + gap) * (double)i;
}

// dropdown open-list option rects: opens downward normally, flips upward if
// the list would otherwise run off the bottom of the window and there's
// more room above the closed box than below it (Delay's Sync list has 17
// entries). The whole list is then clamped to stay within [0, layoutHeight]
// - an unclamped list would render (and be unclickable) partly off-screen
// if it doesn't fully fit in either direction.
inline void getDropdownOptionRect(const Dropdown& dd, size_t i, float layoutHeight, double& oy, double& oh) noexcept
{
    const double n = (double)dd.options.size();
    oh = dd.h - 4.0;
    const double totalH = n * oh;

    const double downSpace = (double)layoutHeight - (dd.y + dd.h);
    const double upSpace = dd.y;
    const bool openUpward = (totalH > downSpace) && (upSpace > downSpace);

    double start = openUpward ? (dd.y - totalH) : (dd.y + dd.h);
    const double maxStart = (double)layoutHeight - totalH;
    if (start > maxStart) start = maxStart;
    if (start < 0.0) start = 0.0;

    oy = start + oh * (double)i;
}

// -----------------------------------------------------------------------------------------------------------
// value formatting

inline void formatParamValue(uint32_t index, float value, char* out, size_t outSize) noexcept
{
    const ParamInfo& info = getParamInfo(index);

    if (std::strcmp(info.unit, "Hz") == 0)
    {
        if (value >= 1000.0f)
            std::snprintf(out, outSize, "%.2fk", value / 1000.0);
        else
            std::snprintf(out, outSize, "%.0f", value);
    }
    else if (std::strcmp(info.unit, "x") == 0)
    {
        std::snprintf(out, outSize, "%.0fx", value);
    }
    else if (std::strcmp(info.unit, "st") == 0)
    {
        std::snprintf(out, outSize, "%.0fst", value);
    }
    else
    {
        std::snprintf(out, outSize, "%.2f", value);
    }
}

// -----------------------------------------------------------------------------------------------------------
// drawing helpers

inline void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -M_PI_2, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0,      M_PI_2);
    cairo_arc(cr, x + r,     y + h - r, r, M_PI_2,   M_PI);
    cairo_arc(cr, x + r,     y + r,     r, M_PI,     3.0 * M_PI_2);
    cairo_close_path(cr);
}

// cairo_show_text() leaves a dangling current point; if the next drawing call
// is cairo_arc()/cairo_line_to() without an explicit new subpath, cairo will
// implicitly connect from that stray point. Always reset the path after text.
inline void drawText(cairo_t* cr, const char* text, double x, double y)
{
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
    cairo_new_path(cr);
}

inline void centeredText(cairo_t* cr, const char* text, double cx, double cy)
{
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    drawText(cr, text, cx - ext.width / 2.0 - ext.x_bearing, cy - ext.height / 2.0 - ext.y_bearing);
}

inline void setFont(cairo_t* cr, double size, bool bold = true)
{
    cairo_select_font_face(cr, "monospace",
                            CAIRO_FONT_SLANT_NORMAL,
                            bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
}

// -----------------------------------------------------------------------------------------------------------
// paint

struct PaintState
{
    const float* values = nullptr;   // [kParamCount]
    int hoverKnob = -1;
    int dragKnob = -1;
    int hoverSelector = -1;
    int hoverSelectorOption = -1;
    int hoverDropdown = -1;
    int openDropdown = -1;
    int hoverDropdownOption = -1;
    const bool* autoShowValue = nullptr; // [kParamCount]

    const char* presetName = "INIT";
    bool presetIsUnsaved = false;
    bool presetEditingName = false;
    const char* presetEditBuffer = "";
    int hoverPresetButton = -1;
    const char* scaleLabel = "";      // UI-scale button text (UiScale.hpp formatLabel)
    bool presetDeleteEnabled = false;

    // panel drag-to-reorder state
    int hoverPanel = -1;      // index into Layout::panels, header-strip hover only
    int dragPanel = -1;       // index into Layout::panels currently being dragged, or -1
    int dropTargetCell = -1;  // cell 0-4 the drag is currently hovering as a drop target, or -1
    double dragGhostX = 0.0;  // top-left of the floating ghost while dragging
    double dragGhostY = 0.0;
};

inline void paintKnob(cairo_t* cr, const Knob& k, float value, bool hovered, bool dragging, bool autoShow)
{
    const float t = paramToNormalized(k.param, value);

    const double startAngle = M_PI * 0.75;
    const double sweep = M_PI * 1.5;
    const double valueAngle = startAngle + sweep * t;

    cairo_set_line_width(cr, 5.0);
    setColor(cr, kKnobRing);
    cairo_arc(cr, k.cx, k.cy, k.radius, startAngle, startAngle + sweep);
    cairo_stroke(cr);

    setColor(cr, k.accent, dragging ? 1.0 : (hovered ? 0.9 : 0.75));
    cairo_arc(cr, k.cx, k.cy, k.radius, startAngle, valueAngle);
    cairo_stroke(cr);

    setColor(cr, kKnobBody);
    cairo_arc(cr, k.cx, k.cy, k.radius - 6.0, 0.0, 2.0 * M_PI);
    cairo_fill(cr);

    if (hovered || dragging)
    {
        setColor(cr, k.accent, 0.18);
        cairo_arc(cr, k.cx, k.cy, k.radius - 6.0, 0.0, 2.0 * M_PI);
        cairo_fill(cr);
    }

    setColor(cr, k.accent);
    cairo_set_line_width(cr, 3.0);
    const double px = k.cx + std::cos(valueAngle) * (k.radius - 10.0);
    const double py = k.cy + std::sin(valueAngle) * (k.radius - 10.0);
    cairo_move_to(cr, k.cx, k.cy);
    cairo_line_to(cr, px, py);
    cairo_stroke(cr);

    setFont(cr, 9.5);
    setColor(cr, kTextDim);
    centeredText(cr, k.label, k.cx, k.cy + k.radius + 13.0);

    if (dragging || autoShow)
    {
        char buf[32];
        formatParamValue(k.param, value, buf, sizeof(buf));
        setFont(cr, 10.5);
        setColor(cr, k.accent);
        centeredText(cr, buf, k.cx, k.cy - k.radius - 11.0);
    }
}

inline void paintSelector(cairo_t* cr, const Selector& sel, float value, int hoverOption)
{
    if (sel.caption != nullptr)
    {
        setFont(cr, 9.0, false);
        setColor(cr, kTextDim);
        drawText(cr, sel.caption, sel.x, sel.y - 6.0);
    }

    const size_t n = sel.options.size();

    for (size_t i = 0; i < n; ++i)
    {
        double ox, optW;
        getSelectorOptionRect(sel, i, ox, optW);
        const bool active = std::fabs(sel.options[i].value - value) < 0.001f;
        const bool hovered = (int)i == hoverOption;

        roundedRect(cr, ox, sel.y, optW, sel.h, 4.0);
        if (active)
            setColor(cr, sel.accent, 0.9);
        else
            setColor(cr, kBtnBg, hovered ? 1.0 : 0.85);
        cairo_fill_preserve(cr);
        setColor(cr, active ? sel.accent : kPanelEdge);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        setFont(cr, 10.5);
        setColor(cr, active ? kBg : kTextMain);
        centeredText(cr, sel.options[i].label, ox + optW / 2.0, sel.y + sel.h / 2.0);
    }
}

inline const char* findOptionLabel(const std::vector<SelectorOption>& options, float value) noexcept
{
    for (const SelectorOption& opt : options)
        if (std::fabs(opt.value - value) < 0.001f)
            return opt.label;
    return "?";
}

inline void paintDropdownClosed(cairo_t* cr, const Dropdown& dd, float value, bool hovered, bool open)
{
    if (dd.caption != nullptr)
    {
        setFont(cr, 9.0, false);
        setColor(cr, kTextDim);
        drawText(cr, dd.caption, dd.x, dd.y - 6.0);
    }

    roundedRect(cr, dd.x, dd.y, dd.w, dd.h, 4.0);
    setColor(cr, kBtnBg, (hovered || open) ? 1.0 : 0.85);
    cairo_fill_preserve(cr);
    setColor(cr, open ? dd.accent : kPanelEdge, 1.0);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    setFont(cr, 10.5);
    setColor(cr, dd.accent);
    cairo_text_extents_t ext;
    const char* label = findOptionLabel(dd.options, value);
    cairo_text_extents(cr, label, &ext);
    drawText(cr, label, dd.x + 10.0, dd.y + dd.h / 2.0 - ext.height / 2.0 - ext.y_bearing);

    const double ax = dd.x + dd.w - 18.0, ay = dd.y + dd.h / 2.0;
    setColor(cr, kTextDim);
    cairo_move_to(cr, ax - 5.0, open ? ay + 3.0 : ay - 3.0);
    cairo_line_to(cr, ax + 5.0, open ? ay + 3.0 : ay - 3.0);
    cairo_line_to(cr, ax,       open ? ay - 3.0 : ay + 3.0);
    cairo_close_path(cr);
    cairo_fill(cr);
}

inline void paintDropdownOpenList(cairo_t* cr, const Dropdown& dd, float value, int hoverOption, float layoutHeight)
{
    const size_t n = dd.options.size();
    double oyFirst, oyLast, oh;
    getDropdownOptionRect(dd, 0, layoutHeight, oyFirst, oh);
    getDropdownOptionRect(dd, n - 1, layoutHeight, oyLast, oh);
    const double listTop = oyFirst < oyLast ? oyFirst : oyLast;
    const double listBottom = (oyFirst < oyLast ? oyLast : oyFirst) + oh;

    setColor(cr, kBg, 0.94);
    cairo_rectangle(cr, dd.x, listTop, dd.w, listBottom - listTop);
    cairo_fill(cr);

    for (size_t i = 0; i < n; ++i)
    {
        double oy;
        getDropdownOptionRect(dd, i, layoutHeight, oy, oh);
        const bool active = std::fabs(dd.options[i].value - value) < 0.001f;
        const bool hovered = (int)i == hoverOption;

        cairo_rectangle(cr, dd.x, oy, dd.w, oh);
        if (active)
            setColor(cr, dd.accent, 0.9);
        else
            setColor(cr, kBtnBg, hovered ? 1.0 : 0.0);
        cairo_fill(cr);

        setFont(cr, 10.0);
        setColor(cr, active ? kBg : kTextMain);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, dd.options[i].label, &ext);
        drawText(cr, dd.options[i].label, dd.x + 10.0, oy + oh / 2.0 - ext.height / 2.0 - ext.y_bearing);
    }

    setColor(cr, dd.accent, 0.8);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, dd.x, listTop, dd.w, listBottom - listTop);
    cairo_stroke(cr);
}

inline void paintButton(cairo_t* cr, const Button& b, bool hovered, bool enabled)
{
    roundedRect(cr, b.x, b.y, b.w, b.h, 4.0);
    setColor(cr, kBtnBg, enabled ? (hovered ? 1.0 : 0.85) : 0.4);
    cairo_fill_preserve(cr);
    setColor(cr, b.accent, enabled ? 1.0 : 0.35);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    setFont(cr, 11.5);
    setColor(cr, b.accent, enabled ? 1.0 : 0.35);
    centeredText(cr, b.label, b.x + b.w / 2.0, b.y + b.h / 2.0);
}

inline void paintPresetBar(cairo_t* cr, const PresetBarLayout& pb, const PaintState& state)
{
    paintButton(cr, pb.prev, state.hoverPresetButton == 0, true);
    paintButton(cr, pb.next, state.hoverPresetButton == 1, true);
    paintButton(cr, pb.save, state.hoverPresetButton == 2, true);
    paintButton(cr, pb.del,  state.hoverPresetButton == 3, state.presetDeleteEnabled);

    Button scale = pb.scale;
    scale.label = state.scaleLabel;
    paintButton(cr, scale, state.hoverPresetButton == 4, true);

    roundedRect(cr, pb.nameX, pb.nameY, pb.nameW, pb.nameH, 4.0);
    setColor(cr, state.presetEditingName ? kKnobBody : kBtnBg, 0.9);
    cairo_fill_preserve(cr);
    setColor(cr, state.presetEditingName ? pb.accent : kPanelEdge, state.presetEditingName ? 1.0 : 0.8);
    cairo_set_line_width(cr, state.presetEditingName ? 2.0 : 1.5);
    cairo_stroke(cr);

    setFont(cr, 9.0, false);
    setColor(cr, kTextDim);
    drawText(cr, "PRESET", pb.nameX + 12.0, pb.nameY + 14.0);

    setFont(cr, 13.0);
    if (state.presetEditingName)
    {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s_", state.presetEditBuffer);
        setColor(cr, kTextMain);
        drawText(cr, buf, pb.nameX + 12.0, pb.nameY + pb.nameH - 12.0);
    }
    else
    {
        setColor(cr, state.presetIsUnsaved ? kFoldAccent : kTextMain);
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s%s", state.presetName, state.presetIsUnsaved ? " *" : "");
        drawText(cr, buf, pb.nameX + 12.0, pb.nameY + pb.nameH - 12.0);
    }
}

// a semi-transparent copy of the dragged panel's box + title, following the
// cursor - the source cell's real content stays in place (dimmed, see
// paint()) until the drag resolves.
inline void paintDragGhost(cairo_t* cr, const PanelBox& p, double ghostX, double ghostY)
{
    roundedRect(cr, ghostX, ghostY, p.w, p.h, 8.0);
    setColor(cr, kPanelBg, 0.85);
    cairo_fill_preserve(cr);
    setColor(cr, p.accent, 0.95);
    cairo_set_line_width(cr, 2.5);
    cairo_stroke(cr);

    if (p.title != nullptr)
    {
        setFont(cr, 12.0);
        setColor(cr, p.accent, 0.95);
        drawText(cr, p.title, ghostX + 14.0, ghostY + 22.0);
    }
}

inline void paint(cairo_t* cr, const Layout& L, const PaintState& state)
{
    setColor(cr, kBg);
    cairo_paint(cr);

    setFont(cr, 24.0);
    setColor(cr, kBitcrushAccent);
    drawText(cr, "SIDEOUS EFFECTS", 20.0, 36.0);

    setFont(cr, 9.5, false);
    setColor(cr, kTextDim);
    {
        const char* subtitle = "RAW SID/POKEY FX BOX";
        cairo_text_extents_t ext;
        cairo_text_extents(cr, subtitle, &ext);
        drawText(cr, subtitle, L.width - 20.0 - ext.width, 22.0);
    }

    paintPresetBar(cr, L.presetBar, state);

    // drop-target highlight, drawn under the panels so their own border
    // still reads on top
    if (state.dropTargetCell >= 0 && (size_t)state.dropTargetCell < L.panels.size())
    {
        const PanelBox& target = L.panels[(size_t)state.dropTargetCell];
        roundedRect(cr, target.x - 3.0, target.y - 3.0, target.w + 6.0, target.h + 6.0, 10.0);
        setColor(cr, target.accent, 0.9);
        cairo_set_line_width(cr, 3.0);
        cairo_stroke(cr);
    }

    for (size_t i = 0; i < L.panels.size(); ++i)
    {
        const PanelBox& p = L.panels[i];
        const bool isDragSource = (int)i == state.dragPanel;

        roundedRect(cr, p.x, p.y, p.w, p.h, 8.0);
        setColor(cr, kPanelBg, isDragSource ? 0.4 : 1.0);
        cairo_fill_preserve(cr);
        setColor(cr, p.accent, isDragSource ? 0.3 : ((int)i == state.hoverPanel && p.draggable ? 1.0 : 0.7));
        cairo_set_line_width(cr, (int)i == state.hoverPanel && p.draggable && !isDragSource ? 2.5 : 1.5);
        cairo_stroke(cr);

        if (p.title != nullptr)
        {
            setFont(cr, 12.0);
            setColor(cr, p.accent, isDragSource ? 0.4 : 1.0);
            drawText(cr, p.title, p.x + 14.0, p.y + 22.0);

            // a small drag-handle affordance (dots) at the header's right
            // edge for the reorderable panels, so it reads as grabbable
            if (p.draggable)
            {
                setColor(cr, p.accent, isDragSource ? 0.3 : 0.55);
                for (int dot = 0; dot < 3; ++dot)
                {
                    cairo_arc(cr, p.x + p.w - 16.0, p.y + 10.0 + (double)dot * 5.0, 1.6, 0.0, 2.0 * M_PI);
                    cairo_fill(cr);
                }
            }
        }
    }

    for (size_t i = 0; i < L.selectors.size(); ++i)
    {
        const Selector& sel = L.selectors[i];
        const int hoverOption = (int)i == state.hoverSelector ? state.hoverSelectorOption : -1;
        paintSelector(cr, sel, state.values[sel.param], hoverOption);
    }

    for (size_t i = 0; i < L.dropdowns.size(); ++i)
    {
        const Dropdown& dd = L.dropdowns[i];
        paintDropdownClosed(cr, dd, state.values[dd.param], (int)i == state.hoverDropdown, (int)i == state.openDropdown);
    }

    for (size_t i = 0; i < L.knobs.size(); ++i)
    {
        const Knob& k = L.knobs[i];
        const bool autoShow = state.autoShowValue != nullptr && state.autoShowValue[k.param];
        paintKnob(cr, k, state.values[k.param], (int)i == state.hoverKnob, (int)i == state.dragKnob, autoShow);
    }

    if (state.openDropdown >= 0 && (size_t)state.openDropdown < L.dropdowns.size())
    {
        const Dropdown& dd = L.dropdowns[state.openDropdown];
        paintDropdownOpenList(cr, dd, state.values[dd.param], state.hoverDropdownOption, L.height);
    }

    if (state.dragPanel >= 0 && (size_t)state.dragPanel < L.panels.size())
        paintDragGhost(cr, L.panels[(size_t)state.dragPanel], state.dragGhostX, state.dragGhostY);
}

} // namespace ui
} // namespace sideous
