#include "Theme.h"
#include "BinaryData.h"
#include <unordered_map>
#include <cmath>

namespace pflow {

// ── Tokens ───────────────────────────────────────────────────────────────────

Tweaks& tweaks()
{
    static Tweaks t;
    return t;
}

namespace {

juce::Colour rgba(juce::uint8 r, juce::uint8 g, juce::uint8 b, float a)
{
    return juce::Colour(r, g, b).withAlpha(a);
}

} // namespace

bool usesDarkAppearance()
{
    return themeAt(themeIndex()).dark;
}

static juce::Colour applyContrast(juce::Colour c)
{
    // Contrast < 100 %: compress neutrals (surfaces, lines, text) toward a mid grey so
    // whites soften and blacks lift. Saturated accents (violet, green, red, amber) keep
    // their colour so state stays readable.
    const int pct = juce::jlimit(40, 100, tweaks().contrastPct.load());
    if (pct >= 100 || c.getFloatAlpha() <= 0.001f || c.getSaturation() > 0.35f)
        return c;
    const float amount = (float) (100 - pct) / 100.0f * 0.55f; // 40 % → 0.33
    const auto mid = juce::Colour(0xff8c8d92).withAlpha(c.getFloatAlpha());
    return c.interpolatedWith(mid, amount).withAlpha(c.getFloatAlpha());
}

juce::Colour tintForTheme(juce::Colour c)
{
    const auto& spec = themeAt(themeIndex());
    if (c.getFloatAlpha() <= 0.001f)
        return c;
    if (spec.amount <= 0.001f && std::abs(spec.lift) <= 0.001f)
        return applyContrast(c);

    juce::Colour out = c;
    if (spec.amount > 0.001f)
    {
        const auto hueCol = juce::Colour::fromHSV(spec.hue,
                                                   spec.dark ? 0.42f : 0.20f,
                                                   c.getBrightness(),
                                                   c.getFloatAlpha());
        out = c.interpolatedWith(hueCol, spec.amount);
    }
    if (std::abs(spec.lift) > 0.001f)
        out = out.withBrightness(juce::jlimit(0.0f, 1.0f, out.getBrightness() + spec.lift));
    return applyContrast(out.withAlpha(c.getFloatAlpha()));
}

const ThemeTokens& themeTokens()
{
    // Built each call from ds::palette() so appearance flips live.
    // Static storage would stale across light/dark toggles.
    thread_local ThemeTokens cached {};
    const auto& p = ds::palette();
    cached = {
        p.bg, p.panel, p.rollchrome, p.ctl,
        p.hl, p.hl, p.hl.withAlpha(p.hl.getFloatAlpha() * 0.55f),
        p.tx, p.tx2, p.tx3,
        p.panel, p.laneb, p.hl.withAlpha(p.hl.getFloatAlpha() * 0.45f),
        p.note.darker(0.25f), p.hl,
        p.kw, p.kb,
        p.chrome, p.chrome, p.side, p.side,
        p.rowalt, p.bg, p.bg, p.winbrd,
    };
    return cached;
}

const AccentTokens& accentTokens()
{
    thread_local AccentTokens cached {};
    const auto& p = ds::palette();
    cached = { p.acc,
               usesDarkAppearance() ? p.bg : juce::Colours::white,
               p.selsoft, p.acc.withAlpha(0.40f), p.note };
    return cached;
}

const InspectorTokens& inspectorTokens()
{
    thread_local InspectorTokens cached {};
    const auto& p = ds::palette();
    const bool d = usesDarkAppearance();
    cached = {
        p.panel,           // panelBg
        p.acc,
        p.tx,              // headerText
        p.tx2,             // rowLabel
        p.tx3,             // valueText
        p.hl,              // divider
        p.trk,             // sliderTrack
        ds::ink(),         // sliderKnob (Groove handle = solid ink block)
        p.ctl,             // controlSurface
        ds::surface300(),  // controlSurfaceHi (hover raises one surface step)
        p.hl,              // controlSeparator
        p.trk,             // switchOffTrack
        p.tx3,             // chevron
        p.tx2,             // segmentText
        p.ctlb,            // controlHairline
        p.field,           // tallWell
        p.trk,             // tallFill
        ds::surface300(),  // tallWellHot
        d,
    };
    return cached;
}

void drawInspectorControlSurface(juce::Graphics& g, juce::Rectangle<float> r,
                                 bool over, bool down)
{
    const auto& t = inspectorTokens();
    auto fill = t.controlSurface;
    if (down) fill = fill.darker(t.dark ? 0.10f : 0.06f);
    else if (over) fill = t.controlSurfaceHi;
    g.setColour(fill);
    g.fillRoundedRectangle(r, metrics::controlRadius);
    g.setColour(t.controlHairline);
    g.drawRoundedRectangle(r.reduced(0.25f), metrics::controlRadius, 1.0f);
    ds::shadow::control(g, r, metrics::controlRadius);
}

void drawInspectorDivider(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(inspectorTokens().divider);
    g.fillRect(bounds.getX(), bounds.getBottom() - 1, bounds.getWidth(), 1);
}

// ── Fonts (Levels: Inter for the interface, JetBrains Mono for live values) ──

namespace {

struct GrooveFaces
{
    GrooveFaces()
    {
        auto load = [](const char* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor(data, (size_t) size);
        };
        regular  = load(BinaryData::InterRegular_ttf,         BinaryData::InterRegular_ttfSize);
        medium   = load(BinaryData::InterMedium_ttf,          BinaryData::InterMedium_ttfSize);
        semibold = load(BinaryData::InterSemiBold_ttf,        BinaryData::InterSemiBold_ttfSize);
        bold     = load(BinaryData::InterBold_ttf,            BinaryData::InterBold_ttfSize);
        monoReg  = load(BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
        monoMed  = load(BinaryData::JetBrainsMonoMedium_ttf,  BinaryData::JetBrainsMonoMedium_ttfSize);
    }
    juce::Typeface::Ptr regular, medium, semibold, bold, monoReg, monoMed;
};

const GrooveFaces& grooveFaces()
{
    static const GrooveFaces faces;
    return faces;
}

} // namespace

juce::Font grooveFont(float height, int weight, bool mono)
{
    const auto& f = grooveFaces();
    juce::Typeface::Ptr face;
    if (mono)
        face = weight >= 500 ? f.monoMed : f.monoReg;
    else if (weight >= 700) face = f.bold;
    else if (weight >= 600) face = f.semibold;
    else if (weight >= 500) face = f.medium;
    else                    face = f.regular;

    if (face == nullptr)
        return juce::Font(juce::FontOptions(height));
    return juce::Font(juce::FontOptions().withTypeface(face).withHeight(height));
}

static juce::Font makeUiFont(float pt, bool semibold)
{
    const int weight = semibold ? 600 : (pt < 12.0f ? 500 : 400);
    const auto family = (FontFamily) juce::jlimit(0, (int) FontFamily::Count - 1,
                                                  tweaks().fontFamily.load());
    if (family == FontFamily::Archivo)
        return grooveFont(pt, weight, false);

    juce::String name = "Helvetica Neue";
    if (family == FontFamily::System)
        name = juce::Font::getDefaultSansSerifFontName();
    else if (family == FontFamily::Avenir)
        name = "Avenir Next";
    const int flags = semibold ? juce::Font::bold : juce::Font::plain;
    return juce::Font(juce::FontOptions(name, pt, flags));
}

juce::Colour contrastInkOn(juce::Colour surface)
{
    // Relative luminance shortcut: prefer dark ink on light, light ink on dark.
    const float L = surface.getBrightness();
    if (L > 0.55f)
        return juce::Colour(0xff111111);
    if (L > 0.35f)
        return juce::Colour(0xff1a1a1a);
    return juce::Colour(0xfff2f2f2);
}

juce::Colour contrastLineOn(juce::Colour surface)
{
    const float L = surface.getBrightness();
    if (L > 0.55f)
        return juce::Colour(0xff2a2a2a).withAlpha(0.55f);
    return juce::Colour(0xffffffff).withAlpha(0.45f);
}

SurfaceStyle currentSurfaceStyle()
{
    return (SurfaceStyle) juce::jlimit(0, (int) SurfaceStyle::Count - 1, tweaks().surfaceStyle.load());
}

namespace {
juce::Component::SafePointer<juce::Component> gTipHighlight;
}

void setTooltipHighlightTarget(juce::Component* c)
{
    if (auto* prev = gTipHighlight.getComponent())
        if (prev != c)
            prev->repaint();
    gTipHighlight = c;
    if (c != nullptr)
        c->repaint();
}

bool hasTooltipHighlight(const juce::Component* c)
{
    return c != nullptr && gTipHighlight.getComponent() == c;
}

juce::Font uiFont(float pt, bool semibold)
{
    // UI size scales layout + type; Text size % further scales type only.
    return makeUiFont(pt * contentScale() * textIconScale(), semibold);
}

juce::Font monoFont(float pt, bool semibold)
{
    // Every live value is a readout: IBM Plex Mono so digits never shift.
    juce::ignoreUnused(semibold);
    return grooveFont(pt * contentScale() * textIconScale(), 500, true);
}

juce::Font uiFontFixed(float pt, bool semibold)
{
    // Caps B2 base sizes: respect Text size %, not UI size (layout stays put).
    return makeUiFont(pt * textIconScale(), semibold);
}

juce::Font monoFontFixed(float pt, bool semibold)
{
    juce::ignoreUnused(semibold);
    return grooveFont(pt * textIconScale(), 500, true);
}

juce::Font fontFor(TextStyle s)
{
    // Map legacy TextStyle → Design System §3 styles (no in-between sizes).
    switch (s)
    {
        case TextStyle::LargeTitle:  return ds::font(ds::Type::NumericDisplay);
        case TextStyle::Title2:      return ds::font(ds::Type::PanelHeading);
        case TextStyle::Headline:    return ds::font(ds::Type::PanelHeading);
        case TextStyle::Body:        return ds::font(ds::Type::Body);
        case TextStyle::Callout:     return ds::font(ds::Type::AppName);
        case TextStyle::Subheadline: return ds::font(ds::Type::Body);
        case TextStyle::Footnote:    return ds::font(ds::Type::Metadata);
        case TextStyle::Caption:     return ds::font(ds::Type::Caption);
    }
    return ds::font(ds::Type::Body);
}

namespace {

class HIGAnimator : private juce::Timer
{
public:
    static HIGAnimator& instance()
    {
        static HIGAnimator a;
        return a;
    }

