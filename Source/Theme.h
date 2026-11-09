#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "DesignSystem.h"

namespace pflow {

// ── Cupertino design tokens (MidiToolkit Design System v1.0) ─────────────────
// Source of truth: DesignSystem.h / DesignSystem.cpp (§2). Density tweaks remain.

enum class Density { Compact, Comfortable };
enum class ContentSize { Small, Medium, Large };
enum class Appearance { System = 0, Light = 1, Dark = 2 };

/** Named themes. Indexes 0–9 are light, 10–19 are dark.
    Stored appearance 0/1/2 from older builds is migrated on load. */
struct ThemeSpec
{
    const char* name;
    bool dark;
    float hue;
    float amount;
    float lift;
};

inline constexpr ThemeSpec kThemes[] = {
    { "Paper",    false, 0.00f, 0.00f,  0.00f },
    { "Fog",      false, 0.58f, 0.22f,  0.00f },
    { "Bone",     false, 0.09f, 0.18f,  0.02f },
    { "Sand",     false, 0.10f, 0.28f,  0.00f },
    { "Mist",     false, 0.56f, 0.16f,  0.03f },
    { "Pearl",    false, 0.60f, 0.08f,  0.05f },
    { "Linen",    false, 0.08f, 0.14f,  0.01f },
    { "Ice",      false, 0.55f, 0.30f,  0.02f },
    { "Sage",     false, 0.33f, 0.22f,  0.01f },
    { "Blush",    false, 0.97f, 0.20f,  0.02f },
    { "Ink",      true,  0.00f, 0.00f,  0.00f },
    { "Carbon",   true,  0.00f, 0.00f, -0.04f },
    { "Slate",    true,  0.58f, 0.28f,  0.00f },
    { "Navy",     true,  0.62f, 0.38f, -0.02f },
    { "Plum",     true,  0.78f, 0.30f,  0.00f },
    { "Forest",   true,  0.36f, 0.28f, -0.02f },
    { "Ember",    true,  0.06f, 0.32f,  0.00f },
    { "Graphite", true,  0.58f, 0.10f, -0.02f },
    { "Midnight", true,  0.64f, 0.42f, -0.04f },
    { "Cocoa",    true,  0.07f, 0.26f, -0.01f },
};

inline constexpr int kNumThemes = (int) (sizeof(kThemes) / sizeof(kThemes[0]));

inline const ThemeSpec& themeAt(int index)
{
    return kThemes[(size_t) juce::jlimit(0, kNumThemes - 1, index)];
}

/** Old files stored System=0, Light=1, Dark=2. themeCatalog marks the 20-theme ids. */
inline int migrateThemeIndex(int stored, bool themeCatalog)
{
    if (themeCatalog)
        return juce::jlimit(0, kNumThemes - 1, stored);
    return stored >= 2 ? 10 : 0;
}
enum class ScalePlacement { Top, Bottom };   // legacy; Pitch & Scale lives in inspector

constexpr int kNumContentSizes = 3;
constexpr int kNumAppearances = kNumThemes;
constexpr int kNumScalePlacements = 2;

/** Legacy aggregate mapped 1:1 from ds::Palette — prefer ds:: accessors in new code. */
struct ThemeTokens
{
    juce::Colour bg, panel, panel2, elev;
    juce::Colour line, lineStrong, lineSoft;
    juce::Colour text, text2, text3;
    juce::Colour rollBg, rollShade, rollRowline, rollNoteEdge, rollGhost;
    juce::Colour kbWhite, kbBlack;
    juce::Colour toolbarTop, toolbarBot, sidebarTop, sidebarBot;
    juce::Colour tableAlt, desktopTop, desktopBot;
    juce::Colour windowBorder;
};

struct AccentTokens
{
    juce::Colour accent;
    juce::Colour ink;
    juce::Colour soft;
    juce::Colour line;
    juce::Colour bright;
};

/** Flat effects-inspector palette — derived from ds:: tokens. */
struct InspectorTokens
{
    juce::Colour panelBg;
    juce::Colour accent;
    juce::Colour headerText;
    juce::Colour rowLabel;
    juce::Colour valueText;
    juce::Colour divider;
    juce::Colour sliderTrack;
    juce::Colour sliderKnob;
    juce::Colour controlSurface;
    juce::Colour controlSurfaceHi;
    juce::Colour controlSeparator;
    juce::Colour switchOffTrack;
    juce::Colour chevron;
    juce::Colour segmentText;
    juce::Colour controlHairline;
    juce::Colour tallWell;
    juce::Colour tallFill;
    juce::Colour tallWellHot;
    bool dark = false;
};

const ThemeTokens& themeTokens();
const AccentTokens& accentTokens();
const InspectorTokens& inspectorTokens();

/** True for the ten dark themes (indexes 10–19). */
bool usesDarkAppearance();

/** Shift a base light/dark colour toward the active theme. Paper and Ink pass through. */
juce::Colour tintForTheme(juce::Colour c);

/** @deprecated Use usesDarkAppearance() */
inline bool inspectorUsesDarkPalette() { return usesDarkAppearance(); }

void drawInspectorControlSurface(juce::Graphics& g, juce::Rectangle<float> bounds,
                                 bool over, bool down);
void drawInspectorDivider(juce::Graphics& g, juce::Rectangle<int> bounds);

/** App-level runtime Tweaks (density / content size / appearance). */
enum class FontFamily : int { Archivo = 0, System = 1, Helvetica = 2, Avenir = 3, Count };
enum class SurfaceStyle : int { Flat = 0, Depth = 1, Liquid = 2, Count };

struct Tweaks
{
    std::atomic<int> density { (int) Density::Comfortable };
    std::atomic<int> size    { (int) ContentSize::Medium };
    /** Type + icon glyph scale as percent; 100% = former 115% size. Independent of UI size. */
    std::atomic<int> textScalePct { 100 };
    std::atomic<int> appearance { (int) Appearance::System };
    std::atomic<int> showTooltips { 1 }; // 1 = on
    std::atomic<int> fontFamily { (int) FontFamily::Archivo };
    std::atomic<int> surfaceStyle { (int) SurfaceStyle::Flat };
    /** Shade every other file-list row for readability. */
    std::atomic<int> alternateRows { 0 }; // 1 = on
};

/** Standout amber used for tip bubbles and the control highlight under them. */
inline juce::Colour tooltipAccent() { return juce::Colour(0xffffb020); }

/** Contrast-aware ink against a surface (approx WCAG AA for small text). */
juce::Colour contrastInkOn(juce::Colour surface);
/** Contrast-aware line / note edge against a surface. */
juce::Colour contrastLineOn(juce::Colour surface);

SurfaceStyle currentSurfaceStyle();

void setTooltipHighlightTarget(juce::Component* c);
bool hasTooltipHighlight(const juce::Component* c);

Tweaks& tweaks();

inline Density currentDensity()
{
    return (Density) juce::jlimit(0, 1, tweaks().density.load());
}

inline int themeIndex()
{
    return juce::jlimit(0, kNumThemes - 1, tweaks().appearance.load());
}

inline Appearance currentAppearance()
{
    return (Appearance) juce::jlimit(0, 2, tweaks().appearance.load());
}

/** UI scale for the Small / Medium / Large content-size tweak (layout / chrome). */
inline float contentScale()
{
    switch ((ContentSize) juce::jlimit(0, kNumContentSizes - 1, tweaks().size.load()))
    {
        case ContentSize::Small:  return 0.85f;
        case ContentSize::Medium: return 1.0f;
        case ContentSize::Large:  return 1.2f;
    }
    return 1.0f;
}

/** Text & icon glyph scale from Settings → Text size (%).
    100% matches the former 115% size; choices are relative to that baseline. */
inline float textIconScale()
{
    const int pct = juce::jlimit(60, 150, tweaks().textScalePct.load());
    return ((float) pct / 100.0f) * 1.15f;
}

// ── Fonts (system stack) ─────────────────────────────────────────────────────

/** Groove Panel faces. weight: 400 / 500 / 600 / 700. mono = IBM Plex Mono (400 / 500). */
juce::Font grooveFont(float height, int weight, bool mono = false);
juce::Font uiFont(float pt, bool semibold = false);
juce::Font monoFont(float pt, bool semibold = false);   // tabular-nums system font
/** Same stack as uiFont/monoFont but ignores content-size scaling (Caps B2 inspector). */
juce::Font uiFontFixed(float pt, bool semibold = false);
juce::Font monoFontFixed(float pt, bool semibold = false);

enum class TextStyle
{
    LargeTitle,     // 28pt Semibold
    Title2,         // 17pt Semibold
    Headline,       // 14pt Semibold
    Body,           // 12pt Regular (Cupertino base)
    Callout,        // 13pt Regular
    Subheadline,    // 13pt Regular
    Footnote,       // 11pt Regular
    Caption         // 10.5pt Regular secondary
};

juce::Font fontFor(TextStyle s);

inline juce::Font systemFont(float pt, bool semibold = false) { return uiFont(pt, semibold); }

// ── Colour accessors ─────────────────────────────────────────────────────────

namespace colours {
    // Prefer ds:: tokens; these aliases keep existing call sites compiling.
    inline juce::Colour bg()            { return ds::bg(); }
    inline juce::Colour panel()         { return ds::panel(); }
    inline juce::Colour panel2()        { return ds::rollchrome(); }
    inline juce::Colour elev()          { return ds::ctl(); }
    inline juce::Colour line()          { return ds::hl(); }
    inline juce::Colour lineStrong()    { return ds::hl(); }
    inline juce::Colour lineSoft()      { return ds::hl().withAlpha(ds::hl().getFloatAlpha() * 0.55f); }

