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
    // Groove Panel tokens (Studio Dark / Daylight), mapped onto the legacy slots.
    // Hex lives only in this file.
    static const Palette light {
        juce::Colour(0xffa3a3a3),   // bg        = surface-000
        juce::Colour(0xffc9c9c9),   // chrome    = surface-100
        juce::Colour(0xffc9c9c9),   // side      = surface-100
        juce::Colour(0xffc9c9c9),   // panel     = surface-100
        juce::Colour(0xffc9c9c9),   // rollchrome= surface-100
        juce::Colour(0xff7d7d7d),   // hl        = line
        juce::Colour(0xff111111),   // tx        = ink
        juce::Colour(0xff3e3e3e),   // tx2       = ink-muted
        juce::Colour(0xff3e3e3e),   // tx3       = ink-muted (readable labels)
        juce::Colour(0xffb94e00),   // acc       = accent
        rgba(185, 78, 0, 0.22f),    // selsoft
        juce::Colour(0xffd8d8d8),   // ctl       = surface-200
        juce::Colour(0xff6e6e6e),   // ctlb      = edge
        juce::Colour(0xffebebeb),   // field     = surface-300
        juce::Colour(0xff5e5e5e),   // trk       = ink-faint
        rgba(0, 0, 0, 0.0f),        // rowalt    (hairlines, no zebra)
        juce::Colour(0xff6fb5f0),   // note      = clip-6
        juce::Colour(0xffb94e00),   // stron     = accent
        juce::Colour(0xfff4f4f4),   // kw (white keys)
        juce::Colour(0xff3e3e3e),   // kb (black keys)
        rgba(0, 0, 0, 0.10f),       // laneb
        rgba(0, 0, 0, 0.16f),       // beat
        juce::Colour(0xffff7a6b),   // kindDrums = clip-1
        juce::Colour(0xff6fb5f0),   // kindBass  = clip-6
        juce::Colour(0xffb99af2),   // kindKeys  = clip-7
        juce::Colour(0xffffa64d),   // kindPerc  = clip-2
        juce::Colour(0xff7d7d7d),   // winbrd    = line
        rgba(0, 0, 0, 0.06f),       // hoverWash
        juce::Colour(0xffebebeb),   // head      = surface-300
        juce::Colour(0xffd8d8d8),   // well      = surface-200
        rgba(185, 78, 0, 0.28f),    // kfInW
        rgba(185, 78, 0, 0.70f),    // kfInB
        juce::Colour(0xffd8d8d8),   // kfOutW
        juce::Colour(0xff7d7d7d),   // kfOutB
        juce::Colour(0xffd8d8d8),   // lensFill
        juce::Colour(0xffb94e00),   // logoNoteHi
    };
    static const Palette dark {
        juce::Colour(0xff141414),   // bg        = surface-000
        juce::Colour(0xff232323),   // chrome    = surface-100
        juce::Colour(0xff232323),   // side      = surface-100
        juce::Colour(0xff232323),   // panel     = surface-100
        juce::Colour(0xff232323),   // rollchrome= surface-100
        juce::Colour(0xff0a0a0a),   // hl        = line
        juce::Colour(0xffececec),   // tx        = ink
        juce::Colour(0xffa8a8a8),   // tx2       = ink-muted
        juce::Colour(0xffa8a8a8),   // tx3       = ink-muted (readable labels)
        juce::Colour(0xffff8b3d),   // acc       = accent
        rgba(255, 139, 61, 0.24f),  // selsoft
        juce::Colour(0xff2e2e2e),   // ctl       = surface-200
        juce::Colour(0xff808080),   // ctlb      = edge
        juce::Colour(0xff3a3a3a),   // field     = surface-300
        juce::Colour(0xff7a7a7a),   // trk       = ink-faint
        rgba(0, 0, 0, 0.0f),        // rowalt
        juce::Colour(0xff6fb5f0),   // note      = clip-6
        juce::Colour(0xffff8b3d),   // stron     = accent
        juce::Colour(0xffa8a8a8),   // kw        = ink-muted
        juce::Colour(0xff141414),   // kb        = surface-000
        rgba(0, 0, 0, 0.22f),       // laneb
        rgba(0, 0, 0, 0.28f),       // beat
        juce::Colour(0xffff7a6b),   // kindDrums = clip-1
        juce::Colour(0xff6fb5f0),   // kindBass  = clip-6
        juce::Colour(0xffb99af2),   // kindKeys  = clip-7
        juce::Colour(0xffffa64d),   // kindPerc  = clip-2
        juce::Colour(0xff0a0a0a),   // winbrd    = line
        rgba(255, 255, 255, 0.05f), // hoverWash
        juce::Colour(0xff3a3a3a),   // head      = surface-300
        juce::Colour(0xff2e2e2e),   // well      = surface-200
        rgba(255, 139, 61, 0.28f),  // kfInW
        rgba(255, 139, 61, 0.70f),  // kfInB
        juce::Colour(0xff3a3a3a),   // kfOutW
        juce::Colour(0xff7a7a7a),   // kfOutB
        juce::Colour(0xff2e2e2e),   // lensFill
        juce::Colour(0xffff8b3d),   // logoNoteHi
    };
    return usesDarkAppearance() ? dark : light;
}

