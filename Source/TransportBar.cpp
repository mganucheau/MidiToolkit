#include "TransportBar.h"

namespace pflow {

namespace {
constexpr float kHeaderIconScale = 1.0f;
constexpr int kBtnW = 24;   // control-lg square
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
    // Groove position box: surface-300, edge border, radius-sm; readout-lg value + "BPM" label.
    auto r = getLocalBounds().toFloat();
    g.setColour(ds::surface300());
    g.fillRoundedRectangle(r, ds::radius::sm);
    g.setColour((editable && (isMouseOverOrDragging() || editing)) ? ds::acc() : ds::edge());
    g.drawRoundedRectangle(r.reduced(0.5f), ds::radius::sm, 1.0f);

    auto inner = r.reduced(6.0f, 0.0f);
    const auto value = editing ? editBuffer + "|"
                               : text.upToFirstOccurrenceOf(" ", false, false);
    const auto valueFont = ds::font(ds::Type::NumericDisplay);
    g.setColour(ds::ink().withMultipliedAlpha(foregroundAlpha));
    g.setFont(valueFont);
    const float valueW = juce::GlyphArrangement::getStringWidth(valueFont, value) + 4.0f;
    g.drawText(value, inner.removeFromLeft(valueW), juce::Justification::centredLeft, false);
    g.setColour(ds::inkMuted().withMultipliedAlpha(foregroundAlpha));
    g.setFont(uiFontFixed(11.0f, false));
    g.drawText("BPM", inner, juce::Justification::centredLeft, false);
    if (hasKeyboardFocus(false))
        drawFocusRing(g, r, ds::radius::sm);
}

int TransportBar::StatusPill::idealWidth() const
{
    const auto value = text.upToFirstOccurrenceOf(" ", false, false);
    const float v = juce::GlyphArrangement::getStringWidth(ds::font(ds::Type::NumericDisplay),
                                                           value.length() < 3 ? juce::String("000") : value);
    const float l = juce::GlyphArrangement::getStringWidth(uiFontFixed(11.0f, false), "BPM");
    return (int) std::ceil(6.0f + v + 4.0f + l + 8.0f);
}

void TransportBar::StatusPill::mouseDrag(const juce::MouseEvent& e)
{
    // Drag vertically like a NumberBox: 2 px per BPM, Shift for ten times finer.
    if (synced || editing || !onCommitBpm) return;
    const float perBpm = e.mods.isShiftDown() ? 20.0f : 2.0f;
    const double v = juce::jlimit(20.0, 300.0, dragStartBpm - (double) e.getDistanceFromDragStartY() / perBpm);
    const double rounded = e.mods.isShiftDown() ? std::round(v * 10.0) / 10.0 : std::round(v);
    if (std::abs(rounded - value) > 1.0e-6)
        onCommitBpm(rounded);
}

void TransportBar::StatusPill::mouseEnter(const juce::MouseEvent&) { repaint(); }
void TransportBar::StatusPill::mouseExit(const juce::MouseEvent&) { repaint(); }

void TransportBar::StatusPill::mouseDown(const juce::MouseEvent&)
{
    if (editing || synced) return;
    dragStartBpm = value;
    setWantsKeyboardFocus(true);
    grabKeyboardFocus();
}

void TransportBar::StatusPill::mouseDoubleClick(const juce::MouseEvent&)
{
    beginEditing();
}

void TransportBar::StatusPill::beginEditing()
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
    if (!editing)
    {
        if (synced || !onCommitBpm) return false;
        double d = 0.0;
        if (key == juce::KeyPress::upKey)            d = key.getModifiers().isShiftDown() ? 0.1 : 1.0;
        else if (key == juce::KeyPress::downKey)     d = key.getModifiers().isShiftDown() ? -0.1 : -1.0;
        else if (key == juce::KeyPress::pageUpKey)   d = 10.0;
        else if (key == juce::KeyPress::pageDownKey) d = -10.0;
        else if (key == juce::KeyPress::returnKey)   { beginEditing(); return true; }
        else return false;
        onCommitBpm(juce::jlimit(20.0, 300.0, value + d));
        return true;
    }
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
    const float sc = juce::jmax(0.5f, contentScale());
    const int textW = (int) std::ceil(uiFont(11.0f, true).getStringWidthFloat("Drag to DAW")) + 2;
    return juce::roundToInt(8.0f * sc) * 2 + juce::roundToInt(10.0f * sc) + juce::roundToInt(4.0f * sc) + textW;
}