    inline juce::Colour text()          { return ds::tx(); }
    inline juce::Colour text2()         { return ds::tx2(); }
    inline juce::Colour text3()         { return ds::tx3(); }

    inline juce::Colour accent()        { return ds::acc(); }
    /** Text/icons on accent fills — dark in dark mode for contrast on blue. */
    inline juce::Colour accentInk()     { return ds::onAccent(); }
    inline juce::Colour accentSoft()    { return ds::selsoft(); }
    inline juce::Colour accentLine()    { return ds::acc().withAlpha(0.40f); }
    inline juce::Colour accentBright()  { return ds::note(); }

    inline juce::Colour rollBg()        { return ds::panel(); }
    inline juce::Colour rollShade()     { return ds::laneb(); }
    inline juce::Colour rollRowline()   { return ds::hl().withAlpha(ds::hl().getFloatAlpha() * 0.45f); }
    inline juce::Colour rollNoteEdge()  { return ds::note().darker(0.25f); }
    inline juce::Colour rollGhost()     { return ds::hl(); }
    inline juce::Colour kbWhite()       { return ds::kw(); }
    inline juce::Colour kbBlack()       { return ds::kb(); }

    inline juce::Colour toolbarTop()    { return ds::chrome(); }
    inline juce::Colour toolbarBot()    { return ds::chrome(); }
    inline juce::Colour sidebarTop()    { return ds::side(); }
    inline juce::Colour sidebarBot()    { return ds::side(); }
    inline juce::Colour tableAlt()      { return ds::rowalt(); }
    inline juce::Colour desktopTop()    { return ds::bg(); }
    inline juce::Colour desktopBot()    { return ds::bg(); }
    inline juce::Colour windowBorder()  { return ds::winbrd(); }

