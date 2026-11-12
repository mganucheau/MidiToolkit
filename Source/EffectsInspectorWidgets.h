#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "Theme.h"
#include "UiAtoms.h"
#include "GrooveEngine.h"
#include "EditModel.h"

namespace pflow {
namespace fx {

// Toolkit sidebar A — logical px, before metrics::scaled().
constexpr float kFontPt = 12.0f;
constexpr float kAnnotPt = 11.0f;
constexpr float kPopupFontPt = 12.0f;
constexpr float kSectionTitlePt = 12.0f;
constexpr float kPaneTitlePt = 13.0f;
constexpr int kSectionHeaderH = 36;   // Levels: flat 36 px section header
constexpr int kSectionPadH = 12;       // chevron starts here
constexpr int kSectionRowInset = 22;   // content indent past the chevron
constexpr int kContentPadX = 30;       // Levels: rows align under the section title
constexpr int kContentPadR = 12;
constexpr int kRowMinH = 28;      // Levels row height
constexpr int kControlH = 24;
constexpr int kSliderRowH = 28;
constexpr int kLabelColW = 72;    // Levels label column
constexpr int kValueColW = 36;
constexpr int kSelectW = 64;
constexpr int kSelectHalfW = 120;
constexpr int kHeaderIconW = 20; // match library sidebar glyph hit target
constexpr float kTallRadius = 4.0f;
constexpr float kTallPadX = 10.0f;

/** Compact / Comfortable spacing between Toolkit rows and section chrome. */
inline int toolkitDensity(int compact)
{
    const int v = currentDensity() == Density::Comfortable ? compact + 2 : compact;
    return metrics::scaled(v);
}
inline int toolkitRowGap()           { return toolkitDensity(4); }
inline int toolkitSectionPadT()      { return 0; }
inline int toolkitSectionPadB()      { return toolkitDensity(12); }
inline int toolkitSectionPadBFolded(){ return toolkitDensity(12); }
inline int toolkitTitleGap()         { return 0; }
inline int toolkitBodyPadB()         { return toolkitDensity(8); }

/** Solid accent behind type. Dark-mode blue is bright enough that black type
    fails and white type needs a deeper fill. */
inline juce::Colour accentFillColour()
{
    return ds::primary();   // Levels: lit = primary, text on it = on-primary
}

inline juce::Colour onAccentColour()
{
    return ds::onPrimary();
}

inline juce::Font inspectorFont(bool semibold = false) { return uiFontFixed(kFontPt, semibold); }
inline juce::Font inspectorMono(bool semibold = false) { return monoFontFixed(kFontPt, semibold); }
inline juce::Font popupFont(bool semibold = false) { return uiFontFixed(kPopupFontPt, semibold); }
inline juce::Font sectionTitleFont()
{
    // Levels section label: mono 12, caps, slight tracking.
    auto f = monoFontFixed(12.0f, true);
    f.setExtraKerningFactor(0.04f);
    return f;
}
inline juce::Font paneTitleFont() { return uiFontFixed(kPaneTitlePt, true); }

inline void fillTallWell(juce::Graphics& g, juce::Rectangle<float> r, bool hot,
                         const juce::Component* tipSource = nullptr)
{
    // Groove field: surface-300, edge border (accent while hot), radius-sm.
    // Tip highlight paints the same amber as the tooltip bubble.
    // Clip the fill to the rounded path so no white corner bleed shows through.
    const bool tipHot = tipSource != nullptr && hasTooltipHighlight(tipSource);
    juce::Path path;
    path.addRoundedRectangle(r, ds::radius::sm);
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(path);
        g.setColour(tipHot ? tooltipAccent() : ds::surface200());
        g.fillRect(r);
    }
    g.setColour(tipHot ? ds::warning().darker(0.6f) : (hot ? ds::primary() : ds::edge()));
    g.drawRoundedRectangle(r.reduced(0.5f), ds::radius::sm, hot ? 1.5f : 1.0f);
}

inline void drawCaretDown(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    juce::Path p;
    const float cx = area.getCentreX(), cy = area.getCentreY();
    const float s = 4.0f;
    p.addTriangle(cx - s, cy - s * 0.55f, cx + s, cy - s * 0.55f, cx, cy + s * 0.7f);
    g.setColour(colour);
    g.fillPath(p);
}

/** Stroked chevron. Open points down; folded is the same path rotated −90°. */
inline void drawStrokedChevron(juce::Graphics& g, juce::Rectangle<float> area,
                               juce::Colour colour, bool open)
{
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float s = 3.6f;
    juce::Path p;
    p.startNewSubPath(cx - s, cy - s * 0.45f);
    p.lineTo(cx, cy + s * 0.55f);
    p.lineTo(cx + s, cy - s * 0.45f);
    if (!open)
        p.applyTransform(juce::AffineTransform::rotation(
            -juce::MathConstants<float>::halfPi, cx, cy));
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
}

inline void drawCaretRight(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    juce::Path p;
    const float cx = area.getCentreX(), cy = area.getCentreY();
    const float s = 4.0f;
    p.addTriangle(cx - s * 0.55f, cy - s, cx + s * 0.7f, cy, cx - s * 0.55f, cy + s);
    g.setColour(colour);
    g.fillPath(p);
}

inline void drawCaretUpDown(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    const float cx = area.getCentreX(), cy = area.getCentreY();
    g.setColour(colour);
    juce::Path up, dn;
    up.addTriangle(cx - 3.5f, cy + 1.5f, cx + 3.5f, cy + 1.5f, cx, cy - 2.5f);
    dn.addTriangle(cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
    g.fillPath(up);
    g.fillPath(dn);
}

inline juce::String signedIntText(int v)
{
    if (v > 0) return "+" + juce::String(v);
    return juce::String(v);
}

class FlatTextButton : public juce::Button
{
public:
    explicit FlatTextButton(const juce::String& text) : juce::Button(text), label(text)
    {
        setWantsKeyboardFocus(false);
    }

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        // primaryStyle always paints as a solid CTA (Generate → New).
        if (primaryStyle)
        {
            auto face = r;
            if (themeDepth() == ThemeDepth::Brutal)
            {
                const float o = juce::jmin(3.0f, themeBorderPx());
                face = r.withTrimmedRight(o).withTrimmedBottom(o);
                ds::shadow::control(g, face, 0.0f);
            }
            auto fill = ds::primary();
            if (down) fill = fill.withMultipliedAlpha(0.85f);
            else if (over) fill = fill.withMultipliedAlpha(0.92f);
            g.setColour(fill);
            g.fillRoundedRectangle(face, ds::radius::md);
            if (themeDepth() == ThemeDepth::Brutal)
            {
                g.setColour(ds::ink());
                g.drawRoundedRectangle(face.reduced(1.0f), ds::radius::md,
                                      juce::jmax(1.5f, themeBorderPx()));
            }
            g.setColour(ds::onPrimary());
        }
        else if (active)
        {
            // Levels active: violet tint + 1px violet border, label stays ink.
            ds::drawActive(g, r, ds::radius::md);
            g.setColour(ds::ink());
        }
        else
        {
            drawInspectorControlSurface(g, r, over, down);
            g.setColour(t.rowLabel);
        }
        g.setFont(inspectorFont());
        g.drawFittedText(label, getLocalBounds().reduced(8, 0), juce::Justification::centred, 1);
        if (!isEnabled())
        {
            g.setColour(t.panelBg.withAlpha(0.45f));
            g.fillRoundedRectangle(r, metrics::controlRadius());
        }
    }

    int idealWidth() const
    {
        return (int) std::ceil(juce::GlyphArrangement::getStringWidth(inspectorFont(), label)) + 16;
    }

    juce::String label;
    bool active = false;
    bool primaryStyle = false; // solid primary CTA (Generate / New)
};

// ── FlatSwitch ───────────────────────────────────────────────────────────────

class FlatSwitch : public juce::Button
{
public:
    FlatSwitch() : juce::Button({})
    {
        setClickingTogglesState(true);
        setWantsKeyboardFocus(true);
        setOpaque(false);
        // Kill default LookAndFeel button fills that flash white behind rounded corners.
        setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    }

    void paintButton(juce::Graphics& g, bool, bool) override
    {
        // Levels switch: ~28×16 pill. Inset so thick theme borders never clip.
        const bool on = getToggleState();
        const bool tipHot = hasTooltipHighlight(this);
        const float border = themeDepth() == ThemeDepth::Brutal
                                 ? juce::jmax(1.5f, themeBorderPx())
                                 : 1.0f;
        const float pad = border + 1.0f;
        const float maxH = juce::jmax(10.0f, (float) getHeight() - pad * 2.0f);
        const float h = juce::jmin(16.0f, maxH);
        const float w = juce::jmin((float) getWidth() - pad * 2.0f, h * 1.75f);
        auto face = getLocalBounds().toFloat().withSizeKeepingCentre(w, h);
        const float rad = face.getHeight() * 0.5f;

        juce::Path trackPath;
        trackPath.addRoundedRectangle(face, rad);
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(trackPath);
            auto track = tipHot ? tooltipAccent() : (on ? ds::acc() : ds::edge());
            if (!isEnabled()) track = track.withMultipliedAlpha(0.45f);
            else if (isMouseOver() && !on)
                track = track.withMultipliedAlpha(0.8f).overlaidWith(ds::inkFaint().withAlpha(0.25f));
            g.setColour(track);
            g.fillRect(face); // clipped to rounded path — no white corner bleed
            const float knob = juce::jmax(6.0f, h - 4.0f);
            auto knobR = juce::Rectangle<float>(knob, knob)
                             .withCentre({ on ? face.getRight() - rad : face.getX() + rad,
                                           face.getCentreY() });
            g.setColour(on ? juce::Colours::white : ds::surface100());
            g.fillEllipse(knobR);
        }
        if (themeDepth() == ThemeDepth::Brutal)
        {
            g.setColour(ds::ink());
            g.drawRoundedRectangle(face.reduced(border * 0.5f), rad, border);
        }
        if (hasKeyboardFocus(true))
            drawFocusRing(g, face, rad);
    }

    int idealWidth() const { return 28; }
};

// ── FlatPopup ────────────────────────────────────────────────────────────────

