#include "TransportBar.h"

namespace pflow {

namespace {
constexpr float kHeaderIconScale = 1.0f;
constexpr int kBtnW = 34;
constexpr int kBtnH = 24;
}

// ── StatusPill ───────────────────────────────────────────────────────────────

void TransportBar::StatusPill::setText(const juce::String& t)
{
    if (text == t && !editing) return;
    text = t;
    repaint();
}

void TransportBar::StatusPill::paint(juce::Graphics& g)
{
    constexpr float kRadius = 5.0f;
    auto r = getLocalBounds().toFloat();
    g.setColour(ds::ctl().withMultipliedAlpha(foregroundAlpha > 0.9f ? 1.0f : 0.85f));
    g.fillRoundedRectangle(r, kRadius);
    g.setColour(ds::ctlb().withMultipliedAlpha(foregroundAlpha));
    g.drawRoundedRectangle(r.reduced(0.5f), kRadius, 1.0f);

    auto inner = r.reduced(12.0f, 0.0f);
    const auto value = editing ? editBuffer + "|"
                               : text.upToFirstOccurrenceOf(" ", false, false);
    const auto valueFont = ds::font(ds::Type::NumericDisplay);
    g.setColour(ds::tx().withMultipliedAlpha(foregroundAlpha));
    g.setFont(valueFont);
    const float valueW = juce::GlyphArrangement::getStringWidth(valueFont, value) + 4.0f;
    g.drawText(value, inner.removeFromLeft(valueW), juce::Justification::centredLeft, false);
    g.setColour(ds::tx3().withMultipliedAlpha(foregroundAlpha));
    g.setFont(ds::font(ds::Type::Caption).withHeight(9.0f).withExtraKerningFactor(0.08f));
    g.drawText("BPM", inner, juce::Justification::centredLeft, false);
}

void TransportBar::StatusPill::mouseDown(const juce::MouseEvent&)
{
    if (editing || synced) return;
}

void TransportBar::StatusPill::mouseDoubleClick(const juce::MouseEvent&)
{
    if (synced) return;
    pendingSyncToggle = false;
    stopTimer();
    editing = true;
    editBuffer = text.upToFirstOccurrenceOf(" ", false, false);
    setWantsKeyboardFocus(true);
    grabKeyboardFocus();
    repaint();
}

void TransportBar::StatusPill::timerCallback()
{
    stopTimer();
    pendingSyncToggle = false;
}

void TransportBar::StatusPill::focusLost(FocusChangeType)
{
    if (!editing) return;
    editing = false;
    const double v = juce::jlimit(20.0, 300.0, editBuffer.getDoubleValue());
    if (onCommitBpm) onCommitBpm(std::round(v * 10.0) / 10.0);
    repaint();
}

bool TransportBar::StatusPill::keyPressed(const juce::KeyPress& key)
{
    if (!editing) return false;
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::escapeKey)
    {
        if (key == juce::KeyPress::returnKey)
        {
            const double v = juce::jlimit(20.0, 300.0, editBuffer.getDoubleValue());
            if (onCommitBpm) onCommitBpm(std::round(v * 10.0) / 10.0);
        }
        editing = false;
        repaint();
        return true;
    }
    if (key == juce::KeyPress::backspaceKey)
    {
        if (editBuffer.isNotEmpty())
            editBuffer = editBuffer.dropLastCharacters(1);
        repaint();
        return true;
    }
    const auto ch = key.getTextCharacter();
    if ((ch >= '0' && ch <= '9') || ch == '.')
    {
        editBuffer += juce::String::charToString(ch);
        repaint();
        return true;
    }
    return false;
}

// ── DragChip ─────────────────────────────────────────────────────────────────

int TransportBar::DragChip::idealWidth() const
{
    const float s = juce::jmax(0.5f, contentScale());
    const auto font = uiFont(11.5f, true);
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, "Drag to DAW", 0.0f, 0.0f);
    float right = 72.0f;
    if (glyphs.getNumGlyphs() > 0)
    {
        right = 0.0f;
        for (int i = 0; i < glyphs.getNumGlyphs(); ++i)
            right = juce::jmax(right, glyphs.getGlyph(i).getRight());
    }
    // Ink bounds stop short of the last glyph's advance, which was clipping the W.
    const int textW = (int) std::ceil(right) + 6;
    const int pad = juce::roundToInt(10.0f * s);
    const int icon = juce::roundToInt(16.0f * s);
    const int gap = juce::roundToInt(6.0f * s);
    return pad + icon + gap + textW + pad;
}