    /** Flagged: traffic-light red is allowed by §1 but not named in §2 — keep as system red. */
    inline juce::Colour playhead()      { return ds::play(); } // Groove: the playhead moves with the music

    inline juce::Colour bgLight()       { return panel2(); }
    inline juce::Colour bgLighter()     { return elev(); }
    inline juce::Colour panelBorder()   { return line(); }
    inline juce::Colour borderHover()   { return lineStrong(); }
    inline juce::Colour separator()     { return line(); }
    inline juce::Colour accentDim()     { return accentSoft(); }
    inline juce::Colour onPrimary()     { return accentInk(); }
    inline juce::Colour textDim()       { return text2(); }
    inline juce::Colour textBright()    { return text(); }
    inline juce::Colour textMuted()     { return text3(); }
    inline juce::Colour controlFill()   { return ds::ctl(); }
    inline juce::Colour knobTrack()     { return ds::trk(); }
    inline juce::Colour compSelectionHighlight() { return accentSoft(); }
    inline juce::Colour pianoWhiteKey() { return kbWhite(); }
    inline juce::Colour pianoBlackKey() { return kbBlack(); }
    inline juce::Colour pianoGrid()     { return rollRowline(); }
    inline juce::Colour noteBlock()     { return ds::note(); }
    inline juce::Colour selection()     { return accentSoft(); }
    inline juce::Colour muteInactive()  { return controlFill(); }
    /** Flagged: traffic yellow/green (§1) — not in §2 table; derived as stron / kindKeys. */
    inline juce::Colour muteYellow()    { return ds::active(); }
    inline juce::Colour soloBlue()      { return ds::solo(); }
    inline juce::Colour recordRed()     { return ds::record(); }
    inline juce::Colour activeGreen()   { return ds::play(); }
    inline juce::Colour muteRed()       { return muteYellow(); }
    inline juce::Colour soloGreen()     { return activeGreen(); }

