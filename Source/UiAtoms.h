#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace pflow {

// ── Icon glyphs (Phosphor-style, drawn as paths) ─────────────────────────────

namespace icons {
    inline constexpr const char* play          = "play";
    inline constexpr const char* pause         = "pause";
    inline constexpr const char* stop          = "stop";
    inline constexpr const char* record        = "record";
    inline constexpr const char* caretUp       = "caret-up";
    inline constexpr const char* caretDown     = "caret-down";
    inline constexpr const char* caretLeft     = "caret-left";
    inline constexpr const char* caretRight    = "caret-right";
    inline constexpr const char* arrowsIn      = "arrows-in-line-horizontal";
    inline constexpr const char* arrowsOut     = "arrows-out-line-horizontal";
    /** Arrow into a tray — drag a clip out to the DAW. */
    inline constexpr const char* dragOut       = "drag-out";
    inline constexpr const char* scissors      = "scissors";
    inline constexpr const char* sliders       = "sliders";
    /** Vertical EQ bars — standard audio Effects glyph. */
    inline constexpr const char* eq            = "eq";
    /** Two rotary dials — Effects. */
    inline constexpr const char* knobs         = "knobs";
    /** Funnel — Filters. */
    inline constexpr const char* funnel        = "funnel";
    /** Wrench — Settings. */
    inline constexpr const char* wrench        = "wrench";
    inline constexpr const char* folder        = "folder";
    inline constexpr const char* folderOpen    = "folder-open";
    inline constexpr const char* star          = "star";
    /** Stroke star for Photos-style nav (Favorites). */
    inline constexpr const char* starOutline   = "star-outline";
    inline constexpr const char* heart         = "heart";
    inline constexpr const char* heartOutline  = "heart-outline";
    /** Bookmark / Keep in Library. */
    inline constexpr const char* bookmark      = "bookmark";
    inline constexpr const char* bookmarkFill  = "bookmark-fill";
    /** Floppy — Save / Keep in Library. */
    inline constexpr const char* save          = "save";
    inline constexpr const char* saveFill      = "save-fill";
    /** 5-pin DIN plug. */
    inline constexpr const char* midi          = "midi";
    inline constexpr const char* plus          = "plus";
    inline constexpr const char* minus         = "minus";
    inline constexpr const char* x             = "x";
    inline constexpr const char* undo          = "arrow-counter-clockwise";
    inline constexpr const char* zoomIn        = "zoom-in";
    inline constexpr const char* zoomOut       = "zoom-out";
    inline constexpr const char* noteBass      = "wave";        // bass clips
    inline constexpr const char* noteKeys      = "piano-keys";  // keys clips
    inline constexpr const char* noteDrums     = "drum";        // drum clips
    inline constexpr const char* sidebar       = "sidebar";
    /** Panel with divider on the right — effects / inspector rail. */
    inline constexpr const char* sidebarRight  = "sidebar-right";
    /** Linked tempo / host sync (not a refresh arrow). */
    inline constexpr const char* sync          = "sync";
    /** Circular arrow — rescan the open folder. */
    inline constexpr const char* refresh       = "refresh";
    inline constexpr const char* gear          = "gear";
    /** 2×2 cells — toggle list/grid browser view. */
    inline constexpr const char* grid          = "grid";
    /** Nested folders — include files from subdirectories. */
    inline constexpr const char* subfolders    = "subfolders";
    inline constexpr const char* more          = "more";
    inline constexpr const char* lockOpen      = "lock-open";
    inline constexpr const char* lockClosed    = "lock-closed";
    inline constexpr const char* foldRows      = "fold-rows";
    inline constexpr const char* infinity      = "infinity";
    inline constexpr const char* search        = "search";
    inline constexpr const char* moon          = "moon";
    inline constexpr const char* sun           = "sun";
}

/** Stroke-drawn icon centered in bounds. `px` is the stroke width. */
void drawIcon(juce::Graphics& g, const juce::String& name,
              juce::Rectangle<float> bounds, juce::Colour colour, float px = 1.6f);

/** Groove focus: 1px solid `focus` outline, offset 1px. */
inline void drawFocusRing(juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour(ds::focus());
    g.drawRoundedRectangle(bounds.expanded(1.5f), radius + 1.0f, 1.0f);
}

// ── Small controls ───────────────────────────────────────────────────────────

/** Icon-only button — no square face; `active` tints the glyph. */
class IconBtn : public juce::Button
{
public:
    explicit IconBtn(const juce::String& iconName, const juce::String& tip = {});
    void paintButton(juce::Graphics&, bool over, bool down) override;

    juce::String icon;
    bool active = false;       // accent (or activeTint) glyph
    bool ghost = true;         // retained for callers; faces are never drawn
    bool headerChrome = false; // muted idle glyph in pane headers
    float iconScale = 1.0f;
    /** When set, the button draws this label beside the icon and grows to fit. */
    juce::String caption;
    /** Used instead of the accent colour while `active`, when not transparent. */
    juce::Colour activeTint {};
    int captionWidth() const;
};

/** Rounded chip with optional leading icon; used for Trim, badges, Editor btn. */
class ChipBtn : public juce::Button
{
public:
    explicit ChipBtn(const juce::String& text, const juce::String& iconName = {});
    void paintButton(juce::Graphics&, bool over, bool down) override;
    int idealWidth() const;

    juce::String label;
    juce::String icon;         // leading icon
    juce::String trailingIcon; // e.g. removable “x”
    bool active = false;       // accent style
    bool accentText = false;   // accent-tinted text/icon when inactive
    bool mono = false;
    /** If >= 0, use this corner radius; otherwise capsule (height/2). */
    float cornerRadius = -1.0f;
};

/** Pill toggle with a status dot (DAW-sync). */
class PillToggle : public juce::Button
{
public:
    PillToggle(const juce::String& onText, const juce::String& offText);
    void paintButton(juce::Graphics&, bool over, bool down) override;
    int idealWidth() const;

    juce::String onLabel, offLabel;
};

/** Connected two-option text toggle — both labels always visible. */
class SegmentToggle : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    SegmentToggle(juce::String leftLabel, juce::String rightLabel);
    void setIndex(int i, juce::NotificationType notify = juce::dontSendNotification);
    int getIndex() const { return index; }
    int idealWidth() const;
    std::function<void(int)> onChange;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    int hitIndex(juce::Point<int> p) const;

    juce::String left, right;
    int index = 0;
    int hover = -1;
};

/** Labelled mini switch (Fit to scale / Map to root): caption · track · value
    laid out on one row. */
class MiniSwitch : public juce::Button
{
public:
    explicit MiniSwitch(const juce::String& caption);
    void paintButton(juce::Graphics&, bool over, bool down) override;
    int idealWidth() const;

    juce::String caption;          // control name, left of the track
    juce::String onText { "on" };  // value text when on
    juce::String offText { "off" };
};

/** - value + stepper (octave). */
class Stepper : public juce::Component
{
public:
    Stepper();
    void resized() override;
    void paint(juce::Graphics&) override;

    void setValue(int v, juce::NotificationType notify = juce::sendNotification);
    int getValue() const { return value; }
    int minValue = -3, maxValue = 3;
    std::function<void(int)> onChange;
    std::function<juce::String(int)> format;

private:
    int value = 0;
    IconBtn btnDown { icons::minus }, btnUp { icons::plus };
};

} // namespace pflow