class FlatPopup : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatPopup() { setWantsKeyboardFocus(true); }
    std::function<void(int)> onChange;

    void setItems(const juce::StringArray& items, int selected)
    {
        labels = items;
        index = juce::jlimit(0, juce::jmax(0, labels.size() - 1), selected);
        repaint();
    }

    int getIndex() const { return index; }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (labels.isEmpty()) return;
        if (!hasKeyboardFocus(true))
            grabKeyboardFocus();
        showMenu();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (labels.isEmpty()) return false;
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        {
            showMenu();
            return true;
        }
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::leftKey)
        {
            setIndex(juce::jmax(0, index - 1), juce::sendNotification);
            return true;
        }
        if (key == juce::KeyPress::downKey || key == juce::KeyPress::rightKey)
        {
            setIndex(juce::jmin(labels.size() - 1, index + 1), juce::sendNotification);
            return true;
        }
        return false;
    }

    void setIndex(int i, juce::NotificationType notify)
    {
        i = juce::jlimit(0, juce::jmax(0, labels.size() - 1), i);
        if (i == index) return;
        index = i;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(index);
    }

    int idealWidth() const
    {
        auto font = popupFont();
        float maxW = 0.0f;
        for (const auto& s : labels)
            maxW = juce::jmax(maxW, font.getStringWidthFloat(s));
        // pad + caret column; never narrower than the Caps select.
        return juce::jmax(kSelectW, (int) std::ceil((double) maxW) + 28);
    }

    void paint(juce::Graphics& g) override
    {
        // Groove field with a chevron: surface-300, edge border, accent outline on hover.
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre((float) getWidth(), juce::jmin((float) getHeight(), 20.0f));
        fillTallWell(g, r, isMouseOver(), this);

        auto caret = r.removeFromRight(16.0f).withSizeKeepingCentre(10.0f, 10.0f);
        drawStrokedChevron(g, caret, ds::inkMuted(), true);

        auto textArea = r.reduced(6.0f, 0.0f);
        g.setColour(hasTooltipHighlight(this) ? juce::Colour(0xff1a1200) : ds::ink());
        g.setFont(monoFontFixed(11.0f));
        const juce::String val = labels.size() > index ? labels[index] : juce::String();
        g.drawText(val, textArea.toNearestInt(), juce::Justification::centredLeft, false);

        if (hasKeyboardFocus(true))
            drawFocusRing(g, getLocalBounds().toFloat(), kTallRadius);
    }

private:
    class MenuList : public juce::Component
    {
    public:
        MenuList(FlatPopup& o, juce::StringArray items, int selected, int maxH)
            : owner(o), labels(std::move(items)), index(selected)
        {
            const int rowH = 24;
            float textW = 0.0f;
            for (const auto& l : labels)
                textW = juce::jmax(textW, juce::GlyphArrangement::getStringWidth(inspectorFont(), l));
            const int menuW = juce::jmax(o.getWidth(), (int) std::ceil(textW) + 28);
            const int fullH = labels.size() * rowH + 8;
            setSize(menuW, juce::jmin(fullH, juce::jmax(rowH * 8 + 8, maxH)));
        }

        void paint(juce::Graphics& g) override
        {
            const auto& t = inspectorTokens();
            auto bounds = getLocalBounds().toFloat();
            g.setColour(t.panelBg);
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour(t.controlHairline);
            g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 0.5f);

            const int rowH = 24;
            auto body = getLocalBounds().reduced(4);
            const int first = juce::jlimit(0, juce::jmax(0, labels.size() - 1), scroll);
            const int visible = juce::jmax(1, (body.getHeight()) / rowH);
            for (int i = first; i < labels.size() && i < first + visible; ++i)
            {
                auto row = body.removeFromTop(rowH).toFloat();
                const bool hi = (i == hover);
                const bool sel = (i == index);
                if (hi || sel)
                {
                    g.setColour(hi ? accentFillColour() : t.accent.withAlpha(0.18f));
                    g.fillRoundedRectangle(row, 4.0f);
                }
                g.setFont(inspectorFont());
                g.setColour(hi ? onAccentColour() : t.rowLabel);
                g.drawText(labels[i], row.reduced(8.0f, 0.0f).toNearestInt(),
                           juce::Justification::centredLeft, true);
            }
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            const int rowH = 24;
            const int visibleIdx = (e.y - 4) / rowH;
            const int h = scroll + visibleIdx;
            const int clamped = juce::isPositiveAndBelow(h, labels.size()) ? h : -1;
            if (clamped != hover) { hover = clamped; repaint(); }
        }

        void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }

        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            const int rowH = 24;
            const int visible = juce::jmax(1, (getHeight() - 8) / rowH);
            const int maxScroll = juce::jmax(0, labels.size() - visible);
            scroll = juce::jlimit(0, maxScroll, scroll - (int) std::lround(wheel.deltaY * 3.0f));
            repaint();
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            const int rowH = 24;
            const int i = scroll + (e.y - 4) / rowH;
            if (juce::isPositiveAndBelow(i, labels.size()))
                owner.setIndex(i, juce::sendNotification);
            if (auto* overlay = getParentComponent())
            {
                if (auto* host = overlay->getParentComponent())
                    host->removeChildComponent(overlay);
                delete overlay;
            }
        }

        FlatPopup& owner;
        juce::StringArray labels;
        int index = 0;
        int hover = -1;
        int scroll = 0;
    };

    void showMenu()
    {
        auto* host = getTopLevelComponent();
        if (host == nullptr) return;

        if (auto* existing = host->findChildWithID("flatPopupMenuHost"))
        {
            host->removeChildComponent(existing);
            delete existing;
        }

        struct Veil : juce::Component
        {
            void mouseDown(const juce::MouseEvent&) override
            {
                if (auto* p = getParentComponent())
                    p->removeChildComponent(this);
                delete this;
            }
        };

        auto* overlay = new Veil();
        overlay->setComponentID("flatPopupMenuHost");
        overlay->setBounds(host->getLocalBounds());
        host->addAndMakeVisible(overlay);
        overlay->toFront(true);

        const int maxH = juce::jmax(200, host->getHeight() - 40);
        auto* menu = new MenuList(*this, labels, index, maxH);
        // Start scrolled so the selected item is visible.
        {
            const int rowH = 24;
            const int visible = juce::jmax(1, (menu->getHeight() - 8) / rowH);
            menu->scroll = juce::jlimit(0, juce::jmax(0, labels.size() - visible),
                                        index - visible / 2);
        }
        const auto screen = localAreaToGlobal(getLocalBounds());
        const auto hostScreen = host->getScreenBounds();
        int x = screen.getX() - hostScreen.getX();
        int y = screen.getBottom() - hostScreen.getY() + 2;
        if (y + menu->getHeight() > host->getHeight())
            y = screen.getY() - hostScreen.getY() - menu->getHeight() - 2;
        y = juce::jlimit(4, host->getHeight() - menu->getHeight() - 4, y);
        x = juce::jlimit(4, host->getWidth() - menu->getWidth() - 4, x);
        menu->setBounds(x, y, menu->getWidth(), menu->getHeight());
        overlay->addAndMakeVisible(menu);
        menu->toFront(false);
    }

    juce::StringArray labels;
    int index = 0;
};

// ── FlatStepper ──────────────────────────────────────────────────────────────

class FlatStepper : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatStepper() { setWantsKeyboardFocus(false); }
    int minV = -3, maxV = 3, value = 0;
    std::function<void(int)> onChange;
    std::function<juce::String(int)> format;

    void setValue(int v, juce::NotificationType notify = juce::sendNotification)
    {
        v = juce::jlimit(minV, maxV, v);
        if (v == value) return;
        value = v;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(value);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat();
        const float minusW = bare ? r.getWidth() / 3.0f : 24.0f;
        const float valueW = bare ? r.getWidth() / 3.0f : 30.0f;
        if (e.x < minusW) setValue(value - 1);
        else if (e.x > minusW + valueW) setValue(value + 1);
    }

    bool bare = false; // no chrome — plain − value + 

    int idealWidth() const { return bare ? 56 : (24 + 30 + 24); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        if (!bare)
        {
            g.setColour(ds::ctl());
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(ds::ctlb());
            g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
        }

        const float minusW = bare ? r.getWidth() / 3.0f : 24.0f;
        const float valueW = bare ? r.getWidth() / 3.0f : 30.0f;
        const float plusW = r.getWidth() - minusW - valueW;
        if (!bare)
        {
            g.setColour(t.controlSeparator);
            g.fillRect(r.getX() + minusW, r.getY() + 3.0f, 0.5f, r.getHeight() - 6.0f);
            g.fillRect(r.getX() + minusW + valueW, r.getY() + 3.0f, 0.5f, r.getHeight() - 6.0f);
        }
        const float seg = minusW; // mouse hit uses equal thirds below
        juce::ignoreUnused(seg, plusW);

        g.setColour(t.rowLabel);
        g.setFont(inspectorFont());
        g.drawFittedText("-", r.withWidth(minusW).toNearestInt(), juce::Justification::centred, 1);
        g.drawFittedText("+", r.withTrimmedLeft(minusW + valueW).toNearestInt(),
                         juce::Justification::centred, 1);

        const juce::String text = format ? format(value) : signedIntText(value);
        g.setFont(inspectorMono(true));
        g.setColour(value == 0 ? t.valueText : t.headerText);
        g.drawFittedText(text, r.withTrimmedLeft(minusW).withWidth(valueW).toNearestInt(),
                         juce::Justification::centred, 1);
    }
};

// ── TempoToggle ──────────────────────────────────────────────────────────────

class TempoToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    TempoToggle()
    {
        setWantsKeyboardFocus(false);
        setTooltip("Halve or double playback tempo vs the clip / host");
    }
    enum class Sel { None, Half, Double };
    std::function<void(double)> onChange;

    void setMultiplier(double m, juce::NotificationType notify = juce::dontSendNotification)
    {
        Sel next = Sel::None;
        if (m < 0.75) next = Sel::Half;
        else if (m > 1.5) next = Sel::Double;
        if (next == selected) return;
        selected = next;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(multiplier());
    }

    double multiplier() const
    {
        switch (selected)
        {
            case Sel::Half: return 0.5;
            case Sel::Double: return 2.0;
            case Sel::None: break;
        }
        return 1.0;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat();
        const bool left = e.x < r.getCentreX();
        const Sel seg = left ? Sel::Half : Sel::Double;
        selected = (selected == seg) ? Sel::None : seg;
        repaint();
        if (onChange) onChange(multiplier());
    }

    int idealWidth() const
    {
        // Fit Half|Double into half a toolkit row (same as former select width).
        return kSelectHalfW;
    }

    void paint(juce::Graphics& g) override
    {
        // Two Groove toggles side by side (2 px apart).
        // Levels segmented control: muted track, selected cell is a raised
        // surface with an edge border.
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre((float) getWidth(), 22.0f);
        g.setColour(ds::surface300());
        g.fillRoundedRectangle(r, ds::radius::md);
        auto inner = r.reduced(2.0f);
        auto left = inner.withTrimmedRight(inner.getWidth() * 0.5f);
        auto right = inner.withTrimmedLeft(inner.getWidth() * 0.5f);
        for (auto [cell, sel] : { std::pair<juce::Rectangle<float>, Sel>{ left, Sel::Half },
                                  std::pair<juce::Rectangle<float>, Sel>{ right, Sel::Double } })
        {
            if (selected != sel) continue;
            g.setColour(ds::surface200());
            g.fillRoundedRectangle(cell, ds::radius::md - 2.0f);
            g.setColour(ds::edge());
            g.drawRoundedRectangle(cell.reduced(0.5f), ds::radius::md - 2.0f, 1.0f);
        }

        g.setFont(uiFontFixed(12.0f, true));
        auto textCol = [&](Sel s)
        {
            return selected == s ? ds::ink() : ds::inkMuted();
        };
        g.setColour(textCol(Sel::Half));
        g.drawText("Half", left.toNearestInt(), juce::Justification::centred, false);
        g.setColour(textCol(Sel::Double));
        g.drawText("Double", right.toNearestInt(), juce::Justification::centred, false);
    }

private:
    Sel selected = Sel::None;
};

// ── FlatSliderRow (tall Photos Adjust: label + value inside, hover highlight) ─

class FlatSliderRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatSliderRow(const juce::String& l, int mn, int mx, int d, bool bi)
        : label(l), minV(mn), maxV(mx), defV(d), value(d), bipolar(bi)
    {
        setWantsKeyboardFocus(false);
        setRepaintsOnMouseActivity(true);
    }

    enum class Style { Inline, Well };
    Style style = Style::Inline;
    /** Sub-row label (grid+amount) draws in tx3. */
    bool subduedLabel = false;

    std::function<void(int)> onChange;
    juce::String valueText;

    void setValue(int v, juce::NotificationType notify = juce::sendNotification)
    {
        v = juce::jlimit(minV, maxV, v);
        if (v == value) return;
        value = v;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(value);
    }

    int getValue() const { return value; }
    /** No label → the track starts at the left edge (e.g. Settings › Contrast). */
    int labelColW() const { return label.isEmpty() ? 0 : kLabelColW; }
    void setDefault(int d) { defV = juce::jlimit(minV, maxV, d); }

    void mouseMove(const juce::MouseEvent& e) override
    {
        if (style == Style::Inline)
            setMouseCursor(trackLane().contains(e.position)
                               ? juce::MouseCursor::LeftRightResizeCursor
                               : juce::MouseCursor::NormalCursor);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (style == Style::Inline && !trackLane().contains(e.position))
            return;
        setFromX(e.position.x);
    }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent&) override { setValue(defV); }

    void paint(juce::Graphics& g) override
    {
        if (style == Style::Inline)
        {
            paintInline(g);
            return;
        }
        const auto& t = inspectorTokens();
        auto r = getLocalBounds().toFloat();
        const bool hot = isMouseOverOrDragging();
        fillTallWell(g, r, false, this);

        const float norm = (float) (value - minV) / (float) juce::jmax(1, maxV - minV);
        juce::Rectangle<float> fill;
        if (bipolar)
        {
            const float mid = r.getCentreX();
            const float x = r.getX() + norm * r.getWidth();
            fill = juce::Rectangle<float>::leftTopRightBottom(
                juce::jmin(mid, x), r.getY(), juce::jmax(mid, x), r.getBottom());
        }
        else
        {
            fill = r.withWidth(juce::jmax(0.0f, r.getWidth() * norm));
        }
        g.setColour(accentFillColour());
        g.fillRoundedRectangle(fill, kTallRadius);

        if (hot)
        {
            g.setColour(t.accent);
            g.drawRoundedRectangle(r.reduced(0.5f), kTallRadius, 1.2f);
        }

        auto pad = r.reduced(kTallPadX, 0.0f).toNearestInt();
        const auto text = valueText.isNotEmpty() ? valueText
                      : grooveValueText({ label.toRawUTF8(), label.toRawUTF8(), minV, maxV, defV }, value);

        auto drawTexts = [&](juce::Colour labelCol, juce::Colour valueCol)
        {
            g.setFont(inspectorFont());
            g.setColour(labelCol);
            g.drawText(label, pad, juce::Justification::centredLeft, true);
            g.setFont(inspectorMono());
            g.setColour(valueCol);
            g.drawText(text, pad, juce::Justification::centredRight, true);
        };
        // Unfilled well keeps body type; the accent portion uses white.
        drawTexts(hot ? t.headerText : t.rowLabel, t.headerText);
        if (fill.getWidth() > 0.5f)
        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(fill.getSmallestIntegerContainer());
            drawTexts(onAccentColour(), onAccentColour());
        }
    }

private:
    juce::Rectangle<float> trackLane() const
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromLeft((float) labelColW() + 10.0f);
        r.removeFromRight((float) kValueColW + 8.0f);
        return r;
    }

    /** Thumb-centre travel, inset so the 14px thumb stays inside the lane. */
    juce::Range<float> thumbTravel() const
    {
        auto lane = trackLane();
        const float inset = 7.0f;
        return { lane.getX() + inset, lane.getRight() - inset };
    }

    void paintInline(juce::Graphics& g) const
    {
        const auto& t = inspectorTokens();
        auto bounds = getLocalBounds();
        auto labelR = bounds.removeFromLeft(labelColW());
        auto valueR = bounds.removeFromRight(kValueColW);
        const bool atDefault = value == defV;
        const auto text = valueText.isNotEmpty() ? valueText
                      : grooveValueText({ label.toRawUTF8(), label.toRawUTF8(), minV, maxV, defV }, value);

        g.setFont(inspectorFont());
        g.setColour(subduedLabel ? t.valueText : t.rowLabel);
        g.drawText(label, labelR, juce::Justification::centredLeft, true);
        g.setFont(inspectorMono(false));
        g.setColour(atDefault ? t.valueText : t.headerText);
        g.drawText(text, valueR, juce::Justification::centredRight, true);

        auto lane = trackLane();
        auto track = lane.withSizeKeepingCentre(lane.getWidth(), 2.0f);
        g.setColour(ds::trk());
        g.fillRoundedRectangle(track, 1.0f);

        const auto travel = thumbTravel();
        const float span = juce::jmax(1.0f, travel.getEnd() - travel.getStart());
        const float norm = (float) (value - minV) / (float) juce::jmax(1, maxV - minV);
        const float x = travel.getStart() + norm * span;
        const float centreNorm = (float) (defV - minV) / (float) juce::jmax(1, maxV - minV);
        const float cx = travel.getStart() + centreNorm * span;

        if (bipolar)
        {
            g.setColour(t.valueText.withAlpha(0.50f));
            g.fillRect(cx - 0.5f, lane.getCentreY() - 5.0f, 1.0f, 10.0f);
            auto fill = juce::Rectangle<float>::leftTopRightBottom(
                juce::jmin(cx, x), track.getY(), juce::jmax(cx, x), track.getBottom());
            // Levels: primary filled track (LvRow spec).
            g.setColour(ds::primary());
            g.fillRoundedRectangle(fill, 1.0f);
        }
        else
        {
            auto fill = juce::Rectangle<float>::leftTopRightBottom(
                track.getX(), track.getY(), x, track.getBottom());
            g.setColour(ds::primary());
            g.fillRoundedRectangle(fill, 1.0f);
        }

        auto thumb = juce::Rectangle<float>(x - 7.0f, lane.getCentreY() - 7.0f, 14.0f, 14.0f);
        ds::shadow::thumb(g, thumb);
    }

    void setFromX(float x)
    {
        float t = 0.0f;
        if (style == Style::Inline)
        {
            const auto travel = thumbTravel();
            const float span = juce::jmax(1.0f, travel.getEnd() - travel.getStart());
            t = juce::jlimit(0.0f, 1.0f, (x - travel.getStart()) / span);
        }
        else
        {
            const float w = (float) juce::jmax(1, getWidth());
            t = juce::jlimit(0.0f, 1.0f, x / w);
        }
        setValue(minV + (int) std::lround(t * (float) (maxV - minV)));
    }

    juce::String label;
    int minV, maxV, defV, value;
    bool bipolar = false;
};

// ── FlatRangeSliderRow (dual-thumb velocity / similar ranges) ─────────────────

class FlatRangeSliderRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    FlatRangeSliderRow(const juce::String& l, int mn, int mx, int defLo, int defHi)
        : label(l), minV(mn), maxV(mx), lo(defLo), hi(defHi), defLoV(defLo), defHiV(defHi)
    {
        setWantsKeyboardFocus(true);
        setRepaintsOnMouseActivity(true);
    }

    enum class Style { Inline, Well };
    Style style = Style::Inline;

    std::function<void(int, int)> onChange;
    juce::String valueText;
    /** Library search: white label/value on accent fill (toolkit uses accent fill too). */
    bool accentFill = false;

    int idealHeight() const
    {
        if (style == Style::Well || accentFill)
            return 36;
        return 18 + 7 + 24;
    }

    void setRange(int newLo, int newHi, juce::NotificationType notify = juce::sendNotification)
    {
        newLo = juce::jlimit(minV, maxV, newLo);
        newHi = juce::jlimit(minV, maxV, newHi);
        if (newHi < newLo) std::swap(newLo, newHi);
        if (newLo == lo && newHi == hi) return;
        lo = newLo;
        hi = newHi;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(lo, hi);
    }

    int getLo() const { return lo; }
    int getHi() const { return hi; }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!hasKeyboardFocus(true))
            grabKeyboardFocus();
        dragThumb = hitThumb(e.position.x);
        setFromX(e.position.x);
    }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent&) override { setRange(defLoV, defHiV); }

    bool keyPressed(const juce::KeyPress& key) override
    {
        const int step = key.getModifiers().isShiftDown() ? 5 : 1;
        if (key == juce::KeyPress::leftKey)
        {
            setRange(lo - step, hi - step);
            return true;
        }
        if (key == juce::KeyPress::rightKey)
        {
            setRange(lo + step, hi + step);
            return true;
        }
        if (key == juce::KeyPress::downKey)
        {
            setRange(lo, hi - step);
            return true;
        }
        if (key == juce::KeyPress::upKey)
        {
            setRange(lo, hi + step);
            return true;
        }
        return false;
    }

    void paint(juce::Graphics& g) override
    {
        if (style == Style::Inline && !accentFill)
        {
            paintInline(g);
            return;
        }
        const auto& t = inspectorTokens();
        auto bounds = getLocalBounds().toFloat();
        const bool hot = isMouseOverOrDragging() || hasKeyboardFocus(true);

        constexpr float kThumbD = 11.0f;
        constexpr float kTrackH = 5.0f;
        constexpr float kLabelH = 13.0f;
        const float inset = kThumbD * 0.5f + 1.0f;

        auto labelRow = bounds.removeFromTop(kLabelH).reduced(2.0f, 0.0f);
        const auto text = valueText.isNotEmpty() ? valueText
                      : (juce::String(lo) + "-" + juce::String(hi));
        g.setFont(accentFill ? uiFontFixed(kAnnotPt) : inspectorFont());
        g.setColour(hot ? t.headerText : t.rowLabel);
        g.drawText(label, labelRow.toNearestInt(), juce::Justification::centredLeft, true);
        g.setFont(accentFill ? monoFontFixed(kAnnotPt) : inspectorMono());
        g.setColour(t.headerText);
        g.drawText(text, labelRow.toNearestInt(), juce::Justification::centredRight, true);

        // Compact track inset so end thumbs sit fully inside the component.
        auto track = juce::Rectangle<float>(bounds.getX() + inset,
                                            bounds.getCentreY() - kTrackH * 0.5f,
                                            juce::jmax(1.0f, bounds.getWidth() - inset * 2.0f),
                                            kTrackH);
        g.setColour(t.tallFill);
        g.fillRoundedRectangle(track, track.getHeight() * 0.5f);
        g.setColour(t.controlHairline);
        g.drawRoundedRectangle(track.reduced(0.5f), track.getHeight() * 0.5f, 1.0f);

        const float span = (float) juce::jmax(1, maxV - minV);
        const float xLo = track.getX() + track.getWidth() * ((float) (lo - minV) / span);
        const float xHi = track.getX() + track.getWidth() * ((float) (hi - minV) / span);
        auto fill = juce::Rectangle<float>::leftTopRightBottom(
            xLo, track.getY(), xHi, track.getBottom());
        g.setColour(ds::primary());
        g.fillRoundedRectangle(fill, track.getHeight() * 0.5f);

        auto thumbAt = [&](float x)
        {
            return juce::Rectangle<float>(x - kThumbD * 0.5f,
                                          track.getCentreY() - kThumbD * 0.5f,
                                          kThumbD, kThumbD);
        };
        ds::shadow::thumb(g, thumbAt(xLo));
        ds::shadow::thumb(g, thumbAt(xHi));
        if (hot)
        {
            g.setColour(ds::acc());
            g.drawEllipse(thumbAt(xLo).reduced(0.5f), 1.0f);
            g.drawEllipse(thumbAt(xHi).reduced(0.5f), 1.0f);
        }
        if (hasKeyboardFocus(true))
            drawFocusRing(g, getLocalBounds().toFloat(), kTallRadius);
    }

    void paintInline(juce::Graphics& g) const
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds();
        auto header = r.removeFromTop(18);
        const bool atDefault = lo <= defLoV && hi >= defHiV;
        const auto text = valueText.isNotEmpty() ? valueText
                      : (juce::String(lo) + "-" + juce::String(hi));
        g.setFont(inspectorFont());
        g.setColour(t.rowLabel);
        g.drawText(label, header, juce::Justification::centredLeft, true);
        g.setFont(inspectorMono());
        g.setColour(atDefault ? t.valueText : ds::ink());
        g.drawText(text, header, juce::Justification::centredRight, true);

        r.removeFromTop(7);
        auto lane = r.removeFromTop(24).toFloat();
        constexpr float kThumbD = 14.0f;
        const float inset = kThumbD * 0.5f;
        auto track = lane.reduced(inset, 0.0f).withSizeKeepingCentre(lane.getWidth() - inset * 2.0f, 2.0f);
        g.setColour(ds::trk());
        g.fillRoundedRectangle(track, 1.0f);
        const float span = (float) juce::jmax(1, maxV - minV);
        const float xLo = track.getX() + track.getWidth() * ((float) (lo - minV) / span);
        const float xHi = track.getX() + track.getWidth() * ((float) (hi - minV) / span);
        auto fill = juce::Rectangle<float>::leftTopRightBottom(xLo, track.getY(), xHi, track.getBottom());
        g.setColour(ds::primary());
        g.fillRoundedRectangle(fill, 1.0f);
        auto thumbAt = [&](float x)
        {
            return juce::Rectangle<float>(x - kThumbD * 0.5f, lane.getCentreY() - kThumbD * 0.5f,
                                          kThumbD, kThumbD);
        };
        ds::shadow::thumb(g, thumbAt(xLo));
        ds::shadow::thumb(g, thumbAt(xHi));
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.drawEllipse(thumbAt(xLo), 0.5f);
        g.drawEllipse(thumbAt(xHi), 0.5f);
    }