    void watch(juce::Component& c)
    {
        if (watching.contains(&c)) return;
        watching.add(&c);
        startTimerHz(60);
    }

private:
    void timerCallback() override
    {
        for (int i = watching.size(); --i >= 0;)
        {
            auto* c = watching.getUnchecked(i);
            if (c == nullptr || !c->isShowing()) { watching.remove(i); continue; }
            c->repaint();
        }
        if (watching.isEmpty()) stopTimer();
    }
    juce::Array<juce::Component*> watching;
};

static const juce::Identifier kHigAlpha("hig_alpha");
static const juce::Identifier kHigLastMs("hig_last_ms");

float animatedAlpha(juce::Component& c, float target, float tauMs)
{
    auto& props = c.getProperties();
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double last = props.contains(kHigLastMs) ? (double) props[kHigLastMs] : now;
    const float cur = props.contains(kHigAlpha) ? (float) props[kHigAlpha] : 0.0f;
    const float dt = (float) juce::jlimit(0.0, 100.0, now - last);
    props.set(kHigLastMs, now);
    const float k = 1.0f - std::exp(-dt / juce::jmax(1.0f, tauMs));
    float next = cur + (target - cur) * k;
    if (std::abs(next - target) < 0.002f) next = target;
    props.set(kHigAlpha, next);
    if (next != target) HIGAnimator::instance().watch(c);
    return next;
}

struct KeyAnimState { float v = 0.0f; double lastMs = 0.0; };
float animatedAlphaForKey(juce::int64 key, float target, float tauMs)
{
    static std::unordered_map<juce::int64, KeyAnimState> states;
    const double now = juce::Time::getMillisecondCounterHiRes();
    auto& st = states[key];
    if (st.lastMs <= 0.0) st.lastMs = now;
    const float dt = (float) juce::jlimit(0.0, 100.0, now - st.lastMs);
    st.lastMs = now;
    const float k = 1.0f - std::exp(-dt / juce::jmax(1.0f, tauMs));
    st.v = st.v + (target - st.v) * k;
    if (std::abs(st.v - target) < 0.002f) st.v = target;
    return st.v;
}

float higPressAlpha(bool highlighted, bool down, bool focused)
{
    if (down) return 0.16f;
    if (focused) return 0.10f;
    if (highlighted) return 0.06f;
    return 0.0f;
}

} // namespace

void PatternFlowLookAndFeel::drawSymbol(juce::Graphics& g, const juce::String& name,
                                        juce::Rectangle<float> b, juce::Colour colour, float stroke)
{
    const float cx = b.getCentreX();
    const float cy = b.getCentreY();
    const float s = juce::jmin(b.getWidth(), b.getHeight()) * 0.38f;
    g.setColour(colour);
    juce::PathStrokeType st(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    if (name == higIcon::star)
    {
        juce::Path p;
        for (int i = 0; i < 5; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 5.0f - juce::MathConstants<float>::halfPi;
            const float r = (i % 2 == 0) ? s : s * 0.42f;
            const float x = cx + r * std::cos(a);
            const float y = cy + r * std::sin(a);
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        p.closeSubPath();
        g.strokePath(p, st);
        return;
    }

    if (name == higIcon::chevronUp)
    {
        juce::Path p;
        p.startNewSubPath(cx - s * 0.55f, cy + s * 0.3f);
        p.lineTo(cx, cy - s * 0.45f);
        p.lineTo(cx + s * 0.55f, cy + s * 0.3f);
        g.strokePath(p, st);
        return;
    }

    if (name == higIcon::chevronDown)
    {
        juce::Path p;
        p.startNewSubPath(cx - s * 0.55f, cy - s * 0.3f);
        p.lineTo(cx, cy + s * 0.45f);
        p.lineTo(cx + s * 0.55f, cy - s * 0.3f);
        g.strokePath(p, st);
        return;
    }

    if (name == higIcon::scissors)
    {
        g.drawLine(cx - s, cy - s * 0.5f, cx + s, cy + s * 0.5f, stroke);
        g.drawLine(cx - s, cy + s * 0.5f, cx + s, cy - s * 0.5f, stroke);
        g.drawEllipse(cx - s * 0.85f, cy - s * 0.95f, s * 0.55f, s * 0.55f, stroke);
        g.drawEllipse(cx + s * 0.30f, cy + s * 0.40f, s * 0.55f, s * 0.55f, stroke);
        return;
    }

    if (name == higIcon::speakerSlash)
    {
        juce::Path spk;
        spk.addTriangle(cx - s * 0.55f, cy - s * 0.35f, cx - s * 0.55f, cy + s * 0.35f, cx - s * 0.05f, cy);
        g.strokePath(spk, st);
        g.drawLine(cx - s * 0.02f, cy - s * 0.45f, cx + s * 0.75f, cy - s * 0.95f, stroke * 0.9f);
        g.drawLine(cx - s * 0.02f, cy + s * 0.45f, cx + s * 0.75f, cy + s * 0.95f, stroke * 0.9f);
        g.drawLine(cx - s * 0.3f, cy + s * 0.95f, cx + s * 0.95f, cy - s * 0.75f, stroke);
    }
}

PatternFlowLookAndFeel::PatternFlowLookAndFeel()
{
    refreshColours();
}

void PatternFlowLookAndFeel::refreshColours()
{
    setColour(juce::ResizableWindow::backgroundColourId, colours::bg());
    setColour(juce::TextEditor::backgroundColourId, colours::bgLight());
    setColour(juce::TextEditor::textColourId, colours::text());
    setColour(juce::TextEditor::outlineColourId, colours::separator());
    setColour(juce::ComboBox::backgroundColourId, colours::bgLight());
    setColour(juce::ComboBox::textColourId, colours::text());
    setColour(juce::ComboBox::outlineColourId, colours::separator());
    setColour(juce::ComboBox::arrowColourId, colours::textDim());
    setColour(juce::PopupMenu::backgroundColourId, colours::bgLight());
    setColour(juce::PopupMenu::textColourId, colours::text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accentSoft());
    setColour(juce::PopupMenu::highlightedTextColourId, colours::accent());
    setColour(juce::ScrollBar::thumbColourId, colours::lineStrong().withAlpha(0.55f));
    setColour(juce::ListBox::backgroundColourId, colours::panel());
    setColour(juce::ListBox::textColourId, colours::text());
    setColour(juce::Label::textColourId, colours::text());
    setColour(juce::ToggleButton::textColourId, colours::text());
    setColour(juce::ToggleButton::tickColourId, colours::accent());
    setColour(juce::TextButton::buttonColourId, colours::elev());
    setColour(juce::TextButton::textColourOffId, colours::text());
    setColour(juce::TextButton::textColourOnId, colours::accentInk());

    setColour(juce::TreeView::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TreeView::linesColourId, colours::separator().withAlpha(0.4f));
    setColour(juce::TreeView::selectedItemBackgroundColourId, colours::selection());
    setColour(juce::TreeView::dragAndDropIndicatorColourId, colours::accent());

    setColour(juce::DirectoryContentsDisplayComponent::textColourId, colours::text());
    setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colours::selection());
    setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, colours::text());

    setColour(juce::TooltipWindow::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TooltipWindow::textColourId, juce::Colour(0xff1a1200));
    setColour(juce::TooltipWindow::outlineColourId, juce::Colours::transparentBlack);
}

void PatternFlowLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                               int w, int h, float sliderPos,
                                               float startAngle, float endAngle, juce::Slider&)
{
    auto radius = (float) juce::jmin(w, h) * 0.38f;
    auto centreX = (float) x + (float) w * 0.5f;
    auto centreY = (float) y + (float) h * 0.5f;
    auto angle = startAngle + sliderPos * (endAngle - startAngle);
    auto bodyRadius = radius - 2.0f;
    g.setColour(colours::controlFill());
    g.fillEllipse(centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    juce::Path track;
    track.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour(colours::knobTrack());
    g.strokePath(track, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (sliderPos > 0.0f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour(colours::accent());
        g.strokePath(valueArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void PatternFlowLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                                    const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = btn.getLocalBounds().toFloat().reduced(0.5f);
    const juce::String id = btn.getComponentID();
    const bool focused = btn.hasKeyboardFocus(true);
    const bool toggled = btn.getToggleState();
    const float pressA = higPressAlpha(highlighted, down, focused);

    if (id == "HIGPrimaryButton")
    {
        juce::Colour fill = colours::accent();
        if (down) fill = fill.darker(0.12f);
        else if (highlighted) fill = fill.brighter(0.06f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, metrics::chipRadius);
        if (pressA > 0.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(pressA));
            g.fillRoundedRectangle(bounds, metrics::chipRadius);
        }
        return;
    }

    if (id == "HIGIconButton")
    {
        if (pressA > 0.0f)
        {
            g.setColour(colours::controlFill());
            g.fillRoundedRectangle(bounds, metrics::chipRadius);
        }
        return;
    }

    if (id == "HIGChipToggle")
    {
        const bool active = toggled && btn.getClickingTogglesState();
        juce::Colour fill = active ? colours::accent() : colours::controlFill();
        if (down) fill = fill.darker(active ? 0.1f : 0.05f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
        return;
    }

    if (pressA > 0.0f)
    {
        g.setColour(colours::controlFill().withAlpha(0.5f + pressA));
        g.fillRoundedRectangle(bounds, metrics::chipRadius);
    }
}

void PatternFlowLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(getLabelFont(label));
    auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
    g.drawFittedText(label.getText(), textArea, label.getJustificationType(), 1, 1.0f);
}

juce::Font PatternFlowLookAndFeel::getLabelFont(juce::Label&) { return fontFor(TextStyle::Body); }

juce::Font PatternFlowLookAndFeel::getTextButtonFont(juce::TextButton& btn, int)
{
    const auto id = btn.getComponentID();
    if (id == "HIGPrimaryButton") return fontFor(TextStyle::Body);
    if (id == "HIGChipToggle") return fontFor(TextStyle::Callout);
    return fontFor(TextStyle::Callout);
}

juce::Font PatternFlowLookAndFeel::getComboBoxFont(juce::ComboBox&) { return fontFor(TextStyle::Body); }

void PatternFlowLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setMinimumHorizontalScale(1.0f);
    auto b = box.getLocalBounds();
    label.setBounds(b.withTrimmedLeft(metrics::comboTextPadding).withTrimmedRight(18));
}

juce::Font PatternFlowLookAndFeel::getPopupMenuFont() { return fontFor(TextStyle::Body); }

void PatternFlowLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& btn, bool, bool)
{
    const auto id = btn.getComponentID();

    if (id == "HIGIconButton")
    {
        const auto icon = btn.getProperties()["hig_icon"].toString();
        drawSymbol(g, icon, btn.getLocalBounds().toFloat(), colours::text(), 1.45f);
        return;
    }

    if (id == "HIGChipToggle" && btn.getButtonText().isEmpty())
    {
        const bool muted = btn.getToggleState();
        drawSymbol(g, higIcon::speakerSlash, btn.getLocalBounds().toFloat(),
                   muted ? colours::onPrimary() : colours::text(), 1.35f);
        return;
    }

    const bool active = btn.getToggleState() && btn.getClickingTogglesState();
    juce::Colour col = colours::text();
    if (id == "HIGPrimaryButton") col = colours::onPrimary();
    else if (id == "HIGChipToggle" && active) col = colours::onPrimary();
    g.setColour(col);
    g.setFont(getTextButtonFont(btn, 0));

    if (id == "HIGChipToggle" && btn.getButtonText() == "Trim")
    {
        auto r = btn.getLocalBounds().reduced(8, 2);
        auto iconArea = r.removeFromLeft(18);
        drawSymbol(g, higIcon::scissors, iconArea.toFloat(),
                   active ? colours::onPrimary() : colours::text(), 1.25f);
        g.drawFittedText(btn.getButtonText(), r, juce::Justification::centredLeft, 1, 1.0f);
        return;
    }

    g.drawFittedText(btn.getButtonText(), btn.getLocalBounds().reduced(8, 2),
                     juce::Justification::centred, 1, 1.0f);
}

void PatternFlowLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                              bool highlighted, bool down)
{
    juce::ignoreUnused(highlighted, down);
    const bool on = button.getToggleState();
    auto b = button.getLocalBounds().toFloat().reduced(1.0f);
    const float h = b.getHeight();
    const float w = h * 1.75f;
    auto track = juce::Rectangle<float>(b.getX(), b.getY() + (b.getHeight() - h * 0.72f) * 0.5f, w, h * 0.72f);
    g.setColour(on ? colours::accent() : colours::controlFill());
    g.fillRoundedRectangle(track, track.getHeight() * 0.5f);
    const float knob = track.getHeight() - 4.0f;
    const float kx = on ? track.getRight() - knob - 2.0f : track.getX() + 2.0f;
    g.setColour(juce::Colours::white);
    g.fillEllipse(kx, track.getCentreY() - knob * 0.5f, knob, knob);
    g.setColour(button.findColour(juce::ToggleButton::textColourId));
    g.setFont(fontFor(TextStyle::Body));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft((int) w + 8),
                     juce::Justification::centredLeft, 1, 1.0f);
}

void PatternFlowLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                         bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);
    const bool focused = box.hasKeyboardFocus(true);
    g.setColour(colours::bgLight());
    g.fillRoundedRectangle(bounds, metrics::cornerRadius);
    g.setColour(focused ? colours::accent() : colours::separator());
    g.drawRoundedRectangle(bounds, metrics::cornerRadius, focused ? 1.5f : 0.5f);
}

void PatternFlowLookAndFeel::drawFileBrowserRow(juce::Graphics& g, int width, int height,
                                               const juce::File&, const juce::String& filename,
                                               juce::Image* icon, const juce::String&,
                                               const juce::String&, bool,
                                               bool isItemSelected, int, juce::DirectoryContentsDisplayComponent& dcc)
{
    auto* fileListComp = dynamic_cast<juce::Component*>(&dcc);
    if (isItemSelected)
    {
        g.setColour(fileListComp != nullptr
            ? fileListComp->findColour(juce::DirectoryContentsDisplayComponent::highlightColourId)
            : findColour(juce::DirectoryContentsDisplayComponent::highlightColourId));
        g.fillRect(0, 1, width, height - 2);
    }

    const int x = 28;
    if (icon != nullptr && icon->isValid())
        g.drawImageWithin(*icon, 4, 2, x - 6, height - 4,
                          juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, false);

    g.setColour(colours::text());
    g.setFont(systemFont(metrics::browserFontSize, isItemSelected));
    g.drawText(filename, x, 0, width - x - 4, height, juce::Justification::centredLeft, true);
}

void PatternFlowLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                                      juce::TextEditor& ed)
{
    // Transparent editors (search query) — no fill; parent owns the chrome.
    if (ed.findColour(juce::TextEditor::backgroundColourId).getAlpha() < 8)
        return;
    LookAndFeel_V4::fillTextEditorBackground(g, width, height, ed);
}

void PatternFlowLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    // Transparent-background editors (search query): parent paints the container
    // focus ring — never outline the bare text field.
    if (ed.findColour(juce::TextEditor::backgroundColourId).getAlpha() < 8)
        return;

    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);
    const bool focused = ed.hasKeyboardFocus(true);
    g.setColour(focused ? colours::accent() : colours::separator());
    g.drawRoundedRectangle(bounds, metrics::cornerRadius, focused ? 1.5f : 0.5f);
}

void PatternFlowLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(1.0f);
    g.setColour(colours::panel());
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(colours::line());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 0.5f);
}

void PatternFlowLookAndFeel::preparePopupMenuWindow(juce::Component& window)
{
    window.setOpaque(false);
}

int PatternFlowLookAndFeel::getPopupMenuBorderSize()
{
    return 6;
}

void PatternFlowLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                                       int standardMenuItemHeight,
                                                       int& idealWidth, int& idealHeight)
{
    LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight,
                                              idealWidth, idealHeight);
    if (!isSeparator)
        idealHeight = 26;
    idealWidth = juce::jmax(idealWidth,
                            (int) std::ceil(juce::GlyphArrangement::getStringWidth(uiFont(12.5f, false), text)) + 36);
}

void PatternFlowLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu, const juce::String& text,
                                              const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                              const juce::Colour* textColourToUse)
{
    juce::ignoreUnused(icon, shortcutKeyText);

    if (isSeparator)
    {
        g.setColour(colours::separator());
        g.fillRect(area.reduced(10, 0).withHeight(1).withY(area.getCentreY()));
        return;
    }

    auto r = area.reduced(6, 1).toFloat();
    if (isHighlighted && isActive)
    {
        g.setColour(colours::accent());
        g.fillRoundedRectangle(r, metrics::chipRadius);
    }
    else if (isTicked)
    {
        g.setColour(colours::accent().withAlpha(0.14f));
        g.fillRoundedRectangle(r, metrics::chipRadius);
    }

    auto textArea = area.reduced(12, 0);
    if (isTicked || hasSubMenu)
        textArea.removeFromLeft(16);

    if (isTicked)
    {
        // ASCII-safe checkmark drawn as geometry (avoids missing-glyph boxes).
        juce::Path tick;
        const float x = (float) area.getX() + 12.0f;
        const float y = (float) area.getCentreY();
        tick.startNewSubPath(x - 3.5f, y);
        tick.lineTo(x - 0.5f, y + 3.0f);
        tick.lineTo(x + 4.5f, y - 3.5f);
        g.setColour(isHighlighted ? colours::accentInk() : colours::accent());
        g.strokePath(tick, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }

    if (hasSubMenu)
    {
        auto chev = juce::Rectangle<float>((float) area.getRight() - 16.0f,
                                           (float) area.getCentreY() - 4.0f, 8.0f, 8.0f);
        juce::Path p;
        p.addTriangle(chev.getX(), chev.getY(),
                      chev.getX(), chev.getBottom(),
                      chev.getRight(), chev.getCentreY());
        g.setColour(isHighlighted ? colours::accentInk() : colours::text3());
        g.fillPath(p);
    }

    juce::Colour col = (textColourToUse != nullptr) ? *textColourToUse : colours::text();
    if (!isActive)
        col = colours::textDim();
    else if (isHighlighted)
        col = colours::accentInk();

    g.setFont(uiFont(12.5f, false));
    g.setColour(col);
    // Draw with fitted text so missing glyphs never leave empty slots.
    g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);
}

void PatternFlowLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y,
                                          int width, int height, bool isScrollbarVertical,
                                          int thumbStartPosition, int thumbSize,
                                          bool isMouseOver, bool isMouseDown)
{
    // Groove scrollbar: a square thumb on a hairline track, not a floating pill.
    auto track = juce::Rectangle<int>(x, y, width, height).toFloat();
    g.setColour(ds::surface100());
    g.fillRect(track);
    g.setColour(ds::line());
    if (isScrollbarVertical)
        g.fillRect(track.getX(), track.getY(), 1.0f, track.getHeight());
    else
        g.fillRect(track.getX(), track.getY(), track.getWidth(), 1.0f);

    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = { track.getX() + 2.0f, track.getY() + (float) thumbStartPosition, track.getWidth() - 3.0f, (float) thumbSize };
    else
        thumb = { track.getX() + (float) thumbStartPosition, track.getY() + 2.0f, (float) thumbSize, track.getHeight() - 3.0f };
    if (thumb.isEmpty())
        return;

    g.setColour(isMouseDown ? ds::acc() : ds::surface300());
    g.fillRect(thumb);
    g.setColour(isMouseDown || isMouseOver ? ds::acc() : ds::edge());
    g.drawRect(thumb, 1.0f);
}

void PatternFlowLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text,
                                         int width, int height)
{
    // Only the bubble is filled — the window behind it stays clear (no white corners).
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(1.0f);
    const float radius = 6.0f;
    g.setColour(tooltipAccent());
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(juce::Colour(0xff5a3a00).withAlpha(0.35f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);

    g.setColour(juce::Colour(0xff1a1200));
    g.setFont(uiFont(11.5f, false));
    g.drawFittedText(text, bounds.reduced(9.0f, 6.0f).toNearestInt(),
                     juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> PatternFlowLookAndFeel::getTooltipBounds(const juce::String& tipText,
                                                              juce::Point<int> screenPos,
                                                              juce::Rectangle<int> parentArea)
{
    const auto font = uiFont(11.5f, false);
    const int maxW = 260;
    int textW = 0;
    int lines = 1;
    {
        juce::AttributedString as;
        as.setJustification(juce::Justification::centredLeft);
        as.append(tipText, font, juce::Colour(0xff1a1200));
        juce::TextLayout layout;
        layout.createLayout(as, (float) maxW);
        textW = (int) std::ceil(layout.getWidth());
        lines = juce::jmax(1, (int) std::ceil(layout.getHeight() / font.getHeight()));
    }
    const int padX = 18;
    const int padY = 12;
    const int w = juce::jlimit(40, maxW + padX, textW + padX);
    const int h = (int) std::ceil(font.getHeight() * (float) lines) + padY;

    int x = screenPos.x + 8;
    int y = screenPos.y + 22;
    if (x + w > parentArea.getRight())
        x = parentArea.getRight() - w - 4;
    if (y + h > parentArea.getBottom())
        y = screenPos.y - h - 10;
    x = juce::jmax(parentArea.getX() + 4, x);
    y = juce::jmax(parentArea.getY() + 4, y);
    return { x, y, w, h };
}

} // namespace pflow
