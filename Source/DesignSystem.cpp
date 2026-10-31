#include "DesignSystem.h"

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
    // Exact §2 tokens — light / dark. Hex lives only in this file.
    static const Palette light {
        juce::Colour(0xffececee),   // bg
        juce::Colour(0xffe9e8ea),   // chrome
        juce::Colour(0xffe2e1e6),   // side
        juce::Colour(0xfff9f9fa),   // panel
        juce::Colour(0xfff2f2f4),   // rollchrome
        rgba(0, 0, 0, 0.13f),       // hl
        juce::Colour(0xff141416),   // tx
        juce::Colour(0xff48484e),   // tx2
        juce::Colour(0xff727278),   // tx3
        juce::Colour(0xff0068e0),   // acc
        rgba(0, 104, 224, 0.14f),   // selsoft
        juce::Colour(0xffffffff),   // ctl
        rgba(0, 0, 0, 0.20f),       // ctlb
        juce::Colour(0xffffffff),   // field
        rgba(0, 0, 0, 0.14f),       // trk
        rgba(0, 0, 0, 0.04f),       // rowalt
        juce::Colour(0xff1f7ef2),   // note
        juce::Colour(0xffe0a800),   // stron
        juce::Colour(0xffffffff),   // kw
        juce::Colour(0xff2e3035),   // kb
        rgba(0, 0, 0, 0.04f),       // laneb (derived from rowalt)
        rgba(0, 0, 0, 0.13f),       // beat (= hl)
        juce::Colour(0xff4a9df5),   // kindDrums
        juce::Colour(0xffa35ce8),   // kindBass
        juce::Colour(0xff18b8a5),   // kindKeys
        juce::Colour(0xfff0842c),   // kindPerc
        rgba(0, 0, 0, 0.18f),       // winbrd
        rgba(0, 0, 0, 0.05f),       // hoverWash
        juce::Colour(0xfff2f2f4),   // head
        rgba(0, 0, 0, 0.045f),      // well
        juce::Colour(0xffcfe2fb),   // kfInW
        juce::Colour(0xff1d5bb0),   // kfInB
        juce::Colour(0xffdcdce0),   // kfOutW
        juce::Colour(0xff8c8c93),   // kfOutB
        juce::Colour(0xffffffff),   // lensFill
        juce::Colour(0xff6fe3ff),   // logoNoteHi
    };
    static const Palette dark {
        juce::Colour(0xff28282b),
        juce::Colour(0xff333336),
        juce::Colour(0xff2b2b2f),
        juce::Colour(0xff1f1f22),
        juce::Colour(0xff232326),
        rgba(255, 255, 255, 0.085f),
        juce::Colour(0xfff2f2f5),
        juce::Colour(0xffb3b3ba),
        juce::Colour(0xff7d7d85),
        juce::Colour(0xff0a84ff),
        rgba(10, 132, 255, 0.22f),
        juce::Colour(0xff3b3b40),
        rgba(255, 255, 255, 0.10f),
        rgba(255, 255, 255, 0.06f),
        rgba(255, 255, 255, 0.16f),
        rgba(255, 255, 255, 0.035f),
        juce::Colour(0xff3f9bff),
        juce::Colour(0xfff5b400),
        juce::Colour(0xffd6d7dc),   // kw dark
        juce::Colour(0xff101114),   // kb dark
        rgba(255, 255, 255, 0.035f),
        rgba(255, 255, 255, 0.085f),
        juce::Colour(0xff4a9df5),
        juce::Colour(0xffa35ce8),
        juce::Colour(0xff18b8a5),
        juce::Colour(0xfff0842c),
        rgba(255, 255, 255, 0.12f),
        rgba(255, 255, 255, 0.05f),
        juce::Colour(0xff252528),           // head
        rgba(255, 255, 255, 0.055f),        // well
        rgba(10, 132, 255, 0.28f),          // kfInW = accent @ 28%
        rgba(10, 132, 255, 0.70f),          // kfInB = accent @ 70%
        rgba(255, 255, 255, 0.14f),         // kfOutW
        rgba(255, 255, 255, 0.28f),         // kfOutB
        juce::Colour(0xff2a2d33),           // lensFill
        juce::Colour(0xff6fe3ff),           // logoNoteHi
    };
    return usesDarkAppearance() ? dark : light;
}

namespace shadow {

void control(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    const float a = usesDarkAppearance() ? 0.30f : 0.14f;
    g.setColour(juce::Colours::black.withAlpha(a));
    g.drawRoundedRectangle(r.translated(0.0f, 0.5f).reduced(0.25f), radius, 1.0f);
}

void card(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour(juce::Colours::black.withAlpha(0.05f));
    g.drawRoundedRectangle(r.translated(0.0f, 1.0f).reduced(0.5f), radius, 1.0f);
}

void popover(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    const bool d = usesDarkAppearance();
    g.setColour(juce::Colours::black.withAlpha(d ? 0.50f : 0.24f));
    g.fillRoundedRectangle(r.expanded(2.0f).translated(0.0f, 4.0f), radius + 2.0f);
    g.setColour(juce::Colours::black.withAlpha(d ? 0.80f : 0.16f));
    g.drawRoundedRectangle(r.reduced(0.25f), radius, 0.5f);
}

void thumb(juce::Graphics& g, juce::Rectangle<float> thumb)
{
    g.setColour(juce::Colours::white);
    g.fillEllipse(thumb);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawEllipse(thumb.translated(0.0f, 1.0f).reduced(0.5f), 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.20f));
    g.drawEllipse(thumb.reduced(0.25f), 0.5f);
}

} // namespace shadow

juce::Font font(Type t)
{
    switch (t)
    {
        case Type::AppName:        return uiFontFixed(13.0f, true);
        case Type::PanelHeading:   return uiFontFixed(12.5f, true);
        case Type::SectionLabel:   return uiFontFixed(10.0f, true);
        case Type::TableHeader:    return uiFontFixed(10.5f, true);
        case Type::Body:           return uiFontFixed(12.5f, false);
        case Type::Metadata:       return monoFontFixed(11.0f, false);
        case Type::Caption:        return uiFontFixed(10.0f, false);
        case Type::NumericDisplay: return monoFontFixed(13.0f, true);
        case Type::Logo:           return uiFontFixed(14.0f, true).withExtraKerningFactor(-0.02f);
    }
    return uiFontFixed(12.5f, false);
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