private:
    enum class Thumb { None, Lo, Hi };
    Thumb dragThumb = Thumb::None;

    Thumb hitThumb(float x) const
    {
        constexpr float kInset = 11.0f * 0.5f + 1.0f;
        const float span = (float) juce::jmax(1, maxV - minV);
        const float trackW = juce::jmax(1.0f, (float) getWidth() - kInset * 2.0f);
        const float xLo = kInset + trackW * ((float) (lo - minV) / span);
        const float xHi = kInset + trackW * ((float) (hi - minV) / span);
        return std::abs(x - xLo) <= std::abs(x - xHi) ? Thumb::Lo : Thumb::Hi;
    }

    void setFromX(float x)
    {
        constexpr float kInset = 11.0f * 0.5f + 1.0f;
        const float trackW = juce::jmax(1.0f, (float) getWidth() - kInset * 2.0f);
        const float t = juce::jlimit(0.0f, 1.0f, (x - kInset) / trackW);
        const int v = minV + (int) std::lround(t * (float) (maxV - minV));
        if (dragThumb == Thumb::Lo)
            setRange(juce::jmin(v, hi), hi);
        else
            setRange(lo, juce::jmax(v, lo));
    }

    juce::String label;
    int minV, maxV, lo, hi, defLoV, defHiV;
};

// ── ChipGrid (2×N key / articulation / mode buttons) ─────────────────────────

class ChipGrid : public juce::Component
{
public:
    /** SolidPrimary = key chips (primary fill + onPrimary text).
        Accent = Kind/Tags (violet tint + border, ink text). */
    enum class SelectStyle { SolidPrimary = 0, Accent };

    std::function<void()> onChange;
    bool multiSelect = false;
    int columns = 6;
    /** When true, chip width follows label text and rows wrap to available width. */
    bool fitContent = false;
    /** Sidebar filter: match Include Subfolders / Favorites label styling. */
    bool sidebarLabelStyle = false;
    /** Header toggles the chip body. Effects inspector leaves this off. */
    bool collapsible = false;
    bool collapsed = true;
    std::function<void()> onCollapsedChanged;
    /** Selected index treated as the untouched default (summary in tx3). */
    int neutralIndex = 0;
    /** When true (Kind), summary reads "3 of 4" instead of listing names. */
    bool countSummary = false;
    SelectStyle selectStyle = SelectStyle::SolidPrimary;
    juce::String hint;

    void setItems(juce::StringArray labelsIn)
    {
        labels = std::move(labelsIn);
        selected.assign((size_t) labels.size(), false);
        candidateMask = 0;
        layoutDirty = true;
        repaint();
        resized();
    }

    void setSelectedIndex(int idx, juce::NotificationType notify = juce::dontSendNotification)
    {
        if (labels.isEmpty()) return;
        selected.assign((size_t) labels.size(), false);
        if (juce::isPositiveAndBelow(idx, labels.size()))
            selected[(size_t) idx] = true;
        repaint();
        if (notify != juce::dontSendNotification && onChange) onChange();
    }

    void setSelectedMask(uint16_t mask, juce::NotificationType notify = juce::dontSendNotification)
    {
        selected.assign((size_t) labels.size(), false);
        for (int i = 0; i < labels.size() && i < 16; ++i)
            selected[(size_t) i] = (mask & (uint16_t) (1u << i)) != 0;
        repaint();
        if (notify != juce::dontSendNotification && onChange) onChange();
    }

    /** Soft-highlight chips that match clip analysis (does not change selection). */
    void setCandidateMask(uint16_t mask)
    {
        if (candidateMask == mask) return;
        candidateMask = mask;
        repaint();
    }

    int getSelectedIndex() const
    {
        for (int i = 0; i < (int) selected.size(); ++i)
            if (selected[(size_t) i]) return i;
        return -1;
    }

    uint16_t getSelectedMask() const
    {
        uint16_t m = 0;
        for (int i = 0; i < (int) selected.size() && i < 16; ++i)
            if (selected[(size_t) i]) m = (uint16_t) (m | (uint16_t) (1u << i));
        return m;
    }

    juce::String selectedSummary() const
    {
        int n = 0;
        juce::StringArray parts;
        for (int i = 0; i < labels.size(); ++i)
            if (i < (int) selected.size() && selected[(size_t) i])
            {
                ++n;
                parts.add(labels[i]);
            }
        if (countSummary)
        {
            if (n <= 0 || n >= labels.size())
                return "Any";
            return juce::String(n) + " of " + juce::String(labels.size());
        }
        if (!parts.isEmpty())
            return parts.joinIntoString(", ");
        // Fall back to candidate names when nothing is exclusively selected.
        for (int i = 0; i < labels.size() && i < 16; ++i)
            if ((candidateMask & (uint16_t) (1u << i)) != 0)
                parts.add(labels[i]);
        return parts.isEmpty() ? "Any" : parts.joinIntoString(", ");
    }

    int idealHeight() const
    {
        if (labels.isEmpty() || (collapsible && collapsed)) return headerStrip();
        if (!fitContent)
        {
            const int rows = (labels.size() + columns - 1) / juce::jmax(1, columns);
            int h = headerStrip() + 7 + rows * kChipH + juce::jmax(0, rows - 1) * kChipGap;
            if (hint.isNotEmpty()) h += 4 + 14;
            return h;
        }
        const int w = juce::jmax(120, getWidth() > 10 ? getWidth()
                                    : metrics::effectsPaneWidth() - kContentPadX - kContentPadR);
        int h = headerStrip() + 7 + measureWrappedHeight(w);
        if (hint.isNotEmpty()) h += 4 + 14;
        return h;
    }

    juce::String title;

    void resized() override
    {
        layoutDirty = true;
        rebuildChipBounds();
    }

    void paint(juce::Graphics& g) override
    {
        rebuildChipBounds();
        const auto& t = inspectorTokens();
        auto header = getLocalBounds().removeFromTop(headerStrip());
        const juce::String titleDraw = sidebarLabelStyle ? title.toUpperCase() : title;
        // Filter sections: FORM-style header strip (chevron + caps title + hairlines).
        if (sidebarLabelStyle && collapsible)
        {
            g.setColour(ds::surface100());
            g.fillRect(header);
            g.setColour(ds::line());
            g.fillRect(header.getX(), header.getY(), header.getWidth(), 1);
            g.fillRect(header.getX(), header.getBottom() - 1, header.getWidth(), 1);
        }
        if (sidebarLabelStyle)
        {
            g.setFont(sectionTitleFont());
            g.setColour(ds::ink());
        }
        else
        {
            g.setFont(inspectorFont());
            g.setColour(t.rowLabel);
        }
        if (collapsible)
        {
            const int titleW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(
                g.getCurrentFont(), titleDraw));
            if (sidebarLabelStyle)
            {
                auto chev = header.removeFromLeft(16).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
                drawStrokedChevron(g, chev, ds::inkMuted(), !collapsed);
                header.removeFromLeft(2);
            }
            auto titleR = header.removeFromLeft(juce::jmin(titleW, juce::jmax(0, header.getWidth() - 48)));
            g.drawText(titleDraw, titleR, juce::Justification::centredLeft, true);
            if (!sidebarLabelStyle)
            {
                header.removeFromLeft(4);
                auto chev = header.removeFromLeft(12).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
                drawStrokedChevron(g, chev, t.valueText, !collapsed);
            }
        }
        else
        {
            g.drawText(titleDraw, header.removeFromLeft(header.getWidth() / 2),
                       juce::Justification::centredLeft, true);
        }
        g.setFont(sidebarLabelStyle ? monoFontFixed(12.0f) : inspectorFont());
        const auto summary = selectedSummary();
        const bool summaryDefault = summary.equalsIgnoreCase("Any")
            || (!countSummary && !multiSelect && getSelectedIndex() == neutralIndex);
        g.setColour(summaryDefault ? ds::inkMuted() : ds::ink());
        g.drawText(summary, header, juce::Justification::centredRight, true);

        if (collapsible && collapsed)
            return;

        auto fillChip = [&](juce::Rectangle<float> cell, juce::Colour fill)
        {
            juce::Path p;
            p.addRoundedRectangle(cell, ds::radius::sm);
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(p);
            g.setColour(fill);
            g.fillRect(cell);
        };