// ── Groove Panel tokens not covered by the legacy slots ─────────────────────

#define GP_TOKEN(fn, darkHex, lightHex) \
    juce::Colour fn() { return juce::Colour(usesDarkAppearance() ? (juce::uint32) darkHex : (juce::uint32) lightHex); }

GP_TOKEN(surface000, 0xff141414, 0xffa3a3a3)
GP_TOKEN(surface100, 0xff232323, 0xffc9c9c9)
GP_TOKEN(surface200, 0xff2e2e2e, 0xffd8d8d8)
GP_TOKEN(surface300, 0xff3a3a3a, 0xffebebeb)
GP_TOKEN(line,       0xff0a0a0a, 0xff7d7d7d)
GP_TOKEN(edge,       0xff808080, 0xff6e6e6e)
GP_TOKEN(ink,        0xffececec, 0xff111111)
GP_TOKEN(inkMuted,   0xffa8a8a8, 0xff3e3e3e)
GP_TOKEN(inkFaint,   0xff7a7a7a, 0xff5e5e5e)
GP_TOKEN(onAccent,   0xff141414, 0xffffffff)
GP_TOKEN(play,       0xff7ad35a, 0xff2c7a1c)
GP_TOKEN(record,     0xffff5b52, 0xffc0241a)
GP_TOKEN(solo,       0xff5cb3ff, 0xff1d62b5)
GP_TOKEN(active,     0xfff2cd3a, 0xff7d6300)
GP_TOKEN(onLit,      0xff141414, 0xffffffff)
GP_TOKEN(focus,      0xffececec, 0xff111111)

#undef GP_TOKEN

juce::Colour clip(int n)
{
    static const juce::uint32 c[] = { 0xffff7a6b, 0xffffa64d, 0xfff3dc5a, 0xff9fe06e,
                                      0xff52d1c2, 0xff6fb5f0, 0xffb99af2, 0xfff58bbd };
    return juce::Colour(c[juce::jlimit(1, 8, n) - 1]);
}

juce::Colour onClip() { return juce::Colour(0xff141414); }

namespace shadow {

// Groove Panel: nothing docked casts a shadow. Kept as no-ops so call sites compile.
void control(juce::Graphics&, juce::Rectangle<float>, float) {}
void card(juce::Graphics&, juce::Rectangle<float>, float) {}

void popover(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    // shadow-popover: 0 4px 12px rgba(0,0,0,.6 dark / .25 light)
    const bool d = usesDarkAppearance();
    juce::DropShadow(juce::Colours::black.withAlpha(d ? 0.60f : 0.25f), 12, { 0, 4 })
        .drawForRectangle(g, r.getSmallestIntegerContainer());
    g.setColour(line());
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
}

void thumb(juce::Graphics& g, juce::Rectangle<float> thumb)
{
    // Groove handle: a solid ink block with radius-sm, no border needed in either theme.
    g.setColour(ink());
    g.fillRoundedRectangle(thumb, 2.0f);
}

} // namespace shadow

juce::Font font(Type t)
{
    // Groove Panel type scale (label 11 / body 12 / title 13 / readout 11 mono / readout-lg 15 mono).
    switch (t)
    {
        case Type::AppName:        return uiFontFixed(13.0f, true);
        case Type::PanelHeading:   return uiFontFixed(13.0f, true);   // title
        case Type::SectionLabel:   return uiFontFixed(11.0f, false);  // label
        case Type::TableHeader:    return uiFontFixed(11.0f, false);  // label
        case Type::Body:           return uiFontFixed(12.0f, false);  // body
        case Type::Metadata:       return monoFontFixed(11.0f);       // readout
        case Type::Caption:        return uiFontFixed(11.0f, false);  // label
        case Type::NumericDisplay: return monoFontFixed(15.0f);       // readout-lg
        case Type::Logo:           return grooveFont(14.0f * textIconScale(), 700).withExtraKerningFactor(-0.01f);
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