void TransportBar::DragChip::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto face = getLocalBounds().toFloat();
    juce::Colour fill = ds::ctl();
    if (down) fill = usesDarkAppearance() ? fill.brighter(0.06f) : fill.darker(0.06f);
    else if (over) fill = usesDarkAppearance() ? fill.brighter(0.03f) : fill.darker(0.03f);
    g.setColour(fill.withMultipliedAlpha(foregroundAlpha > 0.9f ? 1.0f : 0.85f));
    g.fillRoundedRectangle(face, 6.0f);
    g.setColour(ds::ctlb().withMultipliedAlpha(foregroundAlpha));
    g.drawRoundedRectangle(face.reduced(0.5f), 6.0f, 1.0f);

    const float s = juce::jmax(0.5f, contentScale());
    auto inner = face.reduced(10.0f * s, 0.0f);
    auto glyph = inner.removeFromLeft(16.0f * s).withSizeKeepingCentre(16.0f * s, 16.0f * s);
    drawIcon(g, icons::dragOut, glyph, ds::tx2().withMultipliedAlpha(foregroundAlpha),
             juce::jlimit(1.2f, 1.6f, 1.3f * s));
    inner.removeFromLeft(6.0f * s);
    const auto font = uiFont(11.5f, true);
    g.setColour(ds::tx2().withMultipliedAlpha(foregroundAlpha));
    const float baseline = inner.getCentreY() + (font.getAscent() - font.getDescent()) * 0.5f;
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, "Drag to DAW", inner.getX(), baseline);
    glyphs.draw(g);
}

// ── TransportBar ─────────────────────────────────────────────────────────────

TransportBar::TransportBar()
{
    auto prep = [](IconBtn& b, bool ghost)
    {
        b.ghost = ghost;
        b.iconScale = kHeaderIconScale;
        b.setWantsKeyboardFocus(true);
    };

    prep(btnPlay, true);
    btnPlay.setTooltip("Play / pause preview (Space)");
    btnPlay.onClick = [this] { if (onPlayPause) onPlayPause(); };
    addAndMakeVisible(btnPlay);

    prep(btnStop, true);
    btnStop.setTooltip("Stop preview");
    btnStop.onClick = [this] { if (onStop) onStop(); };
    addAndMakeVisible(btnStop);

    prep(btnRecord, true);
    btnRecord.activeTint = juce::Colour(0xffe5484d);
    btnRecord.setTooltip("Record MIDI input");
    btnRecord.onClick = [this] { if (onToggleRecord) onToggleRecord(); };
    addAndMakeVisible(btnRecord);

    statusPill.onCommitBpm = [this](double bpm)
    {
        freeBpm = bpm;
        refreshBpm();
        if (onFreeBpmChanged) onFreeBpmChanged(freeBpm);
    };
    addAndMakeVisible(statusPill);

    prep(btnSync, false);
    btnSync.setTooltip("Sync to host tempo");
    btnSync.onClick = [this]
    {
        synced = !synced;
        refreshBpm();
        resized();
        if (onSyncChanged) onSyncChanged(synced);
    };
    addAndMakeVisible(btnSync);

    btnDragToDaw.accentText = false;
    btnDragToDaw.cornerRadius = 6.0f;
    btnDragToDaw.setTooltip("Drag edited clip onto a DAW track (or right-click to copy)");
    btnDragToDaw.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    btnDragToDaw.onDragStart = [this] { if (onDragToDaw) onDragToDaw(); };
    btnDragToDaw.onCopyToFolder = [this] { if (onCopyToFolder) onCopyToFolder(); };
    addAndMakeVisible(btnDragToDaw);

    prep(btnPassthrough, false);
    btnPassthrough.caption = "Through";
    btnPassthrough.setTooltip("Through\nLive input through effects\nShortcut: P");
    btnPassthrough.onClick = [this] { if (onTogglePassthrough) onTogglePassthrough(); };
    addAndMakeVisible(btnPassthrough);

    prep(btnEditor, false);
    btnEditor.caption = "Piano Roll";
    btnEditor.setTooltip("Piano Roll\nShortcut: E");
    btnEditor.onClick = [this] { if (onToggleEditor) onToggleEditor(); };
    addAndMakeVisible(btnEditor);

    prep(btnEffects, false);
    btnEffects.caption = "Effects";
    btnEffects.setTooltip("Effects\nShortcut: F");
    btnEffects.onClick = [this] { if (onToggleEffects) onToggleEffects(); };
    addAndMakeVisible(btnEffects);

    setOpaque(true);
    refreshBpm();
    startTimerHz(8);
}