        for (int i = 0; i < labels.size(); ++i)
        {
            if (i >= (int) chipBounds.size()) break;
            auto cell = chipBounds[(size_t) i].toFloat();
            const bool on = i < (int) selected.size() && selected[(size_t) i];
            const bool cand = !on && i < 16
                && (candidateMask & (uint16_t) (1u << i)) != 0;
            if (on && selectStyle == SelectStyle::Accent)
            {
                fillChip(cell, ds::selsoft());
                g.setColour(ds::acc());
                g.drawRoundedRectangle(cell.reduced(0.5f), ds::radius::sm, 1.0f);
            }
            else if (on)
            {
                fillChip(cell, ds::primary());
            }
            else
            {
                // Idle key/kind chip: text @ 4 % wash (Levels FilterPopover).
                fillChip(cell, ds::ink().withAlpha(0.04f));
                if (selectStyle == SelectStyle::Accent)
                {
                    g.setColour(ds::line());
                    g.drawRoundedRectangle(cell.reduced(0.5f), ds::radius::sm, 1.0f);
                }
            }
            if (cand)
            {
                g.setColour(ds::acc());
                g.fillEllipse(cell.getRight() - 7.0f, cell.getY() + 3.0f, 4.0f, 4.0f);
            }
            float chipPt = sidebarLabelStyle ? 12.0f : kAnnotPt;
            if (sidebarLabelStyle)
            {
                const float tw = juce::GlyphArrangement::getStringWidth(
                    uiFontFixed(chipPt, on), labels[i]);
                if (tw > cell.getWidth() - 6.0f)
                    chipPt = 9.0f;
            }
            g.setFont(selectStyle == SelectStyle::SolidPrimary && sidebarLabelStyle
                          ? monoFontFixed(chipPt, on)
                          : uiFontFixed(chipPt, on));
            g.setColour(on && selectStyle == SelectStyle::SolidPrimary ? ds::onPrimary()
                        : on ? ds::ink()
                             : ds::inkMuted());
            g.drawText(labels[i], cell.toNearestInt(), juce::Justification::centred, false);
        }
        if (hint.isNotEmpty())
        {
            g.setFont(uiFontFixed(10.5f));
            g.setColour(t.valueText);
            g.drawText(hint, getLocalBounds().removeFromBottom(14),
                       juce::Justification::centredLeft, true);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (collapsible && e.y < headerStrip())
        {
            collapsed = !collapsed;
            layoutDirty = true;
            repaint();
            if (onCollapsedChanged)
                onCollapsedChanged();
            return;
        }
        if (collapsible && collapsed)
            return;
        rebuildChipBounds();
        const int idx = hitIndex(e.getPosition());
        if (idx < 0) return;
        if (multiSelect)
        {
            selected[(size_t) idx] = !selected[(size_t) idx];
        }
        else
        {
            selected.assign((size_t) labels.size(), false);
            selected[(size_t) idx] = true;
        }
        repaint();
        if (onChange) onChange();
    }

private:
    static constexpr int kLabelH = 18;
    static constexpr int kChipH = 24;
    static constexpr int kChipGap = 4;
    static constexpr int kChipPadX = 10;

    int headerStrip() const
    {
        if (sidebarLabelStyle && collapsible)
            return metrics::scaled(28); // FORM-style fold header
        return collapsible ? 22 : kLabelH;
    }

    int chipWidthFor(int idx) const
    {
        const auto f = uiFontFixed(kAnnotPt, false);
        const float tw = juce::GlyphArrangement::getStringWidth(f, labels[idx]);
        return juce::jmax(28, (int) std::ceil(tw) + kChipPadX * 2);
    }

    int measureWrappedHeight(int width) const
    {
        if (labels.isEmpty() || width <= 0) return kChipH;
        int x = 0, rows = 1;
        for (int i = 0; i < labels.size(); ++i)
        {
            const int cw = chipWidthFor(i);
            if (x > 0 && x + cw > width)
            {
                ++rows;
                x = 0;
            }
            x += cw + kChipGap;
        }
        return rows * kChipH + juce::jmax(0, rows - 1) * kChipGap;
    }

    void rebuildChipBounds() const
    {
        if (!layoutDirty && (int) chipBounds.size() == labels.size())
            return;
        layoutDirty = false;
        chipBounds.assign((size_t) labels.size(), {});
        if (collapsible && collapsed)
            return;
        auto body = getLocalBounds().withTrimmedTop(headerStrip() + 7);
        if (hint.isNotEmpty())
            body.removeFromBottom(4 + 14);
        if (body.getWidth() <= 0 || labels.isEmpty())
            return;

        if (!fitContent)
        {
            const int cols = juce::jmax(1, columns);
            const float gap = (float) kChipGap;
            const float cellW = (body.getWidth() - gap * (float) (cols - 1)) / (float) cols;
            const float cellH = (float) kChipH;
            for (int i = 0; i < labels.size(); ++i)
            {
                const int r = i / cols, c = i % cols;
                auto cell = juce::Rectangle<int>(
                    body.getX() + (int) ((float) c * (cellW + gap)),
                    body.getY() + r * (kChipH + kChipGap),
                    (int) cellW, (int) cellH);
                if (c == cols - 1)
                    cell.setRight(body.getRight());
                chipBounds[(size_t) i] = cell;
            }
            return;
        }

        int x = body.getX(), y = body.getY();
        for (int i = 0; i < labels.size(); ++i)
        {
            const int cw = chipWidthFor(i);
            if (x > body.getX() && x + cw > body.getRight())
            {
                x = body.getX();
                y += kChipH + kChipGap;
            }
            chipBounds[(size_t) i] = { x, y, cw, kChipH };
            x += cw + kChipGap;
        }
    }

    int hitIndex(juce::Point<int> pos) const
    {
        for (int i = 0; i < (int) chipBounds.size(); ++i)
            if (chipBounds[(size_t) i].contains(pos))
                return i;
        return -1;
    }

    juce::StringArray labels;
    std::vector<bool> selected;
    uint16_t candidateMask = 0;
    mutable std::vector<juce::Rectangle<int>> chipBounds;
    mutable bool layoutDirty = true;
};

// ── SegmentedSelector (single-row pill group: octave / bars-style) ────────────

class SegmentedSelector : public juce::Component
{
public:
    std::function<void(int)> onChange;
    juce::String title;
    juce::String summary;
    bool summaryDefault = true;
    int nativeIndex = -1;

    void setItems(juce::StringArray labelsIn, int selectedIdx = 0)
    {
        labels = std::move(labelsIn);
        index = juce::jlimit(0, juce::jmax(0, labels.size() - 1), selectedIdx);
        repaint();
    }

    void setIndex(int i, juce::NotificationType notify = juce::dontSendNotification)
    {
        i = juce::jlimit(0, juce::jmax(0, labels.size() - 1), i);
        if (i == index) return;
        index = i;
        repaint();
        if (notify != juce::dontSendNotification && onChange) onChange(index);
    }

    int getIndex() const { return index; }

    int idealHeight() const { return kLabelH + 7 + kTrackH; }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds();
        auto header = r.removeFromTop(kLabelH);
        g.setFont(inspectorFont());
        g.setColour(t.rowLabel);
        g.drawText(title, header, juce::Justification::centredLeft, true);
        if (summary.isNotEmpty())
        {
            g.setColour(summaryDefault ? ds::inkMuted() : ds::ink());
            g.drawText(summary, header, juce::Justification::centredRight, true);
        }

        r.removeFromTop(7);
        auto track = r.removeFromTop(kTrackH).toFloat();

        if (labels.isEmpty()) return;
        // Levels segmented control: muted track, raised selected cell.
        {
            auto bg = juce::Rectangle<float>(track.getX(), track.getCentreY() - 11.0f, track.getWidth(), 22.0f);
            g.setColour(ds::surface300());
            g.fillRoundedRectangle(bg, ds::radius::md);
        }
        const float cellW = (track.getWidth() - 4.0f) / (float) labels.size();
        for (int i = 0; i < labels.size(); ++i)
        {
            auto cell = juce::Rectangle<float>(track.getX() + 2.0f + cellW * (float) i,
                                               track.getCentreY() - 9.0f, cellW, 18.0f);
            if (i == index)
            {
                g.setColour(ds::surface200());
                g.fillRoundedRectangle(cell, ds::radius::sm);
                g.setColour(ds::edge());
                g.drawRoundedRectangle(cell.reduced(0.5f), ds::radius::sm, 1.0f);
            }
            g.setFont(uiFontFixed(kAnnotPt, i == index));
            g.setColour(i == index ? ds::ink() : ds::inkMuted());
            g.drawText(labels[i], cell.toNearestInt(), juce::Justification::centred, false);
            if (i == nativeIndex)
            {
                g.setColour(t.valueText);
                const float dx = cell.getCentreX();
                const float dy = cell.getBottom() - 1.0f;
                g.fillEllipse(dx - 1.5f, dy - 1.0f, 3.0f, 3.0f);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto track = getLocalBounds().withTrimmedTop(kLabelH + 7).removeFromTop(kTrackH);
        if (!track.contains(e.getPosition()) || labels.isEmpty()) return;
        const int i = juce::jlimit(0, labels.size() - 1,
            (int) ((float) (e.x - track.getX()) / (float) track.getWidth() * (float) labels.size()));
        setIndex(i, juce::sendNotification);
    }

private:
    static constexpr int kLabelH = 18;
    static constexpr int kTrackH = 24;
    juce::StringArray labels;
    int index = 0;
};

// ── HistRangeSlider (TEMPO-style histogram + dual thumbs) ─────────────────────

class HistRangeSlider : public juce::Component, public juce::SettableTooltipClient
{
public:
    /** Sidebar filter: match Include Subfolders / Favorites label styling. */
    bool sidebarLabelStyle = false;
    /** Header toggles the histogram. Effects inspector leaves this off. */
    bool collapsible = false;
    bool collapsed = true;
    std::function<void()> onCollapsedChanged;

    HistRangeSlider(const juce::String& l, int mn, int mx)
        : label(l), minV(mn), maxV(mx), lo(mn), hi(mx), defLo(mn), defHi(mx)
    {
        setWantsKeyboardFocus(true);
        setRepaintsOnMouseActivity(true);
        bins.assign(16, 0);
    }

    std::function<void(int, int)> onChange;

    /** Visual histogram bins spanning [minV, maxV] evenly (any length). */
    void setHistogram(const std::vector<int>& counts)
    {
        bins = counts;
        if (bins.empty())
            bins.assign(16, 0);
        repaint();
    }

    /** Build ~`numBins` bars from integer samples in [minV, maxV]. */
    void setHistogramFromSamples(const std::vector<int>& samples, int numBins = 16)
    {
        numBins = juce::jlimit(4, 48, numBins);
        bins.assign((size_t) numBins, 0);
        const float span = (float) juce::jmax(1, maxV - minV);
        for (int s : samples)
        {
            const int v = juce::jlimit(minV, maxV, s);
            int idx = (int) ((float) (v - minV) / span * (float) numBins);
            idx = juce::jlimit(0, numBins - 1, idx);
            bins[(size_t) idx] += 1;
        }
        repaint();
    }

    void setRange(int newLo, int newHi, juce::NotificationType notify = juce::sendNotification)
    {
        newLo = juce::jlimit(minV, maxV, newLo);
        newHi = juce::jlimit(minV, maxV, newHi);
        if (newHi < newLo) std::swap(newLo, newHi);
        if (newLo == lo && newHi == hi) return;
        lo = newLo;
        hi = newHi;
        repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange(lo, hi);
    }

    int getLo() const { return lo; }
    int getHi() const { return hi; }
    bool isFullRange() const { return lo <= minV && hi >= maxV; }

