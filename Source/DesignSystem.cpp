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
#define TINT(field) p.field = tintForTheme(p.field)
        TINT(bg); TINT(chrome); TINT(side); TINT(panel); TINT(rollchrome); TINT(hl);
        TINT(tx); TINT(tx2); TINT(tx3); TINT(acc); TINT(selsoft); TINT(ctl); TINT(ctlb);
        TINT(field); TINT(trk); TINT(rowalt); TINT(note); TINT(stron);
        TINT(kw); TINT(kb); TINT(laneb); TINT(beat);
        TINT(kindDrums); TINT(kindBass); TINT(kindKeys); TINT(kindPerc);
        TINT(winbrd); TINT(hoverWash); TINT(head); TINT(well);
        TINT(kfInW); TINT(kfInB); TINT(kfOutW); TINT(kfOutB);
        TINT(lensFill); TINT(logoNoteHi);
#undef TINT
        return p;
    };

    static Palette cached;
    static int cachedTheme = -1;
    static int cachedContrast = -1;
    const int theme = themeIndex();
    const int contrast = tweaks().contrastPct.load();
    if (theme != cachedTheme || contrast != cachedContrast)
    {
        cached = tinted(themeAt(theme).dark ? dark : light);
        cachedTheme = theme;
        cachedContrast = contrast;
    }
    return cached;
}

// ── Groove Panel tokens not covered by the legacy slots ─────────────────────

#define GP_TOKEN(fn, darkHex, lightHex) \
    juce::Colour fn() { \
        const auto base = juce::Colour(usesDarkAppearance() ? (juce::uint32) darkHex : (juce::uint32) lightHex); \
        return tintForTheme(base); \
    }

// Dark neutrals from mockup pixels: ground ~#272729, raised ~#303032.
GP_TOKEN(surface000, 0xff27272a, 0xffffffff)   // ground (header / panels)
GP_TOKEN(surface100, 0xff303033, 0xfff6f6f6)   // raised (sidebar, wells, toolkit)
GP_TOKEN(surface200, 0xff27272a, 0xffffffff)   // control face = ground
GP_TOKEN(surface300, 0xff363639, 0xffeeeff0)   // hover / grey count pills
GP_TOKEN(line,       0xff3d3d3f, 0xffe7e8e9)   // hairline
GP_TOKEN(edge,       0xff4e4e50, 0xffd4d5d8)   // edge
GP_TOKEN(ink,        0xffffffff, 0xff111827)   // text
GP_TOKEN(inkMuted,   0xffb1b1b2, 0xff676b75)   // secondary @ ~64 %
GP_TOKEN(inkFaint,   0xffb1b1b2, 0xff676b75)
juce::Colour onAccent() { return juce::Colour(0xffffffff); }
GP_TOKEN(play,       0xff16a34a, 0xff16a34a)   // success: playhead, previewing
GP_TOKEN(record,     0xffdc2626, 0xffdc2626)   // danger
GP_TOKEN(solo,       0xff7b5cff, 0xff7b5cff)   // mockup violet
GP_TOKEN(active,     0xffd97706, 0xffd97706)   // warning
juce::Colour onLit() { return juce::Colour(0xffffffff); }
GP_TOKEN(focus,      0xffffffff, 0xff27272a)   // 2 px primary ring
GP_TOKEN(primary,    0xffffffff, 0xff27272a)   // solid actions: Play, Drag Me
GP_TOKEN(onPrimary,  0xff27272a, 0xffffffff)
GP_TOKEN(warning,    0xffd97706, 0xffd97706)

#undef GP_TOKEN

juce::Colour clip(int n)
{
    static const juce::uint32 c[] = { 0xffff7a6b, 0xffffa64d, 0xfff3dc5a, 0xff9fe06e,
                                      0xff52d1c2, 0xff6fb5f0, 0xffb99af2, 0xfff58bbd };
    return juce::Colour(c[juce::jlimit(1, 8, n) - 1]);
}

juce::Colour onClip() { return juce::Colour(0xff141414); }

void drawActive(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour(selsoft());
    g.fillRoundedRectangle(r, radius);
    g.setColour(acc());
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
}

namespace shadow {

// Groove Panel: nothing docked casts a shadow. Kept as no-ops so call sites compile.
void control(juce::Graphics&, juce::Rectangle<float>, float) {}
void card(juce::Graphics&, juce::Rectangle<float>, float) {}

void popover(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    // Levels: no drop shadows. A popover separates by its 18 % border and surface fill.
    juce::ignoreUnused(g, r, radius);
    g.setColour(edge());
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
}

void thumb(juce::Graphics& g, juce::Rectangle<float> thumb)
{
    // Levels handle: 12 px surface disc with a 2 px violet ring (mockup).
    const float d = 12.0f; juce::ignoreUnused(thumb.getWidth());
    const auto c = juce::Rectangle<float>(d, d).withCentre(thumb.getCentre());
    // Levels (LvRow): 12 px surface disc with a 2 px primary ring.
    g.setColour(surface200());
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
