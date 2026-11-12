#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace pflow {
namespace ds {

/** MidiToolkit Design System v1.0 — Cupertino direction (§2 colour tokens).
    All app hex values live here. Surfaces must reference these accessors only. */

struct Palette
{
    // §2 named tokens
    juce::Colour bg;       // window base
    juce::Colour chrome;   // toolbar
    juce::Colour side;     // sidebar tint
    juce::Colour panel;    // content panes, cards
    juce::Colour rollchrome;
    juce::Colour hl;       // hairlines
    juce::Colour tx;
    juce::Colour tx2;
    juce::Colour tx3;
    juce::Colour acc;
    juce::Colour selsoft;
    juce::Colour ctl;
    juce::Colour ctlb;
    juce::Colour field;
    juce::Colour trk;
    juce::Colour rowalt;
    juce::Colour note;
    juce::Colour stron;    // favorite star on

    // §6 piano-roll keys / lanes (specified in component section)
    juce::Colour kw;       // white keys
    juce::Colour kb;       // black keys
    juce::Colour laneb;    // black-key lane wash
    juce::Colour beat;     // beat grid lines

    // Kind chips (§2)
    juce::Colour kindDrums;
    juce::Colour kindBass;
    juce::Colour kindKeys;
    juce::Colour kindPerc;

    // Window border (paired with §4 window shadow)
    juce::Colour winbrd;

    // Hover washes (§6 / §9)
    juce::Colour hoverWash;

    // Toolkit pane (sidebar A)
    juce::Colour head;
    juce::Colour well;
    juce::Colour kfInW;
    juce::Colour kfInB;
    juce::Colour kfOutW;
    juce::Colour kfOutB;

    juce::Colour lensFill;    // logo lens face
    juce::Colour logoNoteHi;  // logo note highlight
};

const Palette& palette();

inline bool dark() { return palette().bg.getBrightness() < 0.5f; }

// Convenience aliases matching CSS token names.
inline juce::Colour bg()       { return palette().bg; }
inline juce::Colour chrome()   { return palette().chrome; }
inline juce::Colour side()     { return palette().side; }
inline juce::Colour panel()    { return palette().panel; }
inline juce::Colour rollchrome() { return palette().rollchrome; }
inline juce::Colour hl()       { return palette().hl; }
inline juce::Colour tx()       { return palette().tx; }
inline juce::Colour tx2()      { return palette().tx2; }
inline juce::Colour tx3()      { return palette().tx3; }
inline juce::Colour acc()      { return palette().acc; }
inline juce::Colour selsoft()  { return palette().selsoft; }
inline juce::Colour ctl()      { return palette().ctl; }
inline juce::Colour ctlb()     { return palette().ctlb; }
inline juce::Colour field()    { return palette().field; }
inline juce::Colour trk()      { return palette().trk; }
inline juce::Colour rowalt()   { return palette().rowalt; }
inline juce::Colour note()     { return palette().note; }
inline juce::Colour stron()    { return palette().stron; }
inline juce::Colour kw()       { return palette().kw; }
inline juce::Colour kb()       { return palette().kb; }
inline juce::Colour laneb()    { return palette().laneb; }
inline juce::Colour beat()     { return palette().beat; }
inline juce::Colour winbrd()   { return palette().winbrd; }
inline juce::Colour hoverWash(){ return palette().hoverWash; }
inline juce::Colour head()     { return palette().head; }
inline juce::Colour well()     { return palette().well; }
inline juce::Colour kfInW()    { return palette().kfInW; }
inline juce::Colour kfInB()    { return palette().kfInB; }
inline juce::Colour kfOutW()   { return palette().kfOutW; }
inline juce::Colour kfOutB()   { return palette().kfOutB; }
inline juce::Colour lensFill() { return palette().lensFill; }
inline juce::Colour logoNoteHi() { return palette().logoNoteHi; }
inline juce::Colour accent()  { return acc(); }
inline juce::Colour kindDrums(){ return palette().kindDrums; }
inline juce::Colour kindBass() { return palette().kindBass; }
inline juce::Colour kindKeys() { return palette().kindKeys; }
inline juce::Colour kindPerc() { return palette().kindPerc; }

// ── Groove Panel tokens (exact names from the design system) ────────────────
juce::Colour surface000();
juce::Colour surface100();
juce::Colour surface200();
juce::Colour surface300();
juce::Colour line();
juce::Colour edge();
juce::Colour ink();
juce::Colour inkMuted();
juce::Colour inkFaint();
juce::Colour onAccent();
juce::Colour play();
juce::Colour record();
juce::Colour solo();
juce::Colour active();
juce::Colour onLit();
juce::Colour focus();
/** Levels: solid primary actions (Play, Drag Me, active New) and their label colour. */
juce::Colour primary();
juce::Colour onPrimary();
juce::Colour warning();
/** Levels active state: 1 px acc() border over a selsoft() tint. */
void drawActive(juce::Graphics& g, juce::Rectangle<float> r, float radius);
/** clip-1 … clip-8: the user's labels. Same in both themes; text on them is onClip(). */
juce::Colour clip(int n);
juce::Colour onClip();

/** Groove radii: radius-0 panels, radius-sm toggles/fields, radius-md popovers/devices. */
namespace radius {
    constexpr float none = 0.0f;
    constexpr float sm   = 4.0f;   // Levels rounded.sm
    constexpr float md   = 8.0f;   // Levels rounded.md
}

/** §4 shadow recipes — drop shadows only; never invent others. */
namespace shadow {
    void control (juce::Graphics& g, juce::Rectangle<float> r, float radius);
    void card    (juce::Graphics& g, juce::Rectangle<float> r, float radius);
    void popover (juce::Graphics& g, juce::Rectangle<float> r, float radius);
    void thumb   (juce::Graphics& g, juce::Rectangle<float> thumb); // white circle + ring
}

/** §3 type styles — map every text element to one of these. */
enum class Type
{
    AppName,       // 13 / 600 / tx2
    PanelHeading,  // 12.5 / 600 / tx
    SectionLabel,  // 10 / 600 uppercase tracking / tx3
    TableHeader,   // 10.5 / 600 / tx3
    Body,          // 12–12.5 / 400 / tx
    Metadata,      // 11–11.5 / 400 tabular / tx2
    Caption,       // 10 / 400 / tx3
    NumericDisplay, // 13 / 600 tabular / tx
    Logo            // 14 / 600 / −0.02 tracking / tx
};

juce::Font font (Type t);
juce::Colour colour (Type t);

/** §5 region metrics (logical px at Medium). */
namespace region {
    constexpr int toolbarH     = 45;   // header (+5; content stays vertically centered)
    constexpr int sidebarW     = 200;  // library (Levels mockup)
    constexpr int fileListW    = 368;  // file list column
    constexpr int gridBrowserW = 484;  // five 96 px kind columns + hairlines
    constexpr int toolkitW     = 280;  // Levels: Toolkit + Generator column
    constexpr int rollMinW     = 320;
    constexpr int velocityH    = 100;
    constexpr int previewH     = 134;  // 14 ruler + 120 notes
    constexpr int listRowH     = 28;  // Levels
    constexpr int listHeaderH  = 28;  // Levels
    constexpr int sidebarRowH  = 28;  // Levels
}

} // namespace ds
} // namespace pflow