void TransportBar::setPlaying(bool p)
{
    if (playing == p) return;
    playing = p;
    btnPlay.active = playing;
    btnPlay.repaint();
}

void TransportBar::setRecording(bool on)
{
    btnRecord.active = on;
    btnRecord.repaint();
}

void TransportBar::setSynced(bool s, juce::NotificationType notify)
{
    synced = s;
    statusPill.synced = s;
    refreshBpm();
    resized();
    if (notify != juce::dontSendNotification && onSyncChanged)
        onSyncChanged(synced);
}

void TransportBar::setHostBpm(double b)
{
    if (std::abs(b - hostBpm) < 0.01) return;
    hostBpm = b;
    if (synced) refreshBpm();
}

void TransportBar::setBpmMultiplier(double m)
{
    multiplier = juce::jlimit(0.25, 4.0, m);
    if (synced) refreshBpm();
}

void TransportBar::setFreeBpm(double b)
{
    freeBpm = juce::jlimit(20.0, 300.0, b);
    if (!synced) refreshBpm();
}

void TransportBar::setClipBpm(double b)
{
    if (b > 1.0)
        freeBpm = juce::jlimit(20.0, 300.0, b);
    if (!synced) refreshBpm();
}

void TransportBar::setEditorOpen(bool open)
{
    editorOpen = open;
    btnEditor.active = open;
    btnEditor.repaint();
    resized();
}

void TransportBar::setEffectsOpen(bool open)
{
    effectsOpen = open;
    btnEffects.active = open;
    btnEffects.repaint();
    resized();
}

void TransportBar::setPassthrough(bool on)
{
    passthrough = on;
    btnPassthrough.active = on;
    const bool show = !on;
    btnPlay.setVisible(show);
    btnStop.setVisible(show);
    btnRecord.setVisible(show);
    statusPill.setVisible(show);
    btnSync.setVisible(show);
    btnEditor.setVisible(show);
    btnEffects.setVisible(show);
    btnDragToDaw.setVisible(show && hasClip);
    resized();
    repaint();
}

void TransportBar::setHasClip(bool has)
{
    hasClip = has;
    resized();
}

void TransportBar::setReserveTrafficLights(bool reserve)
{
    if (reserveTrafficLights == reserve) return;
    reserveTrafficLights = reserve;
    resized();
}

void TransportBar::setLightsZoneWidth(int px)
{
    measuredLightsZone = juce::jmax(0, px);
    resized();
    repaint();
}

namespace {

float glyphLineWidth(const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, text, 0.0f, 0.0f);
    if (glyphs.getNumGlyphs() <= 0)
        return 0.0f;
    return glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true).getWidth();
}

juce::Font toolkitWordFont()
{
    return uiFontFixed(14.0f, false).withExtraKerningFactor(-0.02f);
}

} // namespace

int TransportBar::logoIdealWidth() const
{
    const auto midi = ds::font(ds::Type::Logo);
    const float text = glyphLineWidth(midi, "MIDI")
                     + glyphLineWidth(midi, " ")
                     + glyphLineWidth(toolkitWordFont(), "Toolkit");
    return 16 + 6 + (int) std::ceil(text);
}

int TransportBar::logoMidiWidth() const
{
    const float text = glyphLineWidth(ds::font(ds::Type::Logo), "MIDI");
    return 16 + 6 + (int) std::ceil(text);
}