    inline juce::Colour bpBg()          { return rollBg(); }
    inline juce::Colour bpBar()         { return lineStrong(); }
    inline juce::Colour bpBeat()        { return ds::beat(); }
    inline juce::Colour bpRow()         { return rollShade(); }
    inline juce::Colour bpNote()        { return ds::note(); }
    inline juce::Colour bpEdge()        { return rollNoteEdge(); }
}

// ── Metrics ──────────────────────────────────────────────────────────────────

namespace metrics {
    inline int scaled(int px)
    {
        return juce::roundToInt((float) px * contentScale());
    }

    constexpr float uiScale         = 1.0f;

    constexpr float cornerRadius    = ds::radius::md;
    constexpr float windowRadius    = 12.0f;
    constexpr float groupedRadius   = ds::radius::md;
    constexpr float chipRadius      = ds::radius::sm;
    constexpr float controlRadius   = ds::radius::sm;
    constexpr float browserFontSize = 12.0f;

    constexpr int grid              = 8;
    constexpr int pluginPad         = 16;
    constexpr int sectionHeaderH    = 24;
    constexpr int toolbarH          = ds::region::toolbarH;
    constexpr int toolbarGap        = 8;
    constexpr int previewH          = ds::region::previewH;
    constexpr int previewControlsH  = 32;
    constexpr int editorHeaderH     = 48;
    constexpr int iconButtonSize    = 24;
    constexpr int comboTextPadding  = 6;

