#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "DesignSystem.h"

namespace pflow {

// ── Cupertino design tokens (MidiToolkit Design System v1.0) ─────────────────
// Source of truth: DesignSystem.h / DesignSystem.cpp (§2). Density tweaks remain.

enum class Density { Compact, Comfortable };
enum class ContentSize { Small, Medium, Large };
/** Settings → Appearance: Auto follows the OS; Light/Dark pick a family. */
enum class Appearance { System = 0, Light = 1, Dark = 2 };

/** Depth / chrome personality for skill themes (layout stays the same). */
enum class ThemeDepth : uint8_t { Flat = 0, Soft = 1, Glass = 2, Neo = 3, Brutal = 4 };
/** Preferred interface typeface family for a theme. */
enum class ThemeFace : uint8_t { Sans = 0, Rounded = 1, Mono = 2, Serif = 3 };

/** Named themes. First half of the catalog is light, second half is dark.
    Indexes 0–9 / 30–39 are the classic tint themes; 10–29 / 40–59 are skill packs. */
struct ThemeSpec
{
    const char* name;
    bool dark;
    float hue;     // classic tint hue (0–1)
    float amount;  // classic tint strength
    float lift;    // classic brightness lift
    /** 0 = keep Levels token; otherwise explicit ARGB override. */
    juce::uint32 accent = 0;
    juce::uint32 surface = 0;
    juce::uint32 text = 0;
    juce::uint32 primary = 0; // solid actions / note fill preference
    float radiusSm = 0.0f;    // 0 → 4
    float radiusMd = 0.0f;    // 0 → 8
    float spaceScale = 1.0f;  // padding/gap multiplier
    ThemeDepth depth = ThemeDepth::Flat;
    ThemeFace face = ThemeFace::Sans;
    float borderPx = 1.0f;
};

// Skill-pack helpers (light / dark pairs from ~/.agents/awesome-design-skills).
#define PF_SKILL_L(name, acc, surf, txt, prim, rSm, rMd, space, depth, face, border) \
    { name, false, 0, 0, 0, acc, surf, txt, prim, rSm, rMd, space, depth, face, border }
#define PF_SKILL_D(name, acc, surf, txt, prim, rSm, rMd, space, depth, face, border) \
    { name, true,  0, 0, 0, acc, surf, txt, prim, rSm, rMd, space, depth, face, border }

inline constexpr ThemeSpec kThemes[] = {
    // ── Classic light (0–9) ──────────────────────────────────────────────────
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

    // ── Skill light (10–29) — distinctive packs for dense MIDI/tool UIs ──────
    // Chosen for max variety (depth/face/spacing/color); each has a dark twin.
    PF_SKILL_L("Minimal",      0xff312c85, 0xfff4f4f1, 0xff0c0c09, 0xff0c0c09, 4, 8, 1.15f, ThemeDepth::Flat,   ThemeFace::Sans,    1.0f),
    PF_SKILL_L("Clean",        0xff6366f1, 0xfff8fafc, 0xff0f172a, 0xff2563eb, 6, 12,1.20f, ThemeDepth::Soft,   ThemeFace::Rounded, 1.0f),
    PF_SKILL_L("Professional", 0xff111111, 0xffffffff, 0xff111827, 0xfffece14, 4, 8, 0.95f, ThemeDepth::Soft,   ThemeFace::Sans,    1.0f),
    PF_SKILL_L("Material",     0xffc8b3fd, 0xfff7f5ff, 0xff1e1b4b, 0xff6442d6, 6, 12,1.05f, ThemeDepth::Soft,   ThemeFace::Sans,    1.0f),
    PF_SKILL_L("Shadcn",       0xff18181b, 0xffffffff, 0xff09090b, 0xff09090b, 4, 8, 1.00f, ThemeDepth::Flat,   ThemeFace::Sans,    1.0f),
    PF_SKILL_L("Flat",         0xff8b5cf6, 0xffffffff, 0xff111827, 0xfff2673c, 2, 4, 1.00f, ThemeDepth::Flat,   ThemeFace::Sans,    1.0f),
    PF_SKILL_L("Mono",         0xff00a6f4, 0xffe7e5e4, 0xff1c1917, 0xff37f712, 2, 4, 0.90f, ThemeDepth::Flat,   ThemeFace::Mono,    1.0f),
    PF_SKILL_L("Neumorph",     0xff006666, 0xffe7e5e4, 0xff1e2938, 0xff006666, 10,16,1.00f, ThemeDepth::Neo,    ThemeFace::Mono,    0.0f),
    PF_SKILL_L("Glass",        0xff3a344e, 0xffeef2ff, 0xff141414, 0xff1856ff, 8, 14,1.10f, ThemeDepth::Glass,  ThemeFace::Rounded, 1.0f),
    PF_SKILL_L("Neon",         0xff00bcff, 0xfff8fff0, 0xff0a2a1a, 0xff65a30d, 4, 8, 1.00f, ThemeDepth::Soft,   ThemeFace::Sans,    1.5f),
    PF_SKILL_L("Neobrutal",    0xff432dd7, 0xfffbfbf9, 0xff1c293c, 0xfffdc800, 0, 0, 1.00f, ThemeDepth::Brutal, ThemeFace::Sans,    2.5f),
    PF_SKILL_L("Editorial",    0xff111111, 0xfffffbf5, 0xff111111, 0xff111111, 2, 4, 1.15f, ThemeDepth::Flat,   ThemeFace::Serif,   1.0f),
    PF_SKILL_L("Terracotta",   0xffc56a3c, 0xfff3e9d8, 0xff1c1917, 0xffc56a3c, 4, 8, 1.05f, ThemeDepth::Soft,   ThemeFace::Serif,   1.0f),
    PF_SKILL_L("Pulse",        0xfff59e0b, 0xffffedd5, 0xff9a3412, 0xffea580b, 2, 4, 1.10f, ThemeDepth::Brutal, ThemeFace::Sans,    2.0f),
    PF_SKILL_L("Matrix",       0xff2db58a, 0xffe8f5ef, 0xff0b0c14, 0xff0b0c14, 2, 4, 0.92f, ThemeDepth::Flat,   ThemeFace::Mono,    1.0f),
    PF_SKILL_L("Vintage",      0xff008080, 0xffc0c0c0, 0xff000000, 0xff008080, 2, 4, 0.95f, ThemeDepth::Soft,   ThemeFace::Mono,    1.5f),
    PF_SKILL_L("Skeuo",        0xfff08321, 0xfff4efe8, 0xff1c1917, 0xfffa3c00, 6, 10,1.00f, ThemeDepth::Soft,   ThemeFace::Sans,    1.0f),
    PF_SKILL_L("Gradient",     0xffe60076, 0xfffaf5ff, 0xff1e1033, 0xff990ffa, 6, 12,1.10f, ThemeDepth::Glass,  ThemeFace::Rounded, 1.0f),
    PF_SKILL_L("Bold",         0xff009866, 0xfff4f7fb, 0xff0b1220, 0xff0077bc, 2, 4, 1.00f, ThemeDepth::Brutal, ThemeFace::Sans,    2.0f),
    PF_SKILL_L("Clay",         0xff3b82f6, 0xffeef4ff, 0xff1c398e, 0xff3b82f6, 12,20,1.15f, ThemeDepth::Neo,    ThemeFace::Rounded, 0.0f),

    // ── Classic dark (30–39) ─────────────────────────────────────────────────
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

    // ── Skill dark (40–59) ───────────────────────────────────────────────────
    PF_SKILL_D("Minimal",      0xffa5b4fc, 0xff0c0c09, 0xfff4f4f1, 0xffa5b4fc, 4, 8, 1.15f, ThemeDepth::Flat,   ThemeFace::Sans,    1.0f),
    PF_SKILL_D("Clean",        0xff818cf8, 0xff0f172a, 0xfff8fafc, 0xff60a5fa, 6, 12,1.20f, ThemeDepth::Soft,   ThemeFace::Rounded, 1.0f),
    PF_SKILL_D("Professional", 0xfffece14, 0xff111111, 0xfffafafa, 0xfffece14, 4, 8, 0.95f, ThemeDepth::Soft,   ThemeFace::Sans,    1.0f),
    PF_SKILL_D("Material",     0xffc8b3fd, 0xff1a1428, 0xfff5f3ff, 0xff8b6cff, 6, 12,1.05f, ThemeDepth::Soft,   ThemeFace::Sans,    1.0f),
    PF_SKILL_D("Shadcn",       0xfffafafa, 0xff09090b, 0xfffafafa, 0xfffafafa, 4, 8, 1.00f, ThemeDepth::Flat,   ThemeFace::Sans,    1.0f),
    PF_SKILL_D("Flat",         0xffc4b5fd, 0xff151515, 0xfff5f5f5, 0xfff2673c, 2, 4, 1.00f, ThemeDepth::Flat,   ThemeFace::Sans,    1.0f),
    PF_SKILL_D("Mono",         0xff00a6f4, 0xff0a0a0a, 0xff37f712, 0xff37f712, 2, 4, 0.90f, ThemeDepth::Flat,   ThemeFace::Mono,    1.0f),
    PF_SKILL_D("Neumorph",     0xff2dd4bf, 0xff1c1917, 0xffe7e5e4, 0xff14b8a6, 10,16,1.00f, ThemeDepth::Neo,    ThemeFace::Mono,    0.0f),
    PF_SKILL_D("Glass",        0xff93c5fd, 0xff0b1020, 0xffeef2ff, 0xff3b82f6, 8, 14,1.10f, ThemeDepth::Glass,  ThemeFace::Rounded, 1.0f),
    PF_SKILL_D("Neon",         0xff00bcff, 0xff050505, 0xffbbf351, 0xffbbf351, 4, 8, 1.00f, ThemeDepth::Soft,   ThemeFace::Sans,    1.5f),
    PF_SKILL_D("Neobrutal",    0xfffdc800, 0xff111111, 0xfffbfbf9, 0xff432dd7, 0, 0, 1.00f, ThemeDepth::Brutal, ThemeFace::Sans,    2.5f),
    PF_SKILL_D("Editorial",    0xfff5f5f5, 0xff111111, 0xfffffbf5, 0xfff5f5f5, 2, 4, 1.15f, ThemeDepth::Flat,   ThemeFace::Serif,   1.0f),
    PF_SKILL_D("Terracotta",   0xffe7a07a, 0xff1c1410, 0xfff3e9d8, 0xffc56a3c, 4, 8, 1.05f, ThemeDepth::Soft,   ThemeFace::Serif,   1.0f),
    PF_SKILL_D("Pulse",        0xfff59e0b, 0xff1c0a00, 0xffffedd5, 0xffea580b, 2, 4, 1.10f, ThemeDepth::Brutal, ThemeFace::Sans,    2.0f),
    PF_SKILL_D("Matrix",       0xff2db58a, 0xff0b0c14, 0xff2db58a, 0xff2db58a, 2, 4, 0.92f, ThemeDepth::Flat,   ThemeFace::Mono,    1.0f),
    PF_SKILL_D("Vintage",      0xff40e0d0, 0xff1a1a1a, 0xffc0c0c0, 0xff008080, 2, 4, 0.95f, ThemeDepth::Soft,   ThemeFace::Mono,    1.5f),
    PF_SKILL_D("Skeuo",        0xfff08321, 0xff1a120c, 0xfff4efe8, 0xfffa3c00, 6, 10,1.00f, ThemeDepth::Soft,   ThemeFace::Sans,    1.0f),
    PF_SKILL_D("Gradient",     0xffe60076, 0xff12081f, 0xfffaf5ff, 0xff990ffa, 6, 12,1.10f, ThemeDepth::Glass,  ThemeFace::Rounded, 1.0f),
    PF_SKILL_D("Bold",         0xff00c98a, 0xff0b1220, 0xfff4f7fb, 0xff0077bc, 2, 4, 1.00f, ThemeDepth::Brutal, ThemeFace::Sans,    2.0f),
    PF_SKILL_D("Clay",         0xff60a5fa, 0xff101828, 0xffdbeafe, 0xff3b82f6, 12,20,1.15f, ThemeDepth::Neo,    ThemeFace::Rounded, 0.0f),
};

#undef PF_SKILL_L
#undef PF_SKILL_D

inline constexpr int kNumThemes = (int) (sizeof(kThemes) / sizeof(kThemes[0]));
inline constexpr int kNumThemesPerFamily = 30; // 10 classic + 20 skill
inline constexpr int kPearlTheme = 5;
/** Default skill pack (family index): Neobrutal — light 20 / dark 50. */
inline constexpr int kNeobrutalTheme = 20;
inline constexpr int kInkTheme = 30; // first classic dark

inline const ThemeSpec& themeAt(int index)
{
    return kThemes[(size_t) juce::jlimit(0, kNumThemes - 1, index)];
}

/** Map legacy stored ids → Appearance Auto/Light/Dark. */
inline int migrateThemeIndex(int stored, bool themeCatalog)
{
    if (!themeCatalog)
        return juce::jlimit(0, 2, stored);
    // Absolute theme catalogs: light [0, N), dark [N, 2N).
    if (stored >= kNumThemesPerFamily) return (int) Appearance::Dark;
    return (int) Appearance::Light;
}

/** Favorite / starred glyph — solid gold, independent of accent tint. */
inline juce::Colour favoriteGold() { return ds::warning(); } // Levels amber
enum class ScalePlacement { Top, Bottom };   // legacy; Pitch & Scale lives in inspector

constexpr int kNumContentSizes = 3;
constexpr int kNumAppearances = 3; // Auto / Light / Dark
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
    /** Light-family theme index 0–29 (classic + skill packs). */
    std::atomic<int> lightTheme { kNeobrutalTheme };
    /** Dark-family theme index 0–29 → absolute 30–59. */
    std::atomic<int> darkTheme { kNeobrutalTheme };
    std::atomic<int> showTooltips { 1 }; // 1 = on
    std::atomic<int> fontFamily { (int) FontFamily::Archivo };
    std::atomic<int> surfaceStyle { (int) SurfaceStyle::Flat };
    /** Shade every other file-list row for readability. */
    std::atomic<int> alternateRows { 1 }; // 1 = on (default)
    /** Surface separation, 50–300 %. Scales how far neutral layers (sidebar, section
        cards, wells, lines, borders, selection tints) sit from the window background.
        Text and accents are untouched. 100 = Levels default. */
    std::atomic<int> separationPct { 100 };
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

inline Appearance currentAppearance()
{
    return (Appearance) juce::jlimit(0, 2, tweaks().appearance.load());
}

inline int lightThemeIndex()
{
    return juce::jlimit(0, kNumThemesPerFamily - 1, tweaks().lightTheme.load());
}

inline int darkThemeIndex()
{
    return kNumThemesPerFamily
         + juce::jlimit(0, kNumThemesPerFamily - 1, tweaks().darkTheme.load());
}

/** Resolve Appearance + light/dark theme prefs (Auto follows the OS). */
inline int themeIndex()
{
    switch (currentAppearance())
    {
        case Appearance::Light: return lightThemeIndex();
        case Appearance::Dark:  return darkThemeIndex();
        case Appearance::System:
        default:
            return juce::Desktop::getInstance().isDarkModeActive()
                       ? darkThemeIndex() : lightThemeIndex();
    }
}

/** Absolute theme index for the family currently shown in Settings. */
inline int settingsThemeFamilyIndex()
{
    switch (currentAppearance())
    {
        case Appearance::Light: return lightThemeIndex();
        case Appearance::Dark:  return darkThemeIndex();
        case Appearance::System:
        default:
            return juce::Desktop::getInstance().isDarkModeActive()
                       ? darkThemeIndex() : lightThemeIndex();
    }
}

inline bool settingsThemeFamilyIsDark()
{
    return themeAt(settingsThemeFamilyIndex()).dark;
}

inline float themeRadiusSm()
{
    const float v = themeAt(themeIndex()).radiusSm;
    return v > 0.0f ? v : ds::radius::sm;
}

inline float themeRadiusMd()
{
    const float v = themeAt(themeIndex()).radiusMd;
    return v > 0.0f ? v : ds::radius::md;
}

inline float themeSpaceScale()
{
    return juce::jlimit(0.75f, 1.4f, themeAt(themeIndex()).spaceScale);
}

inline ThemeDepth themeDepth() { return themeAt(themeIndex()).depth; }
inline ThemeFace themeFace()   { return themeAt(themeIndex()).face; }
inline float themeBorderPx()   { return juce::jmax(0.0f, themeAt(themeIndex()).borderPx); }

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
        return juce::roundToInt((float) px * contentScale() * themeSpaceScale());
    }

    constexpr float uiScale         = 1.0f;

    inline float cornerRadius()     { return themeRadiusMd(); }
    constexpr float windowRadius    = 12.0f;
    inline float groupedRadius()    { return themeRadiusMd(); }
    inline float chipRadius()       { return themeRadiusSm(); }
    inline float controlRadius()    { return themeRadiusSm(); }
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
    /** Drawn glyph size inside a chromeIconBtn with iconScale 1 (box stays 24). */
    constexpr int chromeIconGlyph   = 15;

    inline int transportH()       { return scaled(toolbarH); }
    inline int listRowH()         { return scaled(ds::region::listRowH); }
    inline int listHeaderH()      { return scaled(ds::region::listHeaderH); }
    /** Matches the transport header so Files / Generator / roll / Effects share one band. */
    inline int paneHeaderH()      { return transportH(); }
    inline int toolkitHeaderH()   { return paneHeaderH(); }
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