void TransportBar::paintLogoGlyph(juce::Graphics& g, juce::Point<float> origin, float alpha) const
{
    const float s = 16.0f / 54.0f;
    auto P = [&](float ux, float uy)
    {
        return origin + juce::Point<float>((ux - 4.0f) * s, (uy - 4.0f) * s);
    };
    const auto ink = ds::tx().withMultipliedAlpha(alpha);
    const auto fill = ds::lensFill().withMultipliedAlpha(alpha);
    const auto c = P(28.0f, 28.0f);
    const float r = 16.5f * s;

    juce::Path handle;
    handle.startNewSubPath(P(39.0f, 39.0f));
    handle.lineTo(P(52.0f, 52.0f));
    g.setColour(ink);
    g.strokePath(handle, juce::PathStrokeType(7.0f * s, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    g.setColour(fill);
    g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clip;
        const float cr = 15.0f * s;
        clip.addEllipse(c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
        g.reduceClipRegion(clip);
        const auto hi = P(17.0f, 0.0f);
        const auto lo = P(39.0f, 0.0f);
        juce::ColourGradient grad(ds::logoNoteHi().withMultipliedAlpha(alpha), hi.x, hi.y,
                                  ds::accent().withMultipliedAlpha(alpha), lo.x, lo.y, false);
        g.setGradientFill(grad);
        auto bar = [&](float ux, float uy, float uw)
        {
            const auto a = P(ux, uy);
            g.fillRoundedRectangle(a.x, a.y, uw * s, 5.0f * s, 2.0f * s);
        };
        bar(17.0f, 20.0f, 15.0f);
        bar(24.0f, 27.0f, 15.0f);
        bar(19.0f, 34.0f, 11.0f);
    }

    g.setColour(ink);
    g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 4.0f * s);
}

void TransportBar::paintLogo(juce::Graphics& g, juce::Rectangle<int> area, float alpha) const
{
    if (area.getWidth() < 16 || area.getHeight() <= 0)
        return;

    const float centreY = area.getCentreY();
    paintLogoGlyph(g, { (float) area.getX(), centreY - 8.0f }, alpha);
    if (logoLevel <= 0)
        return;

    const auto midiFont = ds::font(ds::Type::Logo);
    const auto toolFont = toolkitWordFont();
    float x = (float) area.getX() + 16.0f + 6.0f;
    auto drawWord = [&](const juce::Font& font, const juce::String& text, juce::Colour colour)
    {
        g.setFont(font);
        g.setColour(colour);
        const float baseline = centreY + (font.getAscent() - font.getDescent()) * 0.5f;
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, text, x, baseline);
        glyphs.draw(g);
        if (glyphs.getNumGlyphs() > 0)
            x += glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true).getWidth();
    };

    drawWord(midiFont, "MIDI", ds::tx().withMultipliedAlpha(alpha));
    if (logoLevel < 2)
        return;
    drawWord(midiFont, " ", ds::tx().withMultipliedAlpha(alpha));
    drawWord(toolFont, "Toolkit", ds::tx2().withMultipliedAlpha(alpha));
}

void TransportBar::setClipName(const juce::String& name)
{
    if (btnDragToDaw.clipName == name) return;
    btnDragToDaw.clipName = name;
    resized();
    btnDragToDaw.repaint();
}

void TransportBar::refreshBpm()
{
    const double shown = synced ? hostBpm * multiplier : freeBpm;
    statusPill.synced = synced;
    statusPill.editable = !synced;
    statusPill.setText(juce::String(shown, 1) + " bpm");
    btnSync.active = synced;
    btnSync.setTooltip(synced ? "Synced to host - click for free tempo"
                              : "Free tempo - click to sync to host");
    btnSync.repaint();
}

void TransportBar::timerCallback()
{
    const bool active = nativeChrome::isWindowKey(*this);
    if (active == windowActive) return;
    windowActive = active;
    const float a = windowActive ? 1.0f : 0.50f;
    statusPill.foregroundAlpha = a;
    btnDragToDaw.foregroundAlpha = a;
    btnPlay.setAlpha(a);
    btnStop.setAlpha(a);
    btnSync.setAlpha(a);
    btnPassthrough.setAlpha(a);
    btnEditor.setAlpha(a);
    btnEffects.setAlpha(a);
    repaint();
}

bool TransportBar::hitInteractive(juce::Point<int> p) const
{
    auto covers = [p](const juce::Component& c)
    {
        return c.isVisible() && c.getBounds().contains(p);
    };
    return covers(btnPlay) || covers(btnStop) || covers(btnRecord) || covers(statusPill) || covers(btnSync)
        || covers(btnDragToDaw) || covers(btnPassthrough) || covers(btnEditor)
        || covers(btnEffects);
}

void TransportBar::mouseDown(const juce::MouseEvent& e)
{
    if (hitInteractive(e.getPosition()))
        return;
#if JUCE_MAC
    if (nativeChrome::performWindowDrag(*this))
        return;
#endif
    if (auto* top = getTopLevelComponent())
        windowDragger.startDraggingComponent(top, e.getEventRelativeTo(top));
}