    // Logical (unscaled) layout widths — §5 regions.
    constexpr int browserWidth      = ds::region::fileListW;
    constexpr int browserMinWidth   = 260;
    constexpr int browserMaxWidth   = 420;
    constexpr int sidebarW          = ds::region::sidebarW;
    constexpr int sidebarRailW      = 44;
    constexpr int sidebarExpandedW  = ds::region::sidebarW;
    constexpr int fileTableW        = ds::region::fileListW;
    /** File browser does not grow past this; extra width goes to the piano roll. */
    constexpr int fileTableMaxW     = 640;
    constexpr int editorPaneW       = 470;
    constexpr int effectsPaneW      = ds::region::toolkitW;
    constexpr int openRollW         = editorPaneW;
    constexpr int openRollMinW      = 360;
    constexpr int foldedWindowW     = sidebarRailW + fileTableW;
    constexpr int openWindowW       = sidebarExpandedW + fileTableW + editorPaneW;
    /** Header chrome icon button (play / effects / etc.). */
    constexpr int chromeIconBtn     = 24;   // control-lg
    /** Drawn glyph size inside a chromeIconBtn with iconScale 1. */
    constexpr int chromeIconGlyph   = 12;

    inline int transportH()       { return scaled(toolbarH); }
    inline int listRowH()         { return scaled(ds::region::listRowH); }
    inline int listHeaderH()      { return scaled(ds::region::listHeaderH); }
    inline int paneHeaderH()      { return scaled(28); }
    inline int toolkitHeaderH()   { return scaled(24); }
    inline int padS()             { return scaled(currentDensity() == Density::Comfortable ? 12 : 8); }
    inline int miniRollH()        { return scaled(ds::region::previewH); }
    /** Scale with Text & icons setting so chrome stays proportional. */
    inline int sidebarRailWidth() { return scaled(sidebarRailW); }
    inline int sidebarExpandedWidth() { return scaled(sidebarExpandedW); }
    inline int chromeIconButton() { return scaled(chromeIconBtn); }
    inline int chromeIconGlyphSize()
    {
        return juce::roundToInt((float) chromeIconGlyph * contentScale() * textIconScale());
    }
    inline int fileTableWidth()   { return scaled(fileTableW); }
    inline int fileTableMaxWidth(){ return scaled(fileTableMaxW); }
    inline int editorPaneWidth()  { return scaled(editorPaneW); }
    inline int effectsPaneWidth() { return scaled(effectsPaneW); }
    inline int browserMinWidthScaled() { return scaled(browserMinWidth); }
    inline int openRollMinWidth() { return scaled(openRollMinW); }
}

inline void styleSectionLabel(juce::Label& lbl, const juce::String& text)
{
    lbl.setText(text, juce::dontSendNotification);
    lbl.setFont(fontFor(TextStyle::Headline));
    lbl.setJustificationType(juce::Justification::centredLeft);
    lbl.setColour(juce::Label::textColourId, colours::textDim());
    lbl.setInterceptsMouseClicks(false, false);
}

namespace higIcon {
    inline constexpr const char* star         = "star";
    inline constexpr const char* chevronUp    = "chevron.up";
    inline constexpr const char* chevronDown  = "chevron.down";
    inline constexpr const char* scissors     = "scissors";
    inline constexpr const char* speakerSlash = "speaker.slash";
}

class PatternFlowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PatternFlowLookAndFeel();
    void refreshColours();

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& bg,
                              bool highlighted, bool down) override;

    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;

    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

    void drawFileBrowserRow(juce::Graphics&, int width, int height,
                            const juce::File& file, const juce::String& filename, juce::Image* icon,
                            const juce::String& fileSizeDescription,
                            const juce::String& fileTimeDescription,
                            bool isDirectory, bool isItemSelected,
                            int itemIndex,
                            juce::DirectoryContentsDisplayComponent& dcc) override;

    void fillTextEditorBackground(juce::Graphics&, int width, int height,
                                  juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height,
                               juce::TextEditor&) override;

    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void preparePopupMenuWindow(juce::Component& window) override;
    int getPopupMenuBorderSize() override;

    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText, const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override;

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                   int standardMenuItemHeight, int& idealWidth, int& idealHeight) override;

    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;

    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton& btn, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    static void drawSymbol(juce::Graphics& g, const juce::String& name,
                           juce::Rectangle<float> bounds, juce::Colour colour, float stroke = 1.5f);
};

} // namespace pflow
