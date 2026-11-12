#include "DesignSystem.h"
#include "Theme.h"

namespace pflow {

bool usesDarkAppearance(); // Theme.cpp
juce::Font uiFontFixed(float pt, bool semibold);
juce::Font monoFontFixed(float pt, bool semibold);

namespace ds {

namespace {

juce::Colour rgba(juce::uint8 r, juce::uint8 g, juce::uint8 b, float a)
{
    return juce::Colour(r, g, b).withAlpha(a);
}

/** Surface separation: push a neutral layer further from (or closer to) the base
    background. Opaque colours scale their per-channel distance from base;
    translucent washes scale their alpha. */
juce::Colour separate(juce::Colour c, juce::Colour base)
{
    const float k = (float) juce::jlimit(50, 300, tweaks().separationPct.load()) / 100.0f;
    if (std::abs(k - 1.0f) < 0.001f) return c;
    if (c.getFloatAlpha() < 0.999f)
        return c.withAlpha(juce::jlimit(0.0f, 1.0f, c.getFloatAlpha() * k));
    auto ch = [k](juce::uint8 v, juce::uint8 b)
    {
        return (juce::uint8) juce::jlimit(0, 255, juce::roundToInt((float) b + ((float) v - (float) b) * k));
    };
    return juce::Colour(ch(c.getRed(), base.getRed()), ch(c.getGreen(), base.getGreen()),
                        ch(c.getBlue(), base.getBlue()), c.getFloatAlpha());
}

} // namespace

const Palette& palette()
{
    // Levels design system (DESIGN.md) mapped onto the legacy slots.
    // Light = Levels tokens; neutrals are opacity steps of text #111827 on surface #FFFFFF.
    // Dark  = derived: ground = primary #27272A, text = surface #FFFFFF, same opacity steps.
    // acc = secondary violet = the ACTIVE state (drawn as border + selsoft tint, never a fill
    // behind text). Solid actions use primary() / onPrimary(). Hex lives only in this file.
    static const Palette light {
        juce::Colour(0xffffffff),   // bg        = surface
        juce::Colour(0xffffffff),   // chrome    = surface
        juce::Colour(0xfff6f6f6),   // side      = text @ 4 %
        juce::Colour(0xffffffff),   // panel     = surface
        juce::Colour(0xfffafafb),   // rollchrome= text @ 2 %
        juce::Colour(0xffe7e8e9),   // hl        = text @ 10 %
        juce::Colour(0xff111827),   // tx        = text
        juce::Colour(0xff676b75),   // tx2       = text @ 64 %
        juce::Colour(0xff676b75),   // tx3       = text @ 64 % (lowest text step)
        juce::Colour(0xff8b5cf6),   // acc       = secondary (active)
        juce::Colour(0xfff1ebfe),   // selsoft   = secondary @ 12 %
        juce::Colour(0xffffffff),   // ctl       = surface
        juce::Colour(0xffd4d5d8),   // ctlb      = text @ 18 %
        juce::Colour(0xffffffff),   // field     = surface
        juce::Colour(0xffd4d5d8),   // trk       = text @ 18 %
        rgba(17, 24, 39, 0.03f),    // rowalt
        juce::Colour(0xff27272a),   // note      = primary
        juce::Colour(0xffd97706),   // stron     = warning (stars)
        juce::Colour(0xffffffff),   // kw
        juce::Colour(0xff27272a),   // kb        = primary
        rgba(17, 24, 39, 0.04f),    // laneb
        juce::Colour(0xffdedfe1),   // beat      = text @ 14 %
        juce::Colour(0xffdc2626),   // kindDrums = danger
        juce::Colour(0xff8b5cf6),   // kindBass  = secondary
        juce::Colour(0xff16a34a),   // kindKeys  = success
        juce::Colour(0xffd97706),   // kindPerc  = warning (Lead)
        juce::Colour(0xffd4d5d8),   // winbrd    = text @ 18 %
        rgba(17, 24, 39, 0.07f),    // hoverWash
        juce::Colour(0xffffffff),   // head      = surface (flat section headers)
        juce::Colour(0xfff6f6f6),   // well      = text @ 4 %
        juce::Colour(0xff27272a),   // kfInW     = primary (in key)
        juce::Colour(0xff27272a),   // kfInB
        juce::Colour(0xffffffff),   // kfOutW
        juce::Colour(0xffd4d5d8),   // kfOutB
        juce::Colour(0xfff6f6f6),   // lensFill
        juce::Colour(0xff8b5cf6),   // logoNoteHi
    };
    static const Palette dark {
        juce::Colour(0xff27272a),   // bg        = mockup ground ~#272729
        juce::Colour(0xff27272a),   // chrome
        juce::Colour(0xff303033),   // side      = raised ~#303032
        juce::Colour(0xff27272a),   // panel
        juce::Colour(0xff2b2b2e),   // rollchrome
        juce::Colour(0xff3d3d3f),   // hl
        juce::Colour(0xffffffff),   // tx
        juce::Colour(0xffb1b1b2),   // tx2
        juce::Colour(0xffb1b1b2),   // tx3
        juce::Colour(0xff7b5cff),   // acc       = mockup violet
        juce::Colour(0xff3f3466),   // selsoft
        juce::Colour(0xff27272a),   // ctl
        juce::Colour(0xff4e4e50),   // ctlb
        juce::Colour(0xff27272a),   // field
        juce::Colour(0xff4e4e50),   // trk
        rgba(255, 255, 255, 0.03f), // rowalt
        juce::Colour(0xffffffff),   // note
        juce::Colour(0xffd97706),   // stron
        juce::Colour(0xff303033),   // kw
        juce::Colour(0xff5b5b5e),   // kb
        rgba(255, 255, 255, 0.05f), // laneb
        juce::Colour(0xff454548),   // beat
        juce::Colour(0xffdc2626),   // kindDrums
        juce::Colour(0xff7b5cff),   // kindBass
        juce::Colour(0xff16a34a),   // kindKeys
        juce::Colour(0xffd97706),   // kindPerc
        juce::Colour(0xff4e4e50),   // winbrd
        rgba(255, 255, 255, 0.07f), // hoverWash
        juce::Colour(0xff27272a),   // head
        juce::Colour(0xff303033),   // well
        juce::Colour(0xffffffff),   // kfInW
        juce::Colour(0xffffffff),   // kfInB
        juce::Colour(0xff27272a),   // kfOutW
        juce::Colour(0xff4e4e50),   // kfOutB
        juce::Colour(0xff303033),   // lensFill
        juce::Colour(0xff7b5cff),   // logoNoteHi
    };

    auto tinted = [](const Palette& src)
    {
        Palette p = src;
#define SEP(field) p.field = separate(p.field, src.bg)
        SEP(side); SEP(rollchrome); SEP(hl); SEP(selsoft); SEP(ctlb); SEP(trk);
        SEP(rowalt); SEP(laneb); SEP(beat); SEP(winbrd); SEP(hoverWash); SEP(well);
#undef SEP
#define TINT(field) p.field = tintForTheme(p.field)
        TINT(bg); TINT(chrome); TINT(side); TINT(panel); TINT(rollchrome); TINT(hl);
        TINT(tx); TINT(tx2); TINT(tx3); TINT(acc); TINT(selsoft); TINT(ctl); TINT(ctlb);
        TINT(field); TINT(trk); TINT(rowalt); TINT(note); TINT(stron);
        TINT(laneb); TINT(beat);
        TINT(kindDrums); TINT(kindBass); TINT(kindKeys); TINT(kindPerc);
        TINT(winbrd); TINT(hoverWash); TINT(head); TINT(well);
        TINT(kfInW); TINT(kfInB); TINT(kfOutW); TINT(kfOutB);
        TINT(lensFill); TINT(logoNoteHi);
        // Piano keys stay literal white/black in every theme (legibility).
        p.kw = juce::Colours::white;
        p.kb = juce::Colours::black;
#undef TINT
        return p;
    };

    static Palette cached;
    static int cachedTheme = -1;
    static int cachedSeparation = -1;
    const int theme = themeIndex();
    const int separation = tweaks().separationPct.load();
    if (theme != cachedTheme || separation != cachedSeparation)
    {
        const auto& spec = themeAt(theme);
        cached = tinted(spec.dark ? dark : light);

        // Skill-pack overrides: rebuild neutrals around the theme surface/text/accent.
        if (spec.surface != 0)
        {
            const auto surf = juce::Colour(spec.surface);
            cached.bg = surf;
            cached.chrome = surf;
            cached.panel = surf;
            cached.ctl = surf;
            cached.field = surf;
            cached.head = surf;
            cached.lensFill = surf.interpolatedWith(spec.dark ? juce::Colours::white
                                                              : juce::Colours::black, 0.04f);
            cached.side = surf.interpolatedWith(spec.dark ? juce::Colours::white
                                                          : juce::Colours::black, 0.05f);
            cached.well = cached.side;
            cached.rollchrome = surf.interpolatedWith(spec.dark ? juce::Colours::white
                                                                : juce::Colours::black, 0.03f);
        }
        if (spec.text != 0)
        {
            const auto ink = juce::Colour(spec.text);
            cached.tx = ink;
            cached.tx2 = ink.withMultipliedAlpha(0.70f).overlaidWith(cached.bg.withAlpha(0.0f));
            cached.tx2 = cached.bg.interpolatedWith(ink, 0.64f);
            cached.tx3 = cached.bg.interpolatedWith(ink, 0.48f);
            cached.hl = cached.bg.interpolatedWith(ink, 0.14f);
            cached.ctlb = cached.bg.interpolatedWith(ink, 0.18f);
            cached.trk = cached.ctlb;
            cached.winbrd = cached.ctlb;
            cached.beat = cached.bg.interpolatedWith(ink, 0.16f);
            cached.rowalt = ink.withAlpha(spec.dark ? 0.04f : 0.03f);
            cached.laneb = ink.withAlpha(spec.dark ? 0.07f : 0.05f);
            cached.hoverWash = ink.withAlpha(0.07f);
        }
        if (spec.accent != 0)
        {
            cached.acc = juce::Colour(spec.accent);
            cached.selsoft = cached.bg.interpolatedWith(cached.acc, spec.dark ? 0.28f : 0.14f);
            cached.kindBass = cached.acc;
            cached.logoNoteHi = cached.acc;
        }
        if (spec.primary != 0)
        {
            cached.note = juce::Colour(spec.primary);
            cached.kfInW = cached.note;
            cached.kfInB = cached.note;
        }
        // Prefer accent for note blocks when primary is too close to the roll surface
        // (e.g. near-white/black primaries) so notes stay legible.
        if (cached.note.getPerceivedBrightness() > 0.82f
            || std::abs(cached.note.getPerceivedBrightness() - cached.panel.getPerceivedBrightness()) < 0.18f)
        {
            if (spec.accent != 0)
                cached.note = juce::Colour(spec.accent);
        }
        // Keys never follow skill-pack surface/text overrides.
        cached.kw = juce::Colours::white;
        cached.kb = juce::Colours::black;

        cachedTheme = theme;
        cachedSeparation = separation;
    }
    return cached;
}

// ── Groove Panel tokens not covered by the legacy slots ─────────────────────

#define GP_SEP_TOKEN(fn, darkHex, lightHex) \
    juce::Colour fn() { \
        const bool d = usesDarkAppearance(); \
        const auto base = juce::Colour(d ? (juce::uint32) darkHex : (juce::uint32) lightHex); \
        const auto ground = juce::Colour(d ? 0xff27272au : 0xffffffffu); \
        return tintForTheme(separate(base, ground)); \
    }

#define GP_TOKEN(fn, darkHex, lightHex) \
    juce::Colour fn() { \
        const auto base = juce::Colour(usesDarkAppearance() ? (juce::uint32) darkHex : (juce::uint32) lightHex); \
        return tintForTheme(base); \
    }

namespace {
juce::Colour themedPrimaryColour()
{
    const auto& spec = themeAt(themeIndex());
    if (spec.primary != 0)
        return juce::Colour(spec.primary);
    if (spec.accent != 0)
        return juce::Colour(spec.accent);
    return juce::Colour(usesDarkAppearance() ? 0xffffffffu : 0xff27272au);
}
} // namespace

// Surfaces / ink follow the active theme palette (classic tint or skill-pack).
juce::Colour surface000() { return palette().bg; }
juce::Colour surface100() { return palette().side; }
juce::Colour surface200() { return palette().ctl; }
juce::Colour surface300() { return palette().rollchrome.interpolatedWith(palette().tx, 0.08f); }
juce::Colour line()       { return palette().hl; }
juce::Colour edge()
{
    auto e = palette().ctlb;
    const float w = themeBorderPx();
    if (w > 1.5f) // brutal themes: stronger edge
        e = palette().tx.withAlpha(usesDarkAppearance() ? 0.55f : 0.35f);
    return e;
}
juce::Colour ink()      { return palette().tx; }
juce::Colour inkMuted() { return palette().tx2; }
juce::Colour inkFaint() { return palette().tx3; }
juce::Colour onAccent() { return juce::Colour(0xffffffff); }
GP_TOKEN(play,       0xff16a34a, 0xff16a34a)   // success: playhead, previewing
GP_TOKEN(record,     0xffdc2626, 0xffdc2626)   // danger
GP_TOKEN(active,     0xffd97706, 0xffd97706)   // warning
juce::Colour onLit() { return juce::Colour(0xffffffff); }
GP_TOKEN(focus,      0xffffffff, 0xff27272a)   // 2 px primary ring
juce::Colour primary() { return themedPrimaryColour(); }
juce::Colour onPrimary() { return contrastInkOn(primary()); }
juce::Colour solo()
{
    const auto& spec = themeAt(themeIndex());
    if (spec.accent != 0)
        return juce::Colour(spec.accent);
    return tintForTheme(juce::Colour(0xff7b5cff));
}
GP_TOKEN(warning,    0xffd97706, 0xffd97706)

#undef GP_TOKEN
#undef GP_SEP_TOKEN

juce::Colour clip(int n)
{
    static const juce::uint32 c[] = { 0xffff7a6b, 0xffffa64d, 0xfff3dc5a, 0xff9fe06e,
                                      0xff52d1c2, 0xff6fb5f0, 0xffb99af2, 0xfff58bbd };
    return juce::Colour(c[juce::jlimit(1, 8, n) - 1]);
}

juce::Colour onClip() { return juce::Colour(0xff141414); }

void drawActive(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    // Clip the tint fill so AA corners never reveal a white parent behind.
    juce::Path path;
    path.addRoundedRectangle(r, radius);
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(path);
        g.setColour(selsoft());
        g.fillRect(r);
    }
    g.setColour(acc());
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
}

namespace shadow {

namespace {
void paintDrop(juce::Graphics& g, juce::Rectangle<float> r, float radius,
               float dy, float blur, float alpha)
{
    juce::ignoreUnused(blur);
    g.setColour(juce::Colours::black.withAlpha(alpha));
    g.fillRoundedRectangle(r.translated(0.0f, dy), radius);
}

void paintNeo(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    // Soft extruded pair: light top-left, dark bottom-right.
    g.setColour(juce::Colours::white.withAlpha(usesDarkAppearance() ? 0.06f : 0.55f));
    g.drawRoundedRectangle(r.translated(-1.0f, -1.0f), radius, 1.2f);
    g.setColour(juce::Colours::black.withAlpha(usesDarkAppearance() ? 0.35f : 0.12f));
    g.drawRoundedRectangle(r.translated(1.5f, 1.5f), radius, 1.4f);
}
} // namespace

void control(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    switch (themeDepth())
    {
        case ThemeDepth::Soft:
            paintDrop(g, r, radius, 1.0f, 4.0f, 0.12f);
            break;
        case ThemeDepth::Glass:
            g.setColour(juce::Colours::white.withAlpha(usesDarkAppearance() ? 0.08f : 0.35f));
            g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
            paintDrop(g, r, radius, 2.0f, 8.0f, 0.10f);
            break;
        case ThemeDepth::Neo:
            paintNeo(g, r, radius);
            break;
        case ThemeDepth::Brutal:
        {
            // Hard offset block sits under the face — caller should inset `r`
            // so this stays inside the component clip.
            const float o = juce::jmin(3.0f, themeBorderPx());
            g.setColour(ink().withAlpha(0.90f));
            g.fillRoundedRectangle(r.translated(o, o), 0.0f);
            break;
        }
        case ThemeDepth::Flat:
        default:
            break;
    }
}

void card(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    control(g, r, radius);
}

void popover(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    // Inset so thick brutal borders / offset shadows never clip at the window edge.
    const float border = juce::jmax(1.0f, themeBorderPx());
    const float inset = themeDepth() == ThemeDepth::Brutal ? border + 1.0f : 0.0f;
    auto face = r.reduced(inset);
    switch (themeDepth())
    {
        case ThemeDepth::Soft:
        case ThemeDepth::Glass:
            paintDrop(g, face.expanded(1.0f), radius, 4.0f, 16.0f, 0.18f);
            break;
        case ThemeDepth::Neo:
            paintNeo(g, face, radius);
            break;
        case ThemeDepth::Brutal:
            g.setColour(ink());
            g.fillRoundedRectangle(face.translated(border, border), 0.0f);
            break;
        case ThemeDepth::Flat:
        default:
            break;
    }
    g.setColour(edge());
    g.drawRoundedRectangle(face.reduced(0.5f), radius, border);
}

void thumb(juce::Graphics& g, juce::Rectangle<float> thumb)
{
    // Levels dual-range handle: 12 px surface disc, 2 px primary ring.
    const float d = 12.0f; juce::ignoreUnused(thumb.getWidth());
    const auto c = juce::Rectangle<float>(d, d).withCentre(thumb.getCentre());
    g.setColour(surface100());
    g.fillEllipse(c);
    g.setColour(primary());
    g.drawEllipse(c.reduced(1.0f), 2.0f);
}

} // namespace shadow

juce::Font font(Type t)
{
    // Groove Panel type scale (label 11 / body 12 / title 13 / readout 11 mono / readout-lg 15 mono).
    switch (t)
    {
        case Type::AppName:        return uiFontFixed(14.0f, true);
        case Type::PanelHeading:   return uiFontFixed(16.0f, true);   // browser folder title
        case Type::SectionLabel:   return monoFontFixed(11.0f).withExtraKerningFactor(0.04f);  // label-caps
        case Type::TableHeader:    return monoFontFixed(11.0f).withExtraKerningFactor(0.04f);  // label-caps
        case Type::Body:           return uiFontFixed(12.0f, false);  // body
        case Type::Metadata:       return monoFontFixed(12.0f);       // readout
        case Type::Caption:        return uiFontFixed(11.0f, false);  // label / filter summary
        case Type::NumericDisplay: return monoFontFixed(14.0f, true); // BPM readout
        case Type::Logo:           return grooveFont(14.0f * textIconScale(), 600).withExtraKerningFactor(-0.01f);
    }
    return uiFontFixed(12.0f, false);
}

juce::Colour colour(Type t)
{
    switch (t)
    {
        case Type::AppName:        return tx2();
        case Type::PanelHeading:   return tx();
        case Type::SectionLabel:   return tx3();
        case Type::TableHeader:    return tx3();
        case Type::Body:           return tx();
        case Type::Metadata:       return tx2();
        case Type::Caption:        return tx3();
        case Type::NumericDisplay: return tx();
        case Type::Logo:           return tx();
    }
    return tx();
}

} // namespace ds
} // namespace pflow