void TransportBar::mouseDrag(const juce::MouseEvent& e)
{
    if (hitInteractive(e.getMouseDownPosition()))
        return;
#if JUCE_MAC
    juce::ignoreUnused(e);
#else
    if (auto* top = getTopLevelComponent())
        windowDragger.dragComponent(top, e.getEventRelativeTo(top), nullptr);
#endif
}

void TransportBar::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (hitInteractive(e.getPosition()))
        return;
#if JUCE_MAC
    nativeChrome::zoomWindow(*this);
#else
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->maximiseButtonPressed();
#endif
}

void TransportBar::applyHeaderCaptions(bool withText)
{
    btnSync.caption = {};
    btnPassthrough.caption = withText ? "Through" : juce::String();
    btnEditor.caption = withText ? "Piano Roll" : juce::String();
    btnEffects.caption = withText ? "Effects" : juce::String();
}

void TransportBar::resized()
{
    const int h = getHeight();
    const float s = juce::jmax(0.5f, contentScale());
    const int btnW = juce::roundToInt((float) kBtnW * s);
    const int btnH = juce::jmin(h - 4, juce::roundToInt((float) kBtnH * s));
    const int y = (h - btnH) / 2;
    const int gap = juce::jmax(4, juce::roundToInt(6.0f * s));

    if (passthrough)
    {
        applyHeaderCaptions(true);
        btnPlay.setVisible(false);
        btnStop.setVisible(false);
        btnRecord.setVisible(false);
        statusPill.setVisible(false);
        btnSync.setVisible(false);
        btnEditor.setVisible(false);
        btnEffects.setVisible(false);
        btnDragToDaw.setVisible(false);
        transportGroupBounds = {};
        dividerBounds = {};

        if (reserveTrafficLights || measuredLightsZone > 0)
            lightsZoneW = juce::jmax(16, measuredLightsZone);
        else
            lightsZoneW = 16;

        const int fullW = logoIdealWidth();
        titleBounds = { lightsZoneW, 0, fullW, h };
        logoLevel = 2;

        int passW = btnPassthrough.captionWidth();
        if (lightsZoneW + fullW + 16 + passW + 8 > getWidth())
        {
            applyHeaderCaptions(false);
            passW = btnPassthrough.captionWidth();
        }
        btnPassthrough.setVisible(true);
        btnPassthrough.setBounds(getWidth() - 8 - passW, y, passW, btnH);
        return;
    }

    btnPlay.setVisible(true);
    btnStop.setVisible(true);
    btnRecord.setVisible(true);
    statusPill.setVisible(true);
    btnSync.setVisible(true);
    btnEditor.setVisible(true);
    btnEffects.setVisible(true);

    // Measured zoom-button edge already includes the 14 px gap. Fullscreen reports 0.
    if (reserveTrafficLights || measuredLightsZone > 0)
        lightsZoneW = juce::jmax(16, measuredLightsZone);
    else
        lightsZoneW = 16;

    const int toggleGap = juce::jmax(2, juce::roundToInt(4.0f * s));
    const int rightInset = juce::roundToInt(8.0f * s);
    const int groupW = btnW * 3;
    const int dragExtra = hasClip ? btnDragToDaw.idealWidth() + juce::roundToInt(8.0f * s) : 0;
    const int dividerGap = juce::roundToInt(17.0f * s);
    const int logoGap = juce::roundToInt(16.0f * s);
    const int fullW = logoIdealWidth();
    const int midiW = logoMidiWidth();
    const int glyphW = juce::roundToInt(16.0f * s);

    auto measureToggles = [&]()
    {
        return btnEditor.captionWidth() + toggleGap
             + btnEffects.captionWidth();
    };

    applyHeaderCaptions(true);
    int togglesW = measureToggles();
    int syncW = btnSync.captionWidth();
    int throughW = btnPassthrough.captionWidth();
    int pillW = juce::roundToInt(80.0f * s);
    auto centerWidth = [&]()
    {
        return groupW + gap + pillW + gap + syncW + gap + throughW;
    };
    int centerW = centerWidth();

    auto fits = [&](int title, int center)
    {
        return lightsZoneW + title + logoGap + center + dragExtra + dividerGap + togglesW + rightInset
            <= juce::jmax(0, getWidth());
    };

    if (!fits(fullW, centerW))
    {
        pillW = juce::roundToInt(64.0f * s);
        centerW = centerWidth();
    }

    int titleW = fullW;
    logoLevel = 2;
    if (!fits(fullW, centerW))
    {
        titleW = midiW;
        logoLevel = 1;
    }
    if (!fits(midiW, centerW))
    {
        titleW = glyphW;
        logoLevel = 0;
    }
    if (!fits(titleW, centerW))
    {
        // Labels drop after the wordmark has already stepped down, so the
        // logo level never grows back as the window gets narrower.
        applyHeaderCaptions(false);
        togglesW = measureToggles();
        syncW = btnSync.captionWidth();
        throughW = btnPassthrough.captionWidth();
        centerW = centerWidth();
    }

    titleBounds = { lightsZoneW, 0, titleW, h };

    const int editW = btnEditor.captionWidth();
    const int fxW = btnEffects.captionWidth();
    const int sideGap = juce::roundToInt(8.0f * s);

    int xRight = getWidth() - rightInset;
    btnEffects.setBounds(xRight - fxW, y, fxW, btnH);
    xRight -= fxW + toggleGap;
    btnEditor.setBounds(xRight - editW, y, editW, btnH);
    xRight -= editW + sideGap;
    dividerBounds = { xRight - 1, (h - juce::roundToInt(16.0f * s)) / 2, 1, juce::roundToInt(16.0f * s) };
    xRight -= 1 + sideGap;

    const int chipH = juce::jmin(h - 4, juce::roundToInt(26.0f * s));
    btnDragToDaw.setVisible(hasClip);
    if (hasClip)
    {
        const int chipW = btnDragToDaw.idealWidth();
        btnDragToDaw.setBounds(xRight - chipW, (h - chipH) / 2, chipW, chipH);
        xRight -= chipW + sideGap;
    }
    else
    {
        btnDragToDaw.setBounds({});
    }

    const int rightEdge = xRight;
    int centerX = titleBounds.getRight() + logoGap;
    const int room = rightEdge - centerX;
    if (room > centerW)
        centerX += (room - centerW) / 2;
    if (centerX + centerW > rightEdge)
        centerX = juce::jmax(lightsZoneW + 4, rightEdge - centerW);

    transportGroupBounds = { centerX, y, groupW, btnH };
    btnPlay.setBounds(transportGroupBounds.getX(), y, btnW, btnH);
    btnStop.setBounds(transportGroupBounds.getX() + btnW, y, btnW, btnH);
    btnRecord.setBounds(transportGroupBounds.getX() + btnW * 2, y, btnW, btnH);
    statusPill.setBounds(transportGroupBounds.getRight() + gap, y, pillW, btnH);
    btnSync.setBounds(statusPill.getRight() + gap, y, syncW, btnH);
    btnPassthrough.setVisible(true);
    btnPassthrough.setBounds(btnSync.getRight() + gap, y, throughW, btnH);
}