    int idealHeight() const
    {
        if (collapsible && collapsed)
            return headerStrip();
        return headerStrip() + toolkitRowGap() + kHistH + 4 + kTrackH + 8;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (collapsible && e.y < headerStrip())
        {
            collapsed = !collapsed;
            dragThumb = Thumb::None;
            repaint();
            if (onCollapsedChanged)
                onCollapsedChanged();
            return;
        }
        if (collapsible && collapsed)
            return;
        if (!hasKeyboardFocus(true)) grabKeyboardFocus();
        dragThumb = hitThumb(e.position.x);
        setFromX(e.position.x);
    }
    void mouseDrag(const juce::MouseEvent& e) override { setFromX(e.position.x); }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (collapsible && e.y < headerStrip())
            return;
        setRange(defLo, defHi);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto r = getLocalBounds();
        auto header = r.removeFromTop(headerStrip());
        const juce::String labelDraw = sidebarLabelStyle ? label.toUpperCase() : label;
        if (sidebarLabelStyle && collapsible)
        {
            g.setColour(ds::surface100());
            g.fillRect(header);
            g.setColour(ds::line());
            g.fillRect(header.getX(), header.getY(), header.getWidth(), 1);
            g.fillRect(header.getX(), header.getBottom() - 1, header.getWidth(), 1);
        }
        if (sidebarLabelStyle)
        {
            g.setFont(sectionTitleFont());
            g.setColour(ds::ink());
        }
        else
        {
            g.setFont(inspectorFont());
            g.setColour(t.rowLabel);
        }
        if (collapsible)
        {
            const int titleW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(
                g.getCurrentFont(), labelDraw));
            if (sidebarLabelStyle)
            {
                auto chev = header.removeFromLeft(16).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
                drawStrokedChevron(g, chev, ds::inkMuted(), !collapsed);
                header.removeFromLeft(2);
            }
            auto titleR = header.removeFromLeft(juce::jmin(titleW, juce::jmax(0, header.getWidth() - 48)));
            g.drawText(labelDraw, titleR, juce::Justification::centredLeft, true);
            if (!sidebarLabelStyle)
            {
                header.removeFromLeft(4);
                auto chev = header.removeFromLeft(12).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
                drawStrokedChevron(g, chev, t.valueText, !collapsed);
            }
        }
        else
        {
            g.drawText(labelDraw, header.removeFromLeft(header.getWidth() / 2),
                       juce::Justification::centredLeft, true);
        }
        g.setColour(isFullRange() ? ds::inkMuted() : ds::ink());
        g.setFont(sidebarLabelStyle ? monoFontFixed(12.0f) : inspectorFont());
        const juce::String val = isFullRange() ? "Any"
            : (juce::String(lo) + juce::String::charToString((juce::juce_wchar) 0x2013) + juce::String(hi));
        g.drawText(val, header, juce::Justification::centredRight, true);

        if (collapsible && collapsed)
            return;

        r.removeFromTop(toolkitRowGap());
        // Fixed hist + track so the 14px thumbs stay inside the component.
        auto hist = r.removeFromTop(kHistH).toFloat();
        const int n = juce::jmax(1, (int) bins.size());
        int peak = 1;
        for (int c : bins) peak = juce::jmax(peak, c);
        const float gap = 1.5f;
        const float barW = juce::jmax(2.0f, (hist.getWidth() - gap * (float) (n - 1)) / (float) n);
        const float span = (float) juce::jmax(1, maxV - minV);
        for (int i = 0; i < n; ++i)
        {
            const int binLo = minV + (int) std::floor((float) i / (float) n * span);
            const int binHi = minV + (int) std::floor((float) (i + 1) / (float) n * span) - 1;
            const bool inRange = binHi >= lo && binLo <= hi;
            const float h = hist.getHeight() * ((float) bins[(size_t) i] / (float) peak);
            auto bar = juce::Rectangle<float>(
                hist.getX() + (float) i * (barW + gap),
                hist.getBottom() - juce::jmax(2.0f, h),
                barW, juce::jmax(2.0f, h));
            if (bins[(size_t) i] <= 0)
            {
                // Empty bin: a 1px hairline so the range still reads.
                g.setColour(ds::surface300());
                g.fillRect(bar.withTop(hist.getBottom() - 1.0f));
            }
            else
            {
                // In-range bars = primary; out-of-range = text @ 18 %.
                g.setColour(inRange ? ds::primary() : ds::ink().withAlpha(0.18f));
                g.fillRoundedRectangle(bar.withTrimmedBottom(-0.01f), 2.0f);
            }
        }

        r.removeFromTop(4);
        constexpr float kThumbD = 12.0f;
        const float thumbInset = kThumbD * 0.5f;
        auto lane = r.removeFromTop(kTrackH).toFloat();
        auto track = lane.reduced(thumbInset, 0.0f).withSizeKeepingCentre(lane.getWidth() - thumbInset * 2.0f, 2.0f);
        g.setColour(ds::ink().withAlpha(0.18f));
        g.fillRect(track);
        const float trackSpan = (float) juce::jmax(1, maxV - minV);
        const float xLo = track.getX() + track.getWidth() * ((float) (lo - minV) / trackSpan);
        const float xHi = track.getX() + track.getWidth() * ((float) (hi - minV) / trackSpan);
        auto sel = juce::Rectangle<float>::leftTopRightBottom(xLo, track.getY(), xHi, track.getBottom());
        g.setColour(ds::primary());
        g.fillRect(sel);
        // Levels dual thumbs: 12 px surface discs with a 2 px primary ring.
        auto thumbAt = [&](float x)
        {
            return juce::Rectangle<float>(kThumbD, kThumbD).withCentre({ x, lane.getCentreY() });
        };
        ds::shadow::thumb(g, thumbAt(xLo));
        ds::shadow::thumb(g, thumbAt(xHi));
    }

private:
    static constexpr int kLabelH = 18;
    static constexpr int kHistH = 28;
    static constexpr int kTrackH = 12;

    int headerStrip() const
    {
        if (sidebarLabelStyle && collapsible)
            return metrics::scaled(28); // FORM-style fold header
        return collapsible ? 16 : kLabelH;
    }
    enum class Thumb { None, Lo, Hi };
    Thumb dragThumb = Thumb::None;

    Thumb hitThumb(float x) const
    {
        constexpr float kInset = 7.0f;
        const float span = (float) juce::jmax(1, maxV - minV);
        const float trackW = juce::jmax(1.0f, (float) getWidth() - kInset * 2.0f);
        const float xLo = kInset + trackW * ((float) (lo - minV) / span);
        const float xHi = kInset + trackW * ((float) (hi - minV) / span);
        return std::abs(x - xLo) <= std::abs(x - xHi) ? Thumb::Lo : Thumb::Hi;
    }

    void setFromX(float x)
    {
        constexpr float kInset = 7.0f;
        const float trackW = juce::jmax(1.0f, (float) getWidth() - kInset * 2.0f);
        if (dragThumb == Thumb::None)
            return;
        const float t = juce::jlimit(0.0f, 1.0f, (x - kInset) / trackW);
        const int v = minV + (int) std::lround(t * (float) (maxV - minV));
        if (dragThumb == Thumb::Lo) setRange(juce::jmin(v, hi), hi);
        else setRange(lo, juce::jmax(v, lo));
    }

    juce::String label;
    int minV, maxV, lo, hi, defLo, defHi;
    std::vector<int> bins;
};

// ── InlineRow (title left · control right-aligned at kSelectW) ───────────────

class InlineRow : public juce::Component
{
public:
    InlineRow(const juce::String& l, juce::Component& c) : control(c), label(l)
    {
        addAndMakeVisible(control);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        // Caps B2: always fixed select width, right-aligned.
        // Fit control height to the row so switches/pills never clip.
        const int useW = controlW > 0 ? controlW : kSelectW;
        const int useH = juce::jmin(kControlH, juce::jmax(12, r.getHeight()));
        auto ctrl = r.removeFromRight(useW);
        control.setBounds(ctrl.withSizeKeepingCentre(useW, useH));
        labelBounds = r.withTrimmedRight(8);
    }

    void setControlWidth(int w)
    {
        controlW = w > 0 ? w : kSelectW;
        resized();
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText(label, labelBounds, juce::Justification::centredLeft, true);
    }

    juce::Component& control;
    int controlW = 0;

private:
    juce::String label;
    juce::Rectangle<int> labelBounds;
};

// ── KeyModeRow ───────────────────────────────────────────────────────────────

class KeyModeRow : public juce::Component
{
public:
    KeyModeRow(FlatPopup& key, FlatPopup& mode) : keyPopup(key), modePopup(mode)
    {
        addAndMakeVisible(keyPopup);
        addAndMakeVisible(modePopup);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        modePopup.setBounds(r.removeFromRight(kSelectW).withSizeKeepingCentre(kSelectW, kControlH));
        r.removeFromRight(6);
        keyPopup.setBounds(r.removeFromRight(kSelectW).withSizeKeepingCentre(kSelectW, kControlH));
        labelBounds = r.withTrimmedRight(8);
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText("Key", labelBounds, juce::Justification::centredLeft, true);
    }

    juce::Rectangle<int> labelBounds;

private:
    FlatPopup& keyPopup;
    FlatPopup& modePopup;
};

// ── PitchRow (plain text + note tight against bare stepper) ──────────────────

class PitchRow : public juce::Component
{
public:
    FlatStepper stepper;
    juce::String annotation;
    juce::String label { "Pitch" };

    PitchRow()
    {
        stepper.minV = -12;
        stepper.maxV = 12;
        stepper.bare = false;
        stepper.format = [](int v) { return signedIntText(v); };
        addAndMakeVisible(stepper);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int stepW = stepper.idealWidth();
        stepper.setBounds(r.removeFromRight(stepW).withSizeKeepingCentre(stepW, kControlH));
        r.removeFromRight(8);
        r.removeFromLeft(kLabelColW);
        annotBounds = r;
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText(label, getLocalBounds().withWidth(kLabelColW),
                   juce::Justification::centredLeft, true);

        g.setFont(uiFontFixed(kAnnotPt, false));
        g.setColour(inspectorTokens().valueText);
        g.drawFittedText(annotation, annotBounds, juce::Justification::centredRight, 1);
    }

    juce::Rectangle<int> annotBounds;
};

// ── NoteFilterBlock (Filter label + 1-oct keyboard; type is a separate row) ──

class NoteFilterBlock : public juce::Component
{
public:
    NoteFilterBlock()
    {
        setWantsKeyboardFocus(false);
    }

    std::function<void()> onChange;

    void setState(uint16_t mask, juce::NotificationType notify)
    {
        mask = (uint16_t) (mask & 0x0FFF);
        if (mask == 0) mask = 0x0FFF;
        const bool changed = mask != noteFilterMask;
        noteFilterMask = mask;
        if (changed) repaint();
        if (notify != juce::dontSendNotification && onChange)
            onChange();
    }

    void setScaleContext(int rootPc, Mode mode)
    {
        scaleRoot = rootPc;
        scaleMode = mode;
        repaint();
    }

    uint16_t getMask() const { return noteFilterMask; }