void TransportBar::DragChip::paintButton(juce::Graphics& g, bool over, bool down)
{
    auto face = getLocalBounds().toFloat();
    g.setColour((over || down) ? ds::surface300() : ds::surface200());
    g.fillRoundedRectangle(face, ds::radius::sm);
    g.setColour(ds::edge());
    g.drawRoundedRectangle(face.reduced(0.5f), ds::radius::sm, 1.0f);

    const float sc = juce::jmax(0.5f, contentScale());
    auto inner = face.reduced(8.0f * sc, 0.0f);
    auto glyph = inner.removeFromLeft(10.0f * sc).withSizeKeepingCentre(10.0f * sc, 10.0f * sc);
    drawIcon(g, icons::dragOut, glyph, ds::ink().withMultipliedAlpha(foregroundAlpha), 1.3f);
    inner.removeFromLeft(4.0f * sc);
    g.setFont(uiFont(11.0f, true));
    g.setColour(ds::ink().withMultipliedAlpha(foregroundAlpha));
    g.drawText("Drag to DAW", inner, juce::Justification::centredLeft, false);
    if (hasKeyboardFocus(true))
        drawFocusRing(g, face, ds::radius::sm);
}

// ── TransportBar ─────────────────────────────────────────────────────────────

TransportBar::TransportBar()
{
    auto prep = [](IconBtn& b)
    {
        b.ghost = false;            // every header control is a Groove toggle face
        b.iconScale = kHeaderIconScale;
        b.setWantsKeyboardFocus(true);
    };

    // Play and stop share one button: play glyph while stopped, stop glyph while playing.
    prep(btnPlay);
    btnPlay.activeTint = ds::play();
    btnPlay.setTooltip("Play preview (Space)");
    btnPlay.onClick = [this]
    {
        if (playing) { if (onStop) onStop(); }
        else if (onPlayPause) onPlayPause();
    };
    addAndMakeVisible(btnPlay);

    btnStop.setVisible(false);      // folded into btnPlay
    addChildComponent(btnStop);

    prep(btnRecord);
    btnRecord.activeTint = ds::record();
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

    prep(btnSync);
    btnSync.onClick = [this]
    {
        synced = !synced;
        refreshBpm();
        resized();
        if (onSyncChanged) onSyncChanged(synced);
    };
    addAndMakeVisible(btnSync);

    prep(btnPassthrough);
    btnPassthrough.setTooltip("Through: live MIDI input runs through the Toolkit (P)");
    btnPassthrough.onClick = [this] { if (onTogglePassthrough) onTogglePassthrough(); };
    addAndMakeVisible(btnPassthrough);

    btnDragToDaw.accentText = false;
    btnDragToDaw.setTooltip("Drag edited clip onto a DAW track (or right-click to copy)");
    btnDragToDaw.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    btnDragToDaw.onDragStart = [this] { if (onDragToDaw) onDragToDaw(); };
    btnDragToDaw.onCopyToFolder = [this] { if (onCopyToFolder) onCopyToFolder(); };
    addAndMakeVisible(btnDragToDaw);

    prep(btnEditor);
    btnEditor.setTooltip("Piano Roll (E)");
    btnEditor.onClick = [this] { if (onToggleEditor) onToggleEditor(); };
    addAndMakeVisible(btnEditor);

    prep(btnEffects);
    btnEffects.setTooltip("Toolkit (F)");
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
    btnPlay.activeTint = ds::play();
    btnPlay.icon = playing ? icons::stop : icons::play;
    btnPlay.setTooltip(playing ? "Stop preview (Space)" : "Play preview (Space)");
    btnPlay.repaint();
}

void TransportBar::setRecording(bool on)
{
    btnRecord.activeTint = ds::record();
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
    return ds::font(ds::Type::Logo);
}

} // namespace

int TransportBar::logoIdealWidth() const
{
    const auto midi = ds::font(ds::Type::Logo);
    const float text = glyphLineWidth(midi, "MIDI")
                     + glyphLineWidth(midi, " ")
                     + glyphLineWidth(toolkitWordFont(), "Toolkit");
    return (int) std::ceil(text);
}

int TransportBar::logoMidiWidth() const
{
    return (int) std::ceil(glyphLineWidth(ds::font(ds::Type::Logo), "MIDI"));
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
    if (logoLevel <= 0 || area.getWidth() <= 0 || area.getHeight() <= 0)
        return;

    const float centreY = area.getCentreY();
    const auto midiFont = ds::font(ds::Type::Logo);
    const auto toolFont = toolkitWordFont();
    float x = (float) area.getX();
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

    drawWord(midiFont, "MIDI", ds::ink().withMultipliedAlpha(alpha));
    if (logoLevel < 2)
        return;
    drawWord(midiFont, " ", ds::ink().withMultipliedAlpha(alpha));
    drawWord(toolFont, "Toolkit", ds::inkMuted().withMultipliedAlpha(alpha));
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
    statusPill.value = shown;
    // Whole numbers unless the tempo really has a fraction (set with Shift).
    const bool whole = std::abs(shown - std::round(shown)) < 0.05;
    statusPill.setText((whole ? juce::String(juce::roundToInt(shown)) : juce::String(shown, 1)) + " bpm");
    statusPill.setMouseCursor(synced ? juce::MouseCursor::NormalCursor : juce::MouseCursor::UpDownResizeCursor);
    statusPill.setTooltip(synced ? "Host tempo. Turn off Sync to set your own"
                                 : "Drag or use the arrow keys to set the tempo. Double-click to type");
    btnSync.active = synced;
    btnSync.setTooltip(synced ? "Synced to host tempo" : "Free tempo. Click to sync to host");
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
    // Sync and Through are icon toggles; the panel toggles carry words when there is room.
    btnSync.caption = {};
    btnPassthrough.caption = {};
    btnEditor.caption = withText ? "Piano Roll" : juce::String();
    btnEffects.caption = withText ? "Toolkit" : juce::String();
}