void TransportBar::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(ds::chrome());
    g.fillRect(b);
    g.setColour(ds::hl());
    g.fillRect(0.0f, (float) getHeight() - 1.0f, (float) getWidth(), 1.0f);

    const float fgAlpha = windowActive ? 1.0f : 0.50f;

    paintLogo(g, titleBounds, fgAlpha);

    // Grouped transport face (outer corners only — play/stop stay ghost)
    if (!transportGroupBounds.isEmpty())
    {
        auto face = transportGroupBounds.toFloat();
        g.setColour(ds::ctl().withMultipliedAlpha(windowActive ? 1.0f : 0.85f));
        g.fillRoundedRectangle(face, 5.0f);
        g.setColour(ds::ctlb().withMultipliedAlpha(fgAlpha));
        g.drawRoundedRectangle(face.reduced(0.5f), 5.0f, 1.0f);
        const float third = face.getWidth() / 3.0f;
        g.fillRect(face.getX() + third - 0.5f, face.getY() + 4.0f, 1.0f, face.getHeight() - 8.0f);
        g.fillRect(face.getX() + third * 2.0f - 0.5f, face.getY() + 4.0f, 1.0f, face.getHeight() - 8.0f);
    }

    if (dividerBounds.getWidth() > 0)
    {
        g.setColour(ds::hl().withMultipliedAlpha(fgAlpha));
        g.fillRect(dividerBounds.toFloat());
    }
}

} // namespace pflow