    juce::String legendLead() const
    {
        const char* words[] = { "ionian", "dorian", "phrygian", "lydian",
                                "mixolydian", "aeolian", "locrian" };
        const int mi = juce::jlimit(0, 6, (int) scaleMode);
        const int pc = juce::jlimit(0, 11, scaleRoot >= 0 ? scaleRoot : 0);
        return "In " + juce::String(kNoteNames[(size_t) pc]) + " " + words[mi];
    }

    int idealHeight() const
    {
        return 18 + 7 + kKeysH + 8 + 12;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop(18 + 7);
        keysBounds = r.removeFromTop(kKeysH);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!keysBounds.contains(e.getPosition()))
            return;
        const int pc = hitPitchClass(e.x - keysBounds.getX(), e.y - keysBounds.getY(),
                                     keysBounds.getWidth(), keysBounds.getHeight());
        if (pc < 0) return;
        const bool on = (noteFilterMask & (uint16_t) (1u << pc)) != 0;
        uint16_t next = noteFilterMask;
        if (on)
        {
            next = (uint16_t) (next & (uint16_t) ~(1u << pc) & 0x0FFF);
            if (next == 0) return; // keep at least one
        }
        else
        {
            next = (uint16_t) ((next | (uint16_t) (1u << pc)) & 0x0FFF);
        }
        noteFilterMask = next;
        repaint(keysBounds);
        if (onChange) onChange();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = inspectorTokens();
        auto header = getLocalBounds().removeFromTop(18);
        const int onCount = [=]
        {
            int n = 0;
            for (int i = 0; i < 12; ++i)
                if ((noteFilterMask & (uint16_t) (1u << i)) != 0) ++n;
            return n;
        }();
        const bool allOn = onCount >= 12;
        g.setFont(inspectorFont());
        g.setColour(t.rowLabel);
        g.drawText("Note filter", header, juce::Justification::centredLeft, true);
        g.setFont(inspectorMono());
        g.setColour(allOn ? t.valueText : ds::ink());
        g.drawText(allOn ? "All notes" : (juce::String(onCount) + " of 12"),
                   header, juce::Justification::centredRight, true);

        paintKeyboard(g, keysBounds.toFloat());
        paintLegend(g);
    }

private:
    static constexpr int kLabelH = 18;
    static constexpr int kKeysH = 46;
    /**
     * Three clearly separated states (ds tokens only):
     * 1) on + in scale  — kw/kb + accent wash
     * 2) on + out of scale — tx3 / trk
     * 3) off — panel/ctl + slash
     */
    // Levels: in key = primary, outside = ink 18% (opaque over surface so
    // overlapping black keys stay solid), muted = surface + slash.
    static juce::Colour activeWhite()   { return ds::primary(); }
    static juce::Colour activeBlack()   { return ds::primary(); }
    static juce::Colour outScaleWhite() { return ds::surface200().overlaidWith(ds::ink().withAlpha(0.18f)); }
    static juce::Colour outScaleBlack() { return ds::surface200().overlaidWith(ds::ink().withAlpha(0.30f)); }
    static juce::Colour offWhiteKey()   { return ds::surface200(); }
    static juce::Colour offBlackKey()   { return ds::surface300(); }

    static void paintOffSlash(juce::Graphics& g, juce::Rectangle<float> key)
    {
        g.setColour(ds::inkMuted());
        const float inset = 4.0f;
        g.drawLine(key.getX() + inset, key.getBottom() - inset,
                   key.getRight() - inset, key.getY() + inset, 1.4f);
    }

    static void fillKey(juce::Graphics& g, juce::Rectangle<float> key, bool white)
    {
        if (!white)
        {
            g.fillRoundedRectangle(key, 2.0f);
            return;
        }
        juce::Path p;
        p.addRoundedRectangle(key.getX(), key.getY(), key.getWidth(), key.getHeight(),
                              4.0f, 4.0f, false, false, true, true);
        g.fillPath(p);
    }

    static void strokeKey(juce::Graphics& g, juce::Rectangle<float> key, bool white, juce::Colour c)
    {
        g.setColour(c);
        if (!white)
        {
            g.drawRoundedRectangle(key.reduced(0.5f), 2.0f, 1.0f);
            return;
        }
        juce::Path p;
        p.addRoundedRectangle(key.getX() + 0.5f, key.getY() + 0.5f,
                              key.getWidth() - 1.0f, key.getHeight() - 1.0f,
                              4.0f, 4.0f, false, false, true, true);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }

    void paintLegend(juce::Graphics& g) const
    {
        auto row = getLocalBounds().toFloat();
        row = row.removeFromBottom(12.0f);
        const auto& t = inspectorTokens();
        struct Swatch { juce::Colour fill; juce::String name; bool slash; };
        const Swatch items[] = {
            { activeWhite(), legendLead(), false },
            { outScaleWhite(), "Outside", false },
            { offWhiteKey(), "Muted", true },
        };
        float x = row.getX();
        g.setFont(monoFontFixed(12.0f));
        for (const auto& item : items)
        {
            auto sw = juce::Rectangle<float>(x, row.getCentreY() - 5.0f, 10.0f, 10.0f);
            g.setColour(item.fill);
            g.fillRoundedRectangle(sw, 2.0f);
            g.setColour(ds::edge());
            g.drawRoundedRectangle(sw.reduced(0.5f), 2.0f, 1.0f);
            if (item.slash)
                paintOffSlash(g, sw);
            x += 12.0f;
            g.setColour(t.valueText);
            const int tw = (int) std::ceil(g.getCurrentFont().getStringWidthFloat(item.name));
            g.drawText(item.name, juce::Rectangle<int>((int) x, (int) row.getY(), tw + 2, 12),
                       juce::Justification::centredLeft, false);
            x += (float) tw + 12.0f;
        }
    }

    void paintKeyboard(juce::Graphics& g, juce::Rectangle<float> area) const
    {
        if (area.getWidth() < 8.0f || area.getHeight() < 8.0f)
            return;

        static constexpr int kWhitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr int kBlackPc[5] = { 1, 3, 6, 8, 10 };
        static constexpr int kBlackAfterWhite[5] = { 0, 1, 3, 4, 5 };

        const float gap = 1.0f;
        const float whiteW = (area.getWidth() - gap * 6.0f) / 7.0f;
        const float whiteH = area.getHeight();
        const float blackW = (area.getWidth() / 7.0f) * 0.62f;
        const float blackH = whiteH * 0.58f;

        auto enabled = [this](int pc)
        {
            return (noteFilterMask & (uint16_t) (1u << pc)) != 0;
        };
        auto inScale = [this](int pc)
        {
            if (scaleRoot < 0) return true;
            return pitchInScale(pc, scaleRoot, scaleMode);
        };

        for (int i = 0; i < 7; ++i)
        {
            const int pc = kWhitePc[i];
            auto key = juce::Rectangle<float>(area.getX() + (whiteW + gap) * (float) i,
                                              area.getY(), whiteW, whiteH);
            const bool on = enabled(pc);
            const bool scale = inScale(pc);
            g.setColour(!on ? offWhiteKey() : (scale ? activeWhite() : outScaleWhite()));
            fillKey(g, key, true);
            if (!on)
                paintOffSlash(g, key);
            const auto border = (on && scale) ? ds::primary() : ds::edge();
            strokeKey(g, key, true, border);
            g.setFont(monoFontFixed(10.0f));
            g.setColour(!on ? ds::inkMuted() : (scale ? ds::onPrimary() : ds::ink()));
            g.drawText(juce::String(kNoteNames[(size_t) pc]),
                       key.removeFromBottom(12.0f).toNearestInt(),
                       juce::Justification::centred, false);
        }

        for (int i = 0; i < 5; ++i)
        {
            const int pc = kBlackPc[i];
            const float slot = area.getWidth() / 7.0f;
            const float cx = area.getX() + slot * ((float) kBlackAfterWhite[i] + 1.0f);
            auto key = juce::Rectangle<float>(cx - blackW * 0.5f, area.getY(), blackW, blackH);
            const bool on = enabled(pc);
            const bool scale = inScale(pc);
            g.setColour(!on ? offBlackKey() : (scale ? activeBlack() : outScaleBlack()));
            fillKey(g, key, false);
            if (!on)
                paintOffSlash(g, key);
            // Black keys sit on the whites: a surface-coloured outline keeps
            // them separated from an in-key (primary) neighbour.
            const auto border = (on && scale) ? ds::surface200() : ds::edge();
            strokeKey(g, key, false, border);
        }
    }

    static int hitPitchClass(int x, int y, int width, int height)
    {
        if (width <= 0 || height <= 0) return -1;
        static constexpr int kWhitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr int kBlackPc[5] = { 1, 3, 6, 8, 10 };
        static constexpr int kBlackAfterWhite[5] = { 0, 1, 3, 4, 5 };
        const float whiteW = (float) width / 7.0f;
        const float blackW = whiteW * 0.62f;
        const float blackH = (float) height * 0.58f;
        if ((float) y <= blackH)
        {
            for (int i = 0; i < 5; ++i)
            {
                const float cx = whiteW * ((float) kBlackAfterWhite[i] + 1.0f);
                if (std::abs((float) x - cx) <= blackW * 0.5f)
                    return kBlackPc[i];
            }
        }
        const int wi = juce::jlimit(0, 6, (int) ((float) x / whiteW));
        return kWhitePc[wi];
    }

    uint16_t noteFilterMask = 0x0FFF;
    int scaleRoot = -1;
    Mode scaleMode = Mode::Ionian;
    juce::Rectangle<int> keysBounds;
};

// ── Divider (hairline inside an open section; hidden when folded) ────────────

class Divider : public juce::Component
{
public:
    int idealHeight() const { return 5; }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        r.removeFromTop(2);
        g.setColour(inspectorTokens().divider);
        g.fillRect(r.removeFromTop(1));
    }
};

// ── GridAmountBlock (popup row + inline amount slider, one dirty unit) ───────

class GridAmountBlock : public juce::Component
{
public:
    GridAmountBlock(const juce::String& headingIn, FlatPopup& popupIn, FlatSliderRow& amountIn)
        : heading(headingIn), popup(popupIn), amount(amountIn)
    {
        amount.subduedLabel = true;
        amount.style = FlatSliderRow::Style::Inline;
        addAndMakeVisible(popup);
        addAndMakeVisible(amount);
    }

    int idealHeight() const { return kRowMinH + 7 + kSliderRowH; }

    void setControlWidth(int w) { controlW = w > 0 ? w : kSelectW; resized(); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop(kRowMinH);
        const int popW = controlW > 0 ? controlW : juce::jmax(kSelectW, popup.idealWidth());
        popup.setBounds(head.removeFromRight(popW).withSizeKeepingCentre(popW, kControlH));
        labelBounds = head.withTrimmedRight(8);
        r.removeFromTop(7);
        amount.setBounds(r.removeFromTop(kSliderRowH));
    }

    void paint(juce::Graphics& g) override
    {
        g.setFont(inspectorFont());
        g.setColour(inspectorTokens().rowLabel);
        g.drawText(heading, labelBounds, juce::Justification::centredLeft, true);
    }

private:
    juce::String heading;
    FlatPopup& popup;
    FlatSliderRow& amount;
    juce::Rectangle<int> labelBounds;
    int controlW = 0;
};

// ── Section (fold chevron + large title + refresh + lock) ────────────────────