void TransportBar::resized()
{
    // [lights] Wordmark · BPM · Play/Stop · Rec · Sync · Through ······ Drag · Piano Roll · Toolkit
    const int h = getHeight();
    const float sc = juce::jmax(0.5f, contentScale());
    const int btn = juce::jmin(h - 8, juce::roundToInt((float) kBtnH * sc));
    const int y = (h - btn) / 2;
    const int tight = juce::roundToInt(2.0f * sc);     // within a group
    const int groupGap = juce::roundToInt(8.0f * sc);  // between groups
    const int leftPad = juce::roundToInt(12.0f * sc);
    const int rightPad = juce::roundToInt(8.0f * sc);

    lightsZoneW = (reserveTrafficLights || measuredLightsZone > 0) ? juce::jmax(leftPad, measuredLightsZone)
                                                                   : leftPad;

    btnStop.setVisible(false);
    btnEffects.setVisible(!passthrough);
    btnDragToDaw.setVisible(!passthrough && hasClip);
    btnEditor.setVisible(!passthrough);   // the Input roll in Through arrives with the piano-roll phase

    const int pillW = statusPill.idealWidth();
    const int leftW = pillW + groupGap + btn + tight + btn + groupGap + btn + tight + btn;
    const int chipW = btnDragToDaw.isVisible() ? btnDragToDaw.idealWidth() : 0;

    auto rightWidth = [&]()
    {
        int w = 0;
        if (btnEditor.isVisible())  w += btnEditor.captionWidth();
        if (btnEffects.isVisible()) w += tight + btnEffects.captionWidth();
        if (chipW > 0)              w += groupGap + chipW;
        return w;
    };

    applyHeaderCaptions(true);
    int rightW = rightWidth();
    const int fullW = logoIdealWidth();
    const int midiW = logoMidiWidth();
    const int avail = getWidth() - lightsZoneW - rightPad;

    auto fits = [&](int logo) { return (logo > 0 ? logo + groupGap : 0) + leftW + groupGap + rightW <= avail; };

    if (!fits(0))
    {
        applyHeaderCaptions(false);
        rightW = rightWidth();
    }
    int titleW = fullW;
    logoLevel = 2;
    if (!fits(fullW)) { titleW = midiW; logoLevel = 1; }
    if (!fits(midiW)) { titleW = 0;     logoLevel = 0; }

    titleBounds = { lightsZoneW, 0, titleW, h };

    int x = lightsZoneW + (titleW > 0 ? titleW + groupGap : 0);
    statusPill.setVisible(true);
    statusPill.setBounds(x, y, pillW, btn);           x += pillW + groupGap;
    btnPlay.setBounds(x, y, btn, btn);                x += btn + tight;
    btnRecord.setBounds(x, y, btn, btn);              x += btn + groupGap;
    btnSync.setBounds(x, y, btn, btn);                x += btn + tight;
    btnPassthrough.setBounds(x, y, btn, btn);
    transportGroupBounds = {};
    dividerBounds = {};

    int xr = getWidth() - rightPad;
    if (btnEffects.isVisible())
    {
        const int w = btnEffects.captionWidth();
        btnEffects.setBounds(xr - w, y, w, btn);
        xr -= w + tight;
    }
    if (btnEditor.isVisible())
    {
        const int w = btnEditor.captionWidth();
        btnEditor.setBounds(xr - w, y, w, btn);
        xr -= w + groupGap;
    }
    if (chipW > 0)
        btnDragToDaw.setBounds(xr - chipW, y, chipW, btn);
    else
        btnDragToDaw.setBounds({});
}

void TransportBar::paint(juce::Graphics& g)
{
    // Header: surface-100 with a 1px line beneath. No grouped faces or dividers.
    g.setColour(ds::surface100());
    g.fillRect(getLocalBounds());
    g.setColour(ds::line());
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
    paintLogo(g, titleBounds, windowActive ? 1.0f : 0.50f);
}

} // namespace pflow