class Section : public juce::Component,
                private juce::Timer
{
public:
    explicit Section(const juce::String& t) : title(t) {}

    void addRow(juce::Component* c, int h = kRowMinH, std::function<bool()> dirtyFn = {})
    {
        rows.push_back({ c, h, {}, std::move(dirtyFn) });
        addChildComponent(c);
        refreshRowVisibility();
    }

    void addRow(juce::Component* c, std::function<int()> heightFn, std::function<bool()> dirtyFn = {})
    {
        rows.push_back({ c, kRowMinH, std::move(heightFn), std::move(dirtyFn) });
        addChildComponent(c);
        refreshRowVisibility();
    }

    void setOpen(bool o)
    {
        if (open == o) return;
        open = o;
        refreshRowVisibility();
        resized(); // lay out rows immediately so open never shows an empty shell
        if (onToggle) onToggle();
        repaint();
    }

    void setLocked(bool l)
    {
        if (locked == l) return;
        locked = l;
        repaint();
    }

    /** Hide a row in every fold state (passthrough controls that do not apply). */
    void setRowForcedHidden(juce::Component* c, bool hidden)
    {
        for (auto& r : rows)
        {
            if (r.comp != c) continue;
            if (r.forcedHidden == hidden) return;
            r.forcedHidden = hidden;
            refreshRowVisibility();
            return;
        }
    }

    bool isLocked() const { return locked; }
    bool isDragging() const { return dragging; }

    /** Re-evaluate dirty-row visibility (call after param changes). */
    void refreshDirtyRows()
    {
        refreshRowVisibility();
        resized();
        repaint();
    }

    int visibleRowCount() const
    {
        int n = 0;
        for (const auto& r : rows)
            if (isRowShowing(r)) ++n;
        return n;
    }

    /** Rows that declare a dirty predicate and are currently modified. */
    int modifiedCount() const
    {
        int n = 0;
        for (const auto& r : rows)
            if (!r.forcedHidden && r.dirty && r.dirty()) ++n;
        return n;
    }

    int idealHeight() const
    {
        return idealHeightForState(open);
    }

    /** Height if the section were forced open or closed (ignores current fold). */
    int idealHeightForState(bool forceOpen) const
    {
        const int rowGap = toolkitRowGap();
        int shown = 0;
        int content = 0;
        for (const auto& r : rows)
        {
            if (r.forcedHidden) continue;
            const bool show = forceOpen || (r.dirty && r.dirty());
            if (!show) continue;
            if (shown > 0) content += rowGap;
            content += r.height();
            ++shown;
        }
        if (shown == 0)
            return kSectionHeaderH;
        const int padB = forceOpen ? toolkitSectionPadB() : toolkitSectionPadBFolded();
        return kSectionHeaderH + toolkitTitleGap() + content + padB;
    }

    void resized() override
    {
        const int rowGap = toolkitRowGap();
        const bool any = visibleRowCount() > 0;
        const int padB = !any ? 0 : (open ? toolkitSectionPadB() : toolkitSectionPadBFolded());
        auto r = getLocalBounds();
        r.removeFromTop(kSectionHeaderH + toolkitTitleGap());
        r.removeFromBottom(padB);
        r.removeFromLeft(kContentPadX);
        r.removeFromRight(kContentPadR);
        bool first = true;
        for (auto& row : rows)
        {
            if (!isRowShowing(row))
            {
                row.comp->setBounds({});
                continue;
            }
            if (!first) r.removeFromTop(rowGap);
            first = false;
            row.comp->setBounds(r.removeFromTop(row.height()));
        }
    }

    // Header interaction: click the chevron/title to fold; click lock or reset;
    // hold then move to pick the section up and reorder it (hold alone still folds).
    void mouseDown(const juce::MouseEvent& e) override
    {
        pressInHeader = false;
        dragArmed = false;
        if (e.y > kSectionHeaderH) return;

        if (showHeaderChrome && lockBounds().contains(e.getPosition()) && onLockToggle)
        {
            onLockToggle();
            return;
        }
        if (showHeaderChrome && resetBounds().contains(e.getPosition()))
        {
            if (onReset && modifiedCount() > 0 && !locked) onReset();
            return;
        }
        if (headerHitBounds().contains(e.getPosition()))
        {
            pressInHeader = true;
            dragging = false;
            if (onDragMove) startTimer(kHoldMs);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!pressInHeader) return;
        if (dragging)
        {
            if (onDragMove) onDragMove(e.getScreenY());
            return;
        }
        // Reorder only after hold arming + a real move — pure click/hold still toggles fold.
        if (dragArmed && onDragMove && e.getDistanceFromDragStart() > 6)
        {
            dragging = true;
            dragArmed = false;
            stopTimer();
            repaint();
            if (onDragStart) onDragStart();
            onDragMove(e.getScreenY());
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        stopTimer();
        if (dragging)
        {
            dragging = false;
            repaint();
            if (onDragEnd) onDragEnd();
        }
        else if (pressInHeader)
        {
            setOpen(!open);
        }
        pressInHeader = false;
        dragArmed = false;
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const bool over = e.y <= kSectionHeaderH && e.y >= 0;
        if (over != headerHover)
        {
            headerHover = over;
            repaint();
        }
        setMouseCursor(over ? juce::MouseCursor::PointingHandCursor
                            : juce::MouseCursor::NormalCursor);
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (headerHover)
        {
            headerHover = false;
            repaint();
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void paint(juce::Graphics& g) override
    {
        // Levels: raised section face (surface100) on surface000 body well.
        const int mods = modifiedCount();
        auto b = getLocalBounds().toFloat();
        g.setColour(ds::surface100());
        g.fillRect(b);

        auto header = getLocalBounds().removeFromTop(kSectionHeaderH);
        if (headerHover || dragging)
        {
            g.setColour(ds::ink().withAlpha(dragging ? 0.07f : 0.04f));
            g.fillRect(dragging ? getLocalBounds() : header);
        }
        auto chev = chevronBounds().toFloat().withSizeKeepingCentre(10.0f, 10.0f);
        drawStrokedChevron(g, chev, ds::inkMuted(), open);

        auto titleR = header.withTrimmedLeft(chevronBounds().getRight() + 2);
        if (showHeaderChrome)
            titleR.setRight(resetBounds().getX() - 4);
        else
            titleR.removeFromRight(4);
        g.setFont(sectionTitleFont());
        g.setColour(ds::ink());
        const auto titleCaps = title.toUpperCase();
        const int titleW = (int) std::ceil(juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), titleCaps)) + 2;
        g.drawText(titleCaps, titleR.removeFromLeft(juce::jmin(titleW, titleR.getWidth())),
                   juce::Justification::centredLeft, true);

        if (showHeaderChrome && mods > 0)
        {
            // Count-only badge: violet 12% tag, ink mono digits (Levels).
            titleR.removeFromLeft(8);
            const auto txt = juce::String(mods);
            const auto bf = monoFontFixed(12.0f, true);
            const float bw = juce::jmax(20.0f, juce::GlyphArrangement::getStringWidth(bf, txt) + 12.0f);
            // Levels: the count sits at the right of the header, before reset/lock.
            const float rightEdge = showHeaderChrome ? (float) resetBounds().getX() - 6.0f
                                                     : (float) getWidth() - 12.0f;
            auto badge = juce::Rectangle<float>(rightEdge - bw, (float) titleR.getCentreY() - 10.0f, bw, 20.0f);
            if (badge.getX() >= (float) titleR.getX())
            {
                g.setColour(ds::acc().withAlpha(ds::dark() ? 0.22f : 0.12f));
                g.fillRoundedRectangle(badge, ds::radius::sm);
                g.setFont(bf);
                g.setColour(ds::ink());
                g.drawText(txt, badge.toNearestInt(), juce::Justification::centred, false);
            }
        }

        if (showHeaderChrome)
        {
            // Reset: live when this section has changes and is not locked.
            const bool canReset = mods > 0 && !locked;
            drawIcon(g, icons::undo, resetBounds().toFloat().withSizeKeepingCentre(10.0f, 10.0f),
                     canReset ? ds::ink() : ds::inkFaint(), 1.3f);

            // Lock: a control-sm toggle; lit (accent) while locked.
            auto lock = lockBounds().toFloat().withSizeKeepingCentre(16.0f, 14.0f);
            if (locked)
            {
                ds::drawActive(g, lock, ds::radius::sm);
            }
            // Unlocked: bare icon (Levels ghost icon button).
            drawIcon(g, locked ? icons::lockClosed : icons::lockOpen, lock.withSizeKeepingCentre(9.0f, 9.0f),
                     locked ? ds::acc() : ds::inkMuted(), 1.2f);
        }

        g.setColour(dragging ? ds::acc() : ds::line());
        g.fillRect(0, getHeight() - 1, getWidth(), 1);
    }

    juce::String title;
    juce::String sectionId;
    /** Folded by default; when folded, dirty (changed) rows stay visible. */
    bool open = false;
    bool locked = false;
    /** When false, header omits lock / reset / "X changed" (generator sections). */
    bool showHeaderChrome = true;
    bool showBottomDivider = false;   // legacy; frames replace dividers
    std::function<void()> onToggle;
    std::function<void()> onReset;
    std::function<void()> onLockToggle;
    /** Hold-to-reorder: fired once the hold completes, while dragging (screen Y), and on release. */
    std::function<void()> onDragStart;
    std::function<void(int)> onDragMove;
    std::function<void()> onDragEnd;

    struct Row
    {
        juce::Component* comp = nullptr;
        int h = kRowMinH;
        std::function<int()> heightFn;
        std::function<bool()> dirty;
        bool forcedHidden = false;
        int height() const { return heightFn ? juce::jmax(1, heightFn()) : h; }
    };
    std::vector<Row> rows;

private:
    static constexpr int kHoldMs = 380;

    void timerCallback() override
    {
        stopTimer();
        if (!pressInHeader || dragging) return;
        // Arm reorder; actual drag starts on mouse movement so a long press still folds.
        dragArmed = true;
    }

    juce::Rectangle<int> lockBounds() const
    {
        auto h = getLocalBounds().removeFromTop(kSectionHeaderH);
        h.removeFromRight(8);
        return h.removeFromRight(20);
    }

    juce::Rectangle<int> resetBounds() const
    {
        auto h = getLocalBounds().removeFromTop(kSectionHeaderH);
        h.removeFromRight(8 + 20 + 4);
        return h.removeFromRight(20);
    }

    juce::Rectangle<int> chevronBounds() const
    {
        return { 8, 0, 16, kSectionHeaderH };
    }

    /** Whole header except the reset and lock cells (when chrome is shown). */
    juce::Rectangle<int> headerHitBounds() const
    {
        auto h = getLocalBounds().removeFromTop(kSectionHeaderH);
        if (showHeaderChrome)
            h.removeFromRight(4 + 20 + 20);
        return h;
    }

    bool headerHover = false;
    bool pressInHeader = false;
    bool dragging = false;
    bool dragArmed = false; // hold completed; waiting for move to start reorder

    bool isRowShowing(const Row& r) const
    {
        if (r.forcedHidden) return false;
        if (open) return true;
        return r.dirty && r.dirty();
    }

    void refreshRowVisibility()
    {
        for (auto& r : rows)
            r.comp->setVisible(isRowShowing(r));
    }
};
} // namespace fx
} // namespace pflow
