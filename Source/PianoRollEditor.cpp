#include "PianoRollEditor.h"
#include <algorithm>

namespace pflow {

namespace {
// -1 = Clip (fit width + note range); else N bars across view.
constexpr int kBarsZoomChoices[] = { -1, 2, 4, 8, 16 };
constexpr int kDivisionChoices[] = { 16, 8, 4, 2, 1 };  // steps per grid unit
} // namespace

void PitchRowMap::fit(const std::vector<RollNote>& notes, float height, int rootPc, int minRange)
{
    rootPc = juce::jlimit(0, 11, rootPc);
    int loN = 127, hiN = 0;
    for (const auto& n : notes)
    {
        loN = juce::jmin(loN, n.pitch);
        hiN = juce::jmax(hiN, n.pitch);
    }
    if (notes.empty()) { loN = 60 + rootPc; hiN = loN; }

    // One note lane above the highest note and one below the lowest.
    int lo = juce::jlimit(0, 126, loN - 1);
    int hi = juce::jlimit(lo + 1, 127, hiN + 1);
    while (hi - lo + 1 < minRange)
    {
        if (lo > 0) --lo;
        if (hi - lo + 1 >= minRange) break;
        if (hi < 127) ++hi;
        else if (lo > 0) --lo;
        else break;
    }
    minPitch = lo;
    maxPitch = hi;
    rowH = height / (float) juce::jmax(1, numRows());
}

double effectiveVelocity(const RollNote& n, const GrooveParams& k)
{
    return noteVelocityWithBase(juce::jlimit(0.04, 1.0, n.fileVelocity / 127.0), n, k);
}

void PianoRollMini::setNotes(std::vector<RollNote> resolvedGrooved, int numBars,
                             const GrooveParams& groove, int root,
                             std::vector<RollNote> frame, Mode m, int div,
                             uint16_t filterMask)
{
    notes = std::move(resolvedGrooved);
    frameNotes = frame.empty() ? notes : std::move(frame);
    bars = juce::jmax(1, numBars);
    knobs = groove;
    rootPc = juce::jlimit(0, 11, root);
    mode = m;
    divisionSteps = juce::jmax(1, div);
    noteFilterMask = (uint16_t) (filterMask == 0 ? 0x0FFF : (filterMask & 0x0FFF));
    repaint();
}

void PianoRollMini::setPlayheadStep(double step, bool isPlaying)
{
    playheadStep = step;
    playing = isPlaying;
    repaint();
}

void PianoRollMini::setTimeStretch(double stretch)
{
    timeStretch = juce::jmax(0.25, stretch);
    repaint();
}

double PianoRollMini::playheadFraction() const
{
    const double total = (double) bars * kStepsPerBar * timeStretch;
    return total > 0.0 ? juce::jlimit(0.0, 1.0, playheadStep * timeStretch / total) : 0.0;
}

void PianoRollMini::paint(juce::Graphics& g)
{
    // Groove preview roll: 14 px bar ruler, key gutter, lanes, square notes in the
    // clip colour (brightness follows velocity), optional velocity lane, green playhead.
    auto b = getLocalBounds().toFloat();
    g.setColour(ds::surface200());
    g.fillRect(b);

    auto ruler = b.removeFromTop((float) kRulerH);
    juce::Rectangle<float> vel;
    if (showVelocity)
        vel = b.removeFromBottom((float) kVelocityH);

    auto gutter = b.removeFromLeft(kGutterW);
    ruler.removeFromLeft(kGutterW);
    if (!vel.isEmpty())
        vel.removeFromLeft(kGutterW);

    // Ruler band and gutter corner.
    g.setColour(ds::surface100());
    g.fillRect(getLocalBounds().toFloat().withHeight((float) kRulerH));
    g.setColour(ds::line());
    g.fillRect(0.0f, (float) kRulerH - 1.0f, (float) getWidth(), 1.0f);
    g.fillRect(gutter.getRight() - 1.0f, 0.0f, 1.0f, (float) getHeight());

    PitchRowMap map;
    map.fit(frameNotes.empty() ? notes : frameNotes, b.getHeight(), rootPc, 0);
    const double totalSteps = (double) bars * kStepsPerBar * timeStretch;
    const float pps = b.getWidth() / (float) juce::jmax(1.0, totalSteps);

    for (int p = map.minPitch; p <= map.maxPitch; ++p)
    {
        const float y = b.getY() + map.yForPitchTop(p, b.getHeight());
        const float h = map.rowH;
        if (y + h < b.getY() || y > b.getBottom()) continue;
        const bool black = isBlackKeyPitch(p);
        g.setColour(black ? ds::surface100() : ds::surface200());
        g.fillRect(b.getX(), y, b.getWidth(), h);
        g.setColour(black ? ds::kb() : ds::kw());
        g.fillRect(gutter.getX(), y, gutter.getWidth() - 1.0f, h);
        if ((noteFilterMask & (uint16_t) (1u << (p % 12))) == 0)
        {
            g.setColour(ds::surface000().withAlpha(0.55f));
            g.fillRect(b.getX(), y, b.getWidth(), h);
        }
    }

    // Bar lines (line) and beat lines (darker hairline), bar numbers in the ruler.
    const int displayBars = juce::jmax(1, (int) std::lround((double) bars * timeStretch));
    const int every = displayBars > 16 ? 4 : (displayBars > 8 ? 2 : 1);
    g.setFont(monoFontFixed(11.0f));
    for (int bar = 0; bar < displayBars; ++bar)
    {
        const float x = b.getX() + (float) (bar * kStepsPerBar) * pps;
        if (bar > 0)
        {
            g.setColour(ds::line());
            g.fillRect(x, b.getY(), 1.0f, b.getHeight());
            if (!vel.isEmpty()) g.fillRect(x, vel.getY(), 1.0f, vel.getHeight());
        }
        if (displayBars <= 8)
        {
            g.setColour(contrastLineOn(ds::surface100()));
            for (int q = 1; q < 4; ++q)
                g.fillRect(x + (float) (q * kStepsPerBar / 4) * pps, b.getY(), 1.0f, b.getHeight());
        }
        if (bar % every == 0)
        {
            g.setColour(contrastInkOn(ds::surface100()));
            g.drawText(juce::String(bar + 1), juce::Rectangle<float>(x + 3.0f, 0.0f, 40.0f, (float) kRulerH - 1.0f),
                       juce::Justification::centredLeft, false);
        }
    }

    if (!vel.isEmpty())
    {
        g.setColour(ds::surface100());
        g.fillRect(vel);
        g.setColour(ds::line());
        g.fillRect(0.0f, vel.getY(), (float) getWidth(), 1.0f);
    }

    {
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(b.toNearestInt());
        for (const auto& n : notes)
        {
            const double v = effectiveVelocity(n, knobs);
            auto r = juce::Rectangle<float>(
                b.getX() + (float) (n.start * timeStretch) * pps,
                b.getY() + map.yForPitchTop(n.pitch, b.getHeight()),
                juce::jmax(2.0f, (float) (n.len * timeStretch) * pps - 0.5f),
                juce::jmax(2.0f, map.rowH - 1.0f));
            const float a = juce::jmax(0.72f, (float) (0.55 + 0.45 * v));
            g.setColour(noteColour.withAlpha(a));
            g.fillRect(r);
            g.setColour(contrastLineOn(noteColour));
            g.drawRect(r, 1.0f);
        }
    }

    if (!vel.isEmpty())
    {
        const float laneH = vel.getHeight() - 6.0f;
        for (const auto& n : notes)
        {
            const double v = effectiveVelocity(n, knobs);
            const float x = vel.getX() + (float) (n.start * timeStretch) * pps;
            const float h = juce::jmax(1.0f, (float) v * laneH);
            g.setColour(noteColour);
            g.fillRect(x, vel.getBottom() - h, 2.0f, h);
            g.fillRect(x - 1.0f, vel.getBottom() - h - 2.0f, 4.0f, 2.0f);
        }
    }

    if (playing)
    {
        const float x = b.getX() + (float) (playheadStep * timeStretch) * pps;
        g.setColour(ds::play());
        g.fillRect(x, 0.0f, 1.0f, (float) getHeight());
        juce::Path tri;
        tri.addTriangle(x - 3.5f, 0.0f, x + 4.5f, 0.0f, x + 0.5f, 6.0f);
        g.fillPath(tri);
    }
}

// ── PianoRollEditor ──────────────────────────────────────────────────────────

PianoRollEditor::PianoRollEditor()
{
    setWantsKeyboardFocus(true);
    // Capture clicks on nested roll/toolbar controls for sticky key-nav.
    addMouseListener(this, true);

    // Instrument strip
    octaveStepper.minValue = -3;
    octaveStepper.maxValue = 3;
    octaveStepper.onChange = [this](int v) { applyEdit([v](ClipEdit& e) { e.octave = v; }); };
    addAndMakeVisible(octaveStepper);

    for (int i = 0; i < 12; ++i)
        rootPicker.addItem(kNoteNames[(size_t) i], i + 1);
    rootPicker.setTextWhenNothingSelected("-");
    rootPicker.setWantsKeyboardFocus(false);
    rootPicker.onChange = [this]
    {
        const int id = rootPicker.getSelectedId();
        if (id > 0)
            applyEdit([id](ClipEdit& e) { e.root = id - 1; });
    };
    addAndMakeVisible(rootPicker);

    // Major / Minor first for discoverability; IDs still map to Mode enum values.
    static constexpr Mode kModeOrder[] = {
        Mode::Ionian, Mode::Aeolian, Mode::Dorian, Mode::Phrygian,
        Mode::Lydian, Mode::Mixolydian, Mode::Locrian
    };
    for (auto m : kModeOrder)
        modePicker.addItem(modeName(m), (int) m + 1);
    modePicker.setWantsKeyboardFocus(false);
    modePicker.onChange = [this]
    {
        const int id = modePicker.getSelectedId();
        if (id > 0)
            applyEdit([id](ClipEdit& e) { e.mode = (Mode) (id - 1); });
    };
    addAndMakeVisible(modePicker);

    fitSwitch.onText = "snapping";
    fitSwitch.onClick = [this]
    {
        const bool on = fitSwitch.getToggleState();
        applyEdit([on](ClipEdit& e)
        {
            e.fitScale = on;
            if (on && e.root < 0) e.root = 0;   // default to C
        });
    };
    addAndMakeVisible(fitSwitch);

    mapSwitch.onClick = [this]
    {
        const bool on = mapSwitch.getToggleState();
        applyEdit([on](ClipEdit& e)
        {
            e.mapToRoot = on;
            if (on && e.root < 0) e.root = 0;   // default to C
        });
    };
    addAndMakeVisible(mapSwitch);

    // Pitch lock lives in EffectsInspector; keep member for ABI but hide it.
    btnLock.setComponentID({});
    btnLock.setVisible(false);
    btnLock.onClick = nullptr;
    addChildComponent(btnLock);

    // Toolbar
    btnRevert.setVisible(false);

    btnRevertNotes.setComponentID("btnRevertNotes");
    btnRevertNotes.setTooltip("Discard note edits (moves, velocities, deletions)");
    btnRevertNotes.onClick = [this] { revertNoteEdits(); };
    addAndMakeVisible(btnRevertNotes);

    btnTrim.onClick = [this]
    {
        if (onTrimRequested)
            onTrimRequested();
        else
            toggleTrim();
    };
    addChildComponent(btnTrim);
    btnTrim.setVisible(false);

    btnFold.setComponentID("btnFold");
    btnFold.accentText = true;
    btnFold.setTooltip("Show only notes that appear in the clip (fold empty pitches)");
    btnFold.onClick = [this]
    {
        folded = !folded;
        btnFold.active = folded;
        btnFold.repaint();
        updateRollSize();
        scrollToContent();
        rollContent.repaint();
        gutter.repaint();
    };
    addAndMakeVisible(btnFold);

    barsZoomPopup.setItems({ "Clip", "2 bars", "4 bars", "8 bars", "16 bars" }, 0);
    barsZoomPopup.setWantsKeyboardFocus(false);
    barsZoomPopup.onChange = [this](int) { applyBarsZoomFromPopup(); };
    addAndMakeVisible(barsZoomPopup);

    divisionPopup.setItems({ "1 bar", "1/2", "1/4", "1/8", "1/16" }, divisionPopupIndex());
    divisionPopup.setWantsKeyboardFocus(false);
    divisionPopup.onChange = [this](int) { applyDivisionFromPopup(); };
    addAndMakeVisible(divisionPopup);

    btnZoomOut.setVisible(false);
    btnZoomIn.setVisible(false);
    selBadge.setVisible(false);

    // Roll
    addAndMakeVisible(gutter);
    addAndMakeVisible(timeRuler);
    rollViewport.setViewedComponent(&rollContent, false);
    rollViewport.setScrollBarsShown(true, true, true, true);
    rollViewport.setScrollBarThickness(10);
    rollViewport.onScrolled = [this]
    {
        gutter.repaint();
        velocityLane.repaint();
        timeRuler.repaint();
    };
    addAndMakeVisible(rollViewport);

    addAndMakeVisible(velocityLane);

    addAndMakeVisible(scalePanel);
    scalePanel.setVisible(false);

    // Empty directory / no selection: show piano roll + grid with no notes.
    resolved.bars = 4;
}

PianoRollEditor::~PianoRollEditor() = default;

// ── model → view ─────────────────────────────────────────────────────────────

void PianoRollEditor::setClip(const StepClip& c, const ClipEdit& e, const GrooveParams& k)
{
    const bool sameClip = hasClip && clip.filePath == c.filePath && clip.name == c.name;
    const int prevBars = hasClip ? resolved.bars : -1;
    hasClip = true;
    clip = c;
    switch (c.kind)
    {
        case ClipKind::Drums: noteColour = ds::clip(1); break;
        case ClipKind::Bass:  noteColour = ds::clip(6); break;
        case ClipKind::Piano: noteColour = ds::clip(7); break;
        case ClipKind::Lead:  noteColour = ds::clip(5); break;
    }
    edit = e;
    groove = k;
    rebuildResolved();
    refreshControls();
    // New clip OR length change (extend/trim) → fit the whole content in view.
    if (!sameClip || resolved.bars != prevBars)
    {
        selection.clear();
        if (!sameClip)
            resetLoopToClip();
        else
            setLoopSteps(loopStartStep, loopEndStep, false);
        visibleBarsZoom = -1; // Clip — fit width + note range
        zoomX = 1.0f;
        barsZoomPopup.setIndex(barsZoomPopupIndex(), juce::dontSendNotification);
        pendingScrollToContent = true;
        zoomToClip();
    }
    else
    {
        // Keep loop inside the (possibly trimmed) clip length.
        setLoopSteps(loopStartStep, loopEndStep, false);
        updateRollSize();
    }
    repaint();
}

void PianoRollEditor::beginRecording(const StepClip& c)
{
    recordingVisual = true;
    setClip(c, {}, {});
    setPlayheadStep(0.0, true);
}

void PianoRollEditor::setRecordingNotes(const StepClip& c, double playhead)
{
    recordingVisual = true;
    hasClip = true;
    clip = c;
    edit = {};
    groove = {};
    rebuildResolved();
    playheadStep = playhead;
    playing = true;
    updateRollSize();

    const int x = (int) contentXFromStep(playheadStep);
    const auto view = rollViewport.getViewArea();
    const int margin = juce::jmax(24, view.getWidth() / 5);
    if (view.getWidth() > 0 && (x > view.getRight() - margin || x < view.getX() + 8))
        rollViewport.setViewPosition(juce::jmax(0, x - view.getWidth() / 2), view.getY());

    rollContent.repaint();
    timeRuler.repaint();
    velocityLane.repaint();
    gutter.repaint();
    if (inputMode)
        repaint(0, 0, getWidth(), toolbarH); // "Recording · bar N"
}

void PianoRollEditor::endRecording(const StepClip& c, const ClipEdit& e, const GrooveParams& g,
                                   double loopEnd)
{
    recordingVisual = false;
    setClip(c, e, g);
    repaint(0, 0, getWidth(), toolbarH);
    setLoopSteps(0.0, loopEnd, true);
    setPlayheadStep(0.0, false);
}

void PianoRollEditor::clearClip()
{
    hasClip = false;
    selection.clear();
    resolved = {};
    resolved.bars = 4; // empty grid placeholder
    grooved.clear();
    foldPitches.clear();
    refreshControls();
    updateRollSize();
    repaint();
}

void PianoRollEditor::setPlayheadStep(double step, bool isPlaying)
{
    playheadStep = step;
    playing = isPlaying;
    rollContent.repaint();
}

void PianoRollEditor::setLockActive(bool locked)
{
    lockActive = locked;
    btnLock.icon = locked ? icons::lockClosed : icons::lockOpen;
    btnLock.active = locked;
    btnLock.setTooltip(locked ? "Pitch edits + trim locked while browsing"
                              : "Lock pitch edits (and trim, if active) while browsing");
    btnLock.repaint();
}

void PianoRollEditor::setTimeStretch(double stretch)
{
    const double s = juce::jmax(0.25, stretch);
    if (std::abs(s - timeStretch) < 1.0e-6) return;
    timeStretch = s;
    computePxPerStepBase();
    updateRollSize();
    rollContent.repaint();
    timeRuler.repaint();
    velocityLane.repaint();
    notifyLoopChanged();
}

void PianoRollEditor::setScalePlacement(ScalePlacement)
{
    // Pitch & Scale lives in the Effects inspector (Cupertino).
}

void PianoRollEditor::rebuildResolved()
{
    if (!hasClip) return;
    resolved = resolveClip(clip, edit);
    grooved = applyGroove(resolved.notes, groove, clip.complexity,
                          clip.kind, clip.root, edit.mode);
    if (edit.fitScale && edit.root >= 0)
        refitNotesToScale(grooved, edit.root, edit.mode);

    ClipEdit noTrim = edit;
    noTrim.clearTrim();
    const auto preTrim = resolveClip(clip, noTrim);
    emptyBarIndices = emptyBars(preTrim.notes, clip.bars);

    // Fold rows from the notes we actually paint/hear (post-groove, re-fitted).
    std::set<int> pitches;
    for (const auto& n : grooved)
        pitches.insert(juce::jlimit(0, 127, n.pitch));
    foldPitches.assign(pitches.begin(), pitches.end());
}

void PianoRollEditor::applyEdit(std::function<void(ClipEdit&)> mutate)
{
    if (!hasClip) return;
    mutate(edit);
    rebuildResolved();
    setLoopSteps(loopStartStep, loopEndStep, true);
    refreshControls();
    updateRollSize();
    repaint();
    if (onEditChanged)
        onEditChanged(edit);
}

void PianoRollEditor::refreshTrimButtonState()
{
    const bool isTrimmed = edit.hasTrim();
    const bool hasNewEmpties = !emptyBarIndices.empty()
                               && emptyBarIndices != edit.removedBars;
    const bool showRestore = isTrimmed && !hasNewEmpties;
    const bool enabled = hasNewEmpties || isTrimmed;
    const juce::String label = showRestore ? "Restore" : "Trim empty measures";
    btnTrim.setEnabled(enabled);
    btnTrim.active = isTrimmed;
    btnTrim.label = label;
    btnTrim.setTooltip(showRestore ? "Restore trimmed bars"
                      : hasNewEmpties ? "Trim all empty measures"
                                      : "No empty measures to trim");
    btnTrim.repaint();
    if (onTrimStateChanged)
        onTrimStateChanged(isTrimmed, enabled, label);
}

void PianoRollEditor::refreshControls()
{
    octaveStepper.setValue(edit.octave, juce::dontSendNotification);
    rootPicker.setSelectedId(edit.root >= 0 ? edit.root + 1 : 0, juce::dontSendNotification);
    modePicker.setSelectedId((int) edit.mode + 1, juce::dontSendNotification);
    fitSwitch.setToggleState(edit.fitScale, juce::dontSendNotification);
    mapSwitch.setToggleState(edit.mapToRoot, juce::dontSendNotification);
    mapSwitch.onText = (edit.mapToRoot && edit.root >= 0 && clip.root >= 0)
        ? juce::String(kNoteNames[(size_t) clip.root]) + juce::String(" -> ")
              + kNoteNames[(size_t) edit.root]
        : juce::String("on");
    mapSwitch.repaint();
    fitSwitch.repaint();

    refreshTrimButtonState();

    btnFold.setEnabled(hasClip && !foldPitches.empty());
    btnFold.active = folded;
    btnFold.repaint();
    btnRevertNotes.setEnabled(hasClip && hasNoteEdits());

    // Edit badges / selection chip removed from the toolbar chrome.
    selBadge.setVisible(false);
    for (auto& chip : badgeChips)
        chip->setVisible(false);
    badgeChips.clear();
    resized();
}

void PianoRollEditor::removeBadge(const juce::String& key)
{
    applyEdit([&key](ClipEdit& e)
    {
        if (key == "oct")        e.octave = -1;
        else if (key == "scale") e.fitScale = false;
        else if (key == "map")   e.mapToRoot = false;
        else if (key == "moves") e.moves.clear();
        else if (key == "trim")  e.clearTrim();
        else if (key == "vel")   e.velocities.clear();
        else if (key == "del")   e.deleted.clear();
    });
}

void PianoRollEditor::deleteSelectedNotes()
{
    if (!hasClip || selection.empty())
        return;
    const auto ids = selection;
    selection.clear();
    applyEdit([&ids](ClipEdit& e)
    {
        for (int id : ids)
            e.deleted.insert(id);
    });
}

bool PianoRollEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (!selection.empty())
        {
            deleteSelectedNotes();
            return true;
        }
    }

    if (!selection.empty() && key.getModifiers().isShiftDown())
    {
        // Compare key codes only — operator== also requires matching modifiers.
        if (key.isKeyCode(juce::KeyPress::upKey))    { nudgeSelection(1, 0); return true; }
        if (key.isKeyCode(juce::KeyPress::downKey))  { nudgeSelection(-1, 0); return true; }
        if (key.isKeyCode(juce::KeyPress::leftKey))  { nudgeSelection(0, -divisionSteps); return true; }
        if (key.isKeyCode(juce::KeyPress::rightKey)) { nudgeSelection(0, divisionSteps); return true; }
    }
    return false;
}

void PianoRollEditor::mouseDown(const juce::MouseEvent&)
{
    if (onActivated)
        onActivated();
}

void PianoRollEditor::nudgeSelection(int dPitch, int dStep)
{
    if (selection.empty() || (dPitch == 0 && dStep == 0)) return;
    const auto ids = selection;
    applyEdit([&ids, dPitch, dStep](ClipEdit& e)
    {
        for (int id : ids)
        {
            auto& mv = e.moves[id];
            mv.dPitch += dPitch;
            mv.dStep += dStep;
            if (mv.dPitch == 0 && mv.dStep == 0)
                e.moves.erase(id);
        }
    });
}

void PianoRollEditor::toggleTrim()
{
    ClipEdit noTrim = edit;
    noTrim.clearTrim();
    const auto preTrim = resolveClip(clip, noTrim);
    const auto toRemove = emptyBars(preTrim.notes, clip.bars);
    emptyBarIndices = toRemove;

    const bool isTrimmed = edit.hasTrim();
    const bool hasNewEmpties = !toRemove.empty() && toRemove != edit.removedBars;

    if (isTrimmed && !hasNewEmpties)
    {
        applyEdit([](ClipEdit& e) { e.clearTrim(); });
        return;
    }
    if (toRemove.empty())
        return;

    applyEdit([toRemove](ClipEdit& e)
    {
        e.clearTrim();
        e.removedBars = toRemove;
    });
}

void PianoRollEditor::resetLoopToClip()
{
    loopStartStep = 0.0;
    loopEndStep = juce::jmax(minLoopSteps(), clipSteps());
    notifyLoopChanged();
    timeRuler.repaint();
    rollContent.repaint();
}

void PianoRollEditor::setLoopSteps(double start, double end, bool notify)
{
    const double minLen = minLoopSteps();
    const double maxEnd = juce::jmax(minLen, clipSteps());
    start = juce::jlimit(0.0, maxEnd - minLen, start);
    end = juce::jlimit(start + minLen, maxEnd, end);
    if (std::abs(start - loopStartStep) < 1.0e-9 && std::abs(end - loopEndStep) < 1.0e-9)
        return;
    loopStartStep = start;
    loopEndStep = end;
    if (notify)
        notifyLoopChanged();
    timeRuler.repaint();
    rollContent.repaint();
}

void PianoRollEditor::notifyLoopChanged()
{
    if (onLoopChanged)
        onLoopChanged(loopStartStep, loopEndStep);
}

double PianoRollEditor::stepFromContentX(float x) const
{
    const float pps = pxPerStep();
    if (pps <= 0.0f || timeStretch <= 0.0)
        return 0.0;
    return (double) x / ((double) pps * timeStretch);
}

float PianoRollEditor::contentXFromStep(double step) const
{
    return (float) (step * timeStretch) * pxPerStep();
}

int PianoRollEditor::hitLoopHandle(float contentX, float hitPx) const
{
    if (recordingVisual)
        return -1;
    const float xs = contentXFromStep(loopStartStep);
    const float xe = contentXFromStep(loopEndStep);
    if (std::abs(contentX - xs) <= hitPx) return 0;
    if (std::abs(contentX - xe) <= hitPx) return 1;
    return -1;
}

void PianoRollEditor::paintLoopOverlay(juce::Graphics& g, juce::Rectangle<float> clipB,
                                       float top, float bottom) const
{
    if (recordingVisual)
        return;
    const float xs = contentXFromStep(loopStartStep);
    const float xe = contentXFromStep(loopEndStep);
    const float h = bottom - top;

    // Dim outside the loop region.
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    if (xs > clipB.getX())
        g.fillRect(clipB.getX(), top, xs - clipB.getX(), h);
    if (xe < clipB.getRight())
        g.fillRect(xe, top, clipB.getRight() - xe, h);

    g.setColour(colours::accent().withAlpha(0.85f));
    g.fillRect(xs - 1.0f, top, 2.0f, h);
    g.fillRect(xe - 1.0f, top, 2.0f, h);
}

void PianoRollEditor::paintTimeRulerLabels(juce::Graphics& g, float viewX,
                                           float width, float height) const
{
    if (!hasClip) return;

    const float pps = pxPerStep();
    if (pps <= 0.0f) return;

    const int bars = juce::jmax(1, resolved.bars);
    const float pxPerBar = (float) kStepsPerBar * (float) timeStretch * pps;
    const float pxPerBeat = pxPerBar / 4.0f;
    const float pxPerDiv = (float) juce::jmax(1, divisionSteps) * (float) timeStretch * pps;
    constexpr float kMinLabelPx = 30.0f;

    // Label resolution scales with how much room each measure has on screen
    // (clip length + zoom), not with the grid picker alone — a 32-bar clip on
    // 1/16 only gets measure numbers; short clips can show beats / divisions.
    enum class Res { EveryNBars, Bars, Beats, Divisions };
    Res res = Res::Bars;
    int barStride = 1;

    if (pxPerBar < kMinLabelPx)
    {
        res = Res::EveryNBars;
        while ((float) barStride * pxPerBar < kMinLabelPx && barStride < 64)
            barStride *= 2;
    }
    else if (bars <= 8 && pxPerBeat >= kMinLabelPx)
    {
        res = (bars <= 4 && pxPerDiv >= kMinLabelPx && divisionSteps < kStepsPerBar)
                  ? Res::Divisions
                  : Res::Beats;
    }
    else if (bars <= 16 && pxPerBeat >= kMinLabelPx * 1.15f)
    {
        res = Res::Beats;
    }

    g.setFont(monoFont(11.0f, true));

    auto drawLabel = [&](float x, const juce::String& text, bool strong)
    {
        g.setColour(strong ? colours::text2() : colours::text3());
        g.fillRect(x, height - (strong ? 4.0f : 3.0f), strong ? 1.0f : 0.5f,
                   strong ? 4.0f : 3.0f);
        if (text.isNotEmpty())
            g.drawText(text, juce::Rectangle<float>(x + 3.0f, 1.0f, 40.0f, height - 5.0f),
                       juce::Justification::centredLeft, false);
    };

    if (res == Res::EveryNBars || res == Res::Bars)
    {
        for (int bar = 0; bar <= bars; ++bar)
        {
            if (res == Res::EveryNBars && (bar % barStride) != 0)
                continue;
            const double step = (double) bar * (double) kStepsPerBar;
            const float x = contentXFromStep(step) - viewX;
            if (x < -40.0f || x > width + 4.0f) continue;
            drawLabel(x, bar < bars ? juce::String(bar + 1) : juce::String(), true);
        }
    }
    else if (res == Res::Beats)
    {
        const int totalBeats = bars * 4;
        for (int beat = 0; beat <= totalBeats; ++beat)
        {
            const double step = (double) beat * 4.0; // 4 steps per beat
            const float x = contentXFromStep(step) - viewX;
            if (x < -40.0f || x > width + 4.0f) continue;
            const int bar = beat / 4;
            const int beatInBar = beat % 4;
            const bool isBar = beatInBar == 0;
            const juce::String label = beat < totalBeats
                ? (isBar ? juce::String(bar + 1)
                         : juce::String(bar + 1) + "." + juce::String(beatInBar + 1))
                : juce::String();
            drawLabel(x, label, isBar);
        }
    }
    else // Divisions
    {
        const int div = juce::jmax(1, divisionSteps);
        const int total = (int) std::lround((double) bars * (double) kStepsPerBar);
        for (int s = 0; s <= total; s += div)
        {
            const float x = contentXFromStep((double) s) - viewX;
            if (x < -40.0f || x > width + 4.0f) continue;
            const int bar = s / kStepsPerBar;
            const int stepInBar = s % kStepsPerBar;
            const bool isBar = stepInBar == 0;
            const bool isBeat = (stepInBar % 4) == 0;
            juce::String label;
            if (s < total)
            {
                if (isBar)
                    label = juce::String(bar + 1);
                else if (isBeat)
                    label = juce::String(bar + 1) + "." + juce::String(stepInBar / 4 + 1);
            }
            drawLabel(x, label, isBar);
        }
    }

    if (recordingVisual)
        return;

    // Loop handles on the ruler.
    const float xs = contentXFromStep(loopStartStep) - viewX;
    const float xe = contentXFromStep(loopEndStep) - viewX;
    g.setColour(colours::accent());
    auto drawHandle = [&](float x)
    {
        juce::Path tri;
        tri.addTriangle(x - 5.0f, 2.0f, x + 5.0f, 2.0f, x, height - 1.0f);
        g.fillPath(tri);
    };
    drawHandle(xs);
    drawHandle(xe);
}

void PianoRollEditor::selectPitch(int pitch, bool additive)
{
    if (!additive)
        selection.clear();
    for (const auto& n : resolved.notes)
        if (n.pitch == pitch)
            selection.insert(n.id);
    refreshControls();
    rollContent.repaint();
    gutter.repaint();
    velocityLane.repaint();
    grabKeyboardFocus();
}

// ── row geometry ─────────────────────────────────────────────────────────────

int PianoRollEditor::rowForPitch(int pitch) const
{
    if (!folded)
        return 127 - juce::jlimit(0, 127, pitch);
    if (foldPitches.empty())
        return 0;
    // Nearest note-bearing row (exact for resolved pitches).
    auto it = std::lower_bound(foldPitches.begin(), foldPitches.end(), pitch);
    if (it == foldPitches.end()) --it;
    const int idx = (int) std::distance(foldPitches.begin(), it);
    return (int) foldPitches.size() - 1 - idx;
}

int PianoRollEditor::pitchForRow(int row) const
{
    if (!folded)
        return 127 - juce::jlimit(0, 127, row);
    if (foldPitches.empty())
        return 60;
    const int idx = juce::jlimit(0, (int) foldPitches.size() - 1,
                                 (int) foldPitches.size() - 1 - row);
    return foldPitches[(size_t) idx];
}

juce::Rectangle<float> PianoRollEditor::noteRect(const RollNote& n, int rowOffset,
                                                 double stepOffset) const
{
    const float pps = pxPerStep();
    const int row = juce::jlimit(0, numRows() - 1, rowForPitch(n.pitch) + rowOffset);
    return { (float) ((n.start + stepOffset) * timeStretch) * pps,
             (float) row * effRowH(),
             juce::jmax(3.0f, (float) (n.len * timeStretch) * pps - 1.0f),
             juce::jmax(3.0f, effRowH() - 1.0f) };
}

const RollNote* PianoRollEditor::noteAt(juce::Point<float> pos) const
{
    for (auto it = grooved.rbegin(); it != grooved.rend(); ++it)
        if (noteRect(*it).contains(pos))
            return &(*it);
    return nullptr;
}

void PianoRollEditor::computePxPerStepBase()
{
    // Prefer the viewport's laid-out width; fall back to the editor body so we
    // never compute a tiny pps before the first real layout (which looks condensed).
    int visW = rollViewport.getMaximumVisibleWidth();
    if (visW < 32)
        visW = juce::jmax(60, getWidth() - gutterW - 8);
    visW = juce::jmax(60, visW);

    const int clipBars = juce::jmax(1, resolved.bars);
    // Clip (≤0 legacy / -1): fit the whole clip. Otherwise fit exactly N bars
    // across the piano-roll viewport so "8 bars" fills the window with 8 bars.
    const int bars = (visibleBarsZoom <= 0) ? clipBars : juce::jmax(1, visibleBarsZoom);
    const double stretch = juce::jmax(0.25, timeStretch);
    // Visual span of one musical bar at zoomX=1.
    const double visualSteps = (double) bars * (double) kStepsPerBar * stretch;
    pxPerStepBase = juce::jmax(0.25f, (float) ((double) visW / juce::jmax(1.0, visualSteps)));
}

void PianoRollEditor::updateRollSize()
{
    int visW = rollViewport.getMaximumVisibleWidth();
    if (visW < 32)
        visW = juce::jmax(1, getWidth() - gutterW - 8);
    visW = juce::jmax(1, visW);
    const int visH = juce::jmax(1, rollViewport.getMaximumVisibleHeight());

    // Content width tracks the clip at the current zoom (musical steps * stretch * pps).
    const double clipVisualSteps = (double) juce::jmax(1, resolved.bars)
                                 * (double) kStepsPerBar
                                 * juce::jmax(0.25, timeStretch);
    const int contentW = (int) std::ceil(clipVisualSteps * (double) pxPerStep());
    const int w = juce::jmax(contentW, visW);
    const int h = juce::jmax((int) std::ceil((float) numRows() * effRowH()), visH);
    rollContent.setSize(w, h);
    gutter.repaint();
    velocityLane.repaint();
}

int PianoRollEditor::barsZoomPopupIndex() const
{
    for (int i = 0; i < (int) (sizeof(kBarsZoomChoices) / sizeof(kBarsZoomChoices[0])); ++i)
        if (kBarsZoomChoices[i] == visibleBarsZoom)
            return i;
    return 0; // Clip
}

int PianoRollEditor::divisionPopupIndex() const
{
    for (int i = 0; i < (int) (sizeof(kDivisionChoices) / sizeof(kDivisionChoices[0])); ++i)
        if (kDivisionChoices[i] == divisionSteps)
            return i;
    return 2; // 1/4
}

void PianoRollEditor::applyBarsZoomFromPopup()
{
    const int idx = juce::jlimit(0, (int) (sizeof(kBarsZoomChoices) / sizeof(kBarsZoomChoices[0])) - 1,
                                 barsZoomPopup.getIndex());
    visibleBarsZoom = kBarsZoomChoices[idx];
    zoomX = 1.0f; // dropdown defines the fit; discard pinch/scroll zoom
    if (visibleBarsZoom < 0)
    {
        zoomToClip();
        return;
    }
    computePxPerStepBase();
    updateRollSize();
    rollContent.repaint();
    timeRuler.repaint();
    velocityLane.repaint();
}

void PianoRollEditor::applyDivisionFromPopup()
{
    const int idx = juce::jlimit(0, (int) (sizeof(kDivisionChoices) / sizeof(kDivisionChoices[0])) - 1,
                                 divisionPopup.getIndex());
    divisionSteps = kDivisionChoices[idx];
    rollContent.repaint();
    timeRuler.repaint();
}

int PianoRollEditor::velocityForNote(const RollNote& n) const
{
    if (auto it = edit.velocities.find(n.id); it != edit.velocities.end())
        return it->second;
    return juce::jlimit(1, 127, (int) std::lround(effectiveVelocity(n, groove) * 127.0));
}

const RollNote* PianoRollEditor::noteAtVelocityX(float laneX) const
{
    const float viewX = (float) rollViewport.getViewPositionX();
    const float pps = pxPerStep();
    const RollNote* best = nullptr;
    float bestDist = 1.0e9f;
    for (const auto& n : grooved)
    {
        const float nx = (float) gutterW + contentXFromStep(n.start) - viewX;
        const float nw = juce::jmax(4.0f, (float) (n.len * timeStretch) * pps);
        if (laneX < nx - 3.0f || laneX > nx + nw + 3.0f) continue;
        const float cx = nx + nw * 0.5f;
        const float d = std::abs(laneX - cx);
        if (d < bestDist)
        {
            bestDist = d;
            best = &n;
        }
    }
    return best;
}

void PianoRollEditor::setVelocityFromLaneY(int noteId, float laneY)
{
    const int laneTop = VelocityLane::headerH;
    const int laneH = VelocityLane::laneH;
    const float t = 1.0f - juce::jlimit(0.0f, 1.0f, (laneY - (float) laneTop) / (float) laneH);
    const int vel = juce::jlimit(1, 127, (int) std::lround(t * 127.0));
    applyEdit([noteId, vel](ClipEdit& e) { e.velocities[noteId] = vel; });
    velocityLane.repaint();
}

void PianoRollEditor::scrollToContent()
{
    zoomToClip();
}

void PianoRollEditor::zoomToClip()
{
    if (!hasClip)
        return;

    // Horizontal: fit the full clip width (Zoom → Clip).
    visibleBarsZoom = -1;
    zoomX = 1.0f;
    barsZoomPopup.setIndex(0, juce::dontSendNotification);
    computePxPerStepBase();

    const int visH = rollViewport.getMaximumVisibleHeight();
    if (visH < 32)
    {
        pendingScrollToContent = true;
        updateRollSize();
        return;
    }

    if (folded)
    {
        // Note rows, plus one empty lane above the highest and one below the lowest.
        const int span = juce::jmax(1, (int) foldPitches.size()) + 2;
        rowH = juce::jlimit(kMinRowH, kMaxRowH, (float) visH / (float) span * 0.5f);
        updateRollSize();
        rollViewport.setViewPosition(0, 0);
        pendingScrollToContent = false;
        rollContent.repaint();
        gutter.repaint();
        timeRuler.repaint();
        velocityLane.repaint();
        return;
    }

    int loN = 127, hiN = 0;
    for (const auto& n : resolved.notes)
    {
        loN = juce::jmin(loN, n.pitch);
        hiN = juce::jmax(hiN, n.pitch);
    }
    if (resolved.notes.empty())
        loN = hiN = 60;

    // Frame the notes with one semitone lane above the highest and one below the lowest.
    const int viewLo = juce::jlimit(0, 126, loN - 1);
    const int viewHi = juce::jlimit(viewLo + 1, 127, hiN + 1);
    const int lanes = juce::jmax(1, viewHi - viewLo + 1);
    rowH = juce::jlimit(kMinRowH, kMaxRowH, (float) visH / (float) lanes);
    updateRollSize();

    const float clusterTop = (float) rowForPitch(viewHi) * effRowH();
    const float clusterH = (float) lanes * effRowH();
    const float yCentre = clusterTop - ((float) visH - clusterH) * 0.5f;
    const int maxY = juce::jmax(0, rollContent.getHeight() - visH);
    rollViewport.setViewPosition(0, juce::jlimit(0, maxY, (int) std::lround(yCentre)));
    pendingScrollToContent = false;
    rollContent.repaint();
    gutter.repaint();
    timeRuler.repaint();
    velocityLane.repaint();
}

void PianoRollEditor::zoomXAround(float factor, float contentX)
{
    const float pps = pxPerStep();
    const float anchorStep = contentX / pps;
    const float cursorInView = contentX - (float) rollViewport.getViewPositionX();

    zoomX = juce::jlimit(1.0f, kMaxZoomX, zoomX * factor);
    updateRollSize();

    rollViewport.setViewPosition(
        (int) std::round(anchorStep * pxPerStep() - cursorInView),
        rollViewport.getViewPositionY());
    rollContent.repaint();
}

void PianoRollEditor::zoomRowsAround(float factor, float contentY)
{
    const float anchorRow = contentY / effRowH();
    const float cursorInView = contentY - (float) rollViewport.getViewPositionY();

    const int visH = juce::jmax(32, rollViewport.getMaximumVisibleHeight());
    // Most zoomed-in: exactly one octave fills the viewport.
    const float maxRowH = folded ? ((float) visH / 12.0f) * 0.5f : ((float) visH / 12.0f);
    rowH = juce::jlimit(kMinRowH, juce::jmin(kMaxRowH, maxRowH), rowH * factor);
    updateRollSize();

    rollViewport.setViewPosition(
        rollViewport.getViewPositionX(),
        (int) std::round(anchorRow * effRowH() - cursorInView));
    rollContent.repaint();
}

void PianoRollEditor::commitNoteDrag()
{
    auto& rc = rollContent;
    if (rc.dragDRows == 0 && rc.dragDStep == 0)
        return;

    std::vector<int> ids;
    if (selection.count(rc.dragNoteId) > 0)
        ids.assign(selection.begin(), selection.end());
    else
        ids.push_back(rc.dragNoteId);

    // Per-note pitch delta: rows map 1:1 to semitones unfolded, and to the
    // note-bearing rows when folded (Live-style fold drag).
    std::map<int, int> pitchDelta;
    for (int id : ids)
        for (const auto& n : resolved.notes)
            if (n.id == id)
            {
                const int row = juce::jlimit(0, numRows() - 1, rowForPitch(n.pitch) + rc.dragDRows);
                pitchDelta[id] = pitchForRow(row) - n.pitch;
                break;
            }

    const int dStep = rc.dragDStep;
    applyEdit([&pitchDelta, dStep](ClipEdit& e)
    {
        for (const auto& [id, dPitch] : pitchDelta)
        {
            auto& mv = e.moves[id];
            mv.dPitch += dPitch;
            mv.dStep += dStep;
            if (mv.dPitch == 0 && mv.dStep == 0)
                e.moves.erase(id);
        }
    });
}

// ── layout / paint ───────────────────────────────────────────────────────────

void PianoRollEditor::resized()
{
    auto r = getLocalBounds();

    btnRevert.setVisible(false);
    btnLock.setVisible(false);
    octaveStepper.setVisible(false);
    rootPicker.setVisible(false);
    modePicker.setVisible(false);
    fitSwitch.setVisible(false);
    mapSwitch.setVisible(false);
    scalePanel.setVisible(false);
    btnZoomIn.setVisible(false);
    btnZoomOut.setVisible(false);
    selBadge.setVisible(false);

    titleBounds = r.removeFromTop(toolbarH).reduced(8, 0);
    layoutHeaderTools();

    const int laneH = velocityOpen ? VelocityLane::headerH + VelocityLane::laneH
                                   : VelocityLane::headerH;
    velocityLane.setBounds(r.removeFromBottom(laneH));

    auto rollArea = r;
    auto rulerRow = rollArea.removeFromTop(rulerH);
    gutter.setBounds(rollArea.removeFromLeft(gutterW));
    timeRuler.setBounds(rulerRow.withTrimmedLeft(gutterW));
    rollViewport.setBounds(rollArea);
    computePxPerStepBase();
    updateRollSize();
    if (pendingScrollToContent)
        scrollToContent();
}

void PianoRollEditor::layoutScaleControls(juce::Rectangle<int> strip)
{
    octaveStepper.setVisible(true);
    rootPicker.setVisible(true);
    modePicker.setVisible(true);
    fitSwitch.setVisible(true);
    mapSwitch.setVisible(true);

    octaveStepper.setBounds(strip.removeFromLeft(80).withSizeKeepingCentre(80, 26));
    strip.removeFromLeft(6);
    rootPicker.setBounds(strip.removeFromLeft(96).withSizeKeepingCentre(96, 26));
    strip.removeFromLeft(4);
    modePicker.setBounds(strip.removeFromLeft(118).withSizeKeepingCentre(118, 26));
    strip.removeFromLeft(10);
    const int secondaryW = juce::jmax(0, strip.getWidth());
    const int fitW = juce::jmin(fitSwitch.idealWidth(), secondaryW / 2);
    fitSwitch.setBounds(strip.removeFromLeft(fitW));
    strip.removeFromLeft(8);
    mapSwitch.setBounds(strip.removeFromLeft(juce::jmin(mapSwitch.idealWidth(), strip.getWidth())));
}

void PianoRollEditor::paint(juce::Graphics& g)
{
    g.setColour(ds::surface100());
    g.fillRect(getLocalBounds());
    g.setColour(ds::surface300());
    g.fillRect(0, 0, getWidth(), toolbarH);
    g.setColour(ds::line());
    g.fillRect(0, toolbarH - 1, getWidth(), 1);

    // Title · subtitle (ellipsised into whatever the tools leave free)
    auto t = titleBounds;
    const juce::String title = inputMode ? "Input" : "Piano Roll";
    juce::String sub;
    if (inputMode)
        sub = inputSub;
    else if (hasClip)
        sub = clip.name + juce::String::fromUTF8(" \u00b7 ") + juce::String(juce::jmax(1, resolved.bars))
              + (resolved.bars == 1 ? " bar" : " bars");

    const auto titleFont = uiFontFixed(11.0f, true);
    g.setFont(titleFont);
    g.setColour(ds::ink());
    const int tw = juce::jmin(t.getWidth(),
                              (int) std::ceil(juce::GlyphArrangement::getStringWidth(titleFont, title)) + 1);
    g.drawText(title, t.removeFromLeft(tw), juce::Justification::centredLeft, false);
    t.removeFromLeft(6);

    if (inputMode)
    {
        // Record status on the right: ● Recording · bar N  /  Press record to capture
        const bool rec = recordingVisual;
        const juce::String status = rec ? "Recording " + juce::String::fromUTF8("\u00b7")
                                            + " bar " + juce::String((int) (playheadStep / kStepsPerBar) + 1)
                                        : juce::String("Press record to capture");
        const auto sf = uiFontFixed(11.0f, false);
        const int sw = (int) std::ceil(juce::GlyphArrangement::getStringWidth(sf, status)) + 1;
        auto right = getLocalBounds().removeFromTop(toolbarH).reduced(8, 0);
        auto sb = right.removeFromRight(juce::jmin(right.getWidth() / 2, sw));
        const auto col = rec ? ds::record() : ds::inkMuted();
        g.setColour(col);
        g.setFont(sf);
        g.drawText(status, sb, juce::Justification::centredLeft, true);
        auto dot = juce::Rectangle<float>(8.0f, 8.0f).withCentre({ (float) sb.getX() - 8.0f, (float) sb.getCentreY() });
        g.fillEllipse(dot);
        t.setRight(juce::jmin(t.getRight(), sb.getX() - 20));
    }

    if (sub.isNotEmpty() && t.getWidth() > 16)
    {
        g.setFont(monoFontFixed(10.5f));
        g.setColour(ds::inkMuted());
        g.drawText(sub, t, juce::Justification::centredLeft, true);
    }

    if (!inputMode)
    {
        g.setFont(uiFontFixed(11.0f, false));
        g.setColour(ds::inkMuted());
        if (!zoomLabelBounds.isEmpty())
            g.drawText("Zoom", zoomLabelBounds, juce::Justification::centredRight, false);
        if (!gridLabelBounds.isEmpty())
            g.drawText("Snap", gridLabelBounds, juce::Justification::centredRight, false);
    }
}

void PianoRollEditor::layoutHeaderTools()
{
    // Header (right to left): Revert · Snap [div] · Zoom [Clip/n] · Fold
    auto bar = titleBounds;
    const int ctrlH = 18; // Groove control-md
    const bool tools = !inputMode;
    btnFold.setVisible(tools);
    barsZoomPopup.setVisible(tools);
    divisionPopup.setVisible(tools);
    btnRevertNotes.setVisible(tools);
    zoomLabelBounds = gridLabelBounds = {};
    if (!tools)
    {
        titleBounds = bar;
        return;
    }

    auto place = [&bar](juce::Component& c, int w)
    {
        c.setBounds(bar.removeFromRight(w).withSizeKeepingCentre(w, ctrlH));
        bar.removeFromRight(4);
    };
    // Title + a short subtitle need ~150px; drop labels first, then Zoom.
    const int revertW = btnRevertNotes.idealWidth();
    const int gridW = juce::jmax(46, divisionPopup.idealWidth() - 8);
    const int zoomW = juce::jmax(56, barsZoomPopup.idealWidth() - 8);
    const int foldW = btnFold.idealWidth();
    const int labels = 26 + 4 + 30 + 4;
    const int full = revertW + gridW + zoomW + foldW + 4 * 4 + 8 + labels;
    const bool showLabels = bar.getWidth() - full >= 150;
    const bool showZoom = bar.getWidth() - (full - labels) >= 120;

    place(btnRevertNotes, revertW);
    bar.removeFromRight(4);
    place(divisionPopup, gridW);
    if (showLabels) { gridLabelBounds = bar.removeFromRight(26); bar.removeFromRight(8); }
    barsZoomPopup.setVisible(showZoom);
    if (showZoom)
    {
        place(barsZoomPopup, zoomW);
        if (showLabels) { zoomLabelBounds = bar.removeFromRight(30); bar.removeFromRight(8); }
    }
    place(btnFold, foldW);
    bar.removeFromRight(4);
    titleBounds = bar;
}

void PianoRollEditor::setInputMode(bool isInput, const juce::String& subtitle)
{
    if (inputMode == isInput && inputSub == subtitle)
        return;
    inputMode = isInput;
    inputSub = subtitle;
    resized();
    repaint();
}

bool PianoRollEditor::hasNoteEdits() const
{
    return !edit.moves.empty() || !edit.velocities.empty() || !edit.deleted.empty();
}

void PianoRollEditor::revertNoteEdits()
{
    if (!hasNoteEdits())
        return;
    edit.moves.clear();
    edit.velocities.clear();
    edit.deleted.clear();
    selection.clear();
    rebuildResolved();
    refreshControls();
    rollContent.repaint();
    velocityLane.repaint();
    if (onEditChanged)
        onEditChanged(edit);
}

void PianoRollEditor::paintOverChildren(juce::Graphics& g)
{
    if (!hasClip && emptyHint.isNotEmpty())
    {
        auto area = rollViewport.getBounds();
        g.setColour(ds::inkMuted());
        g.setFont(uiFontFixed(12.0f, false));
        g.drawFittedText(emptyHint, area.reduced(24), juce::Justification::centred, 2);
    }
    g.setColour(colours::line());
    g.fillRect(0, 0, 1, getHeight());
    g.fillRect(getWidth() - 1, 0, 1, getHeight());
}

// ── RollContent ──────────────────────────────────────────────────────────────

void PianoRollEditor::RollContent::paint(juce::Graphics& g)
{
    auto& ed = owner;

    g.fillAll(colours::rollBg());

    const float pps = ed.pxPerStep();
    const auto clipB = g.getClipBounds().toFloat();

    // Pitch rows: quiet zebra via black-key lane shade only (no loud accent washes).
    const int firstRow = juce::jmax(0, (int) std::floor(clipB.getY() / ed.effRowH()));
    const int lastRow = juce::jmin(ed.numRows() - 1, (int) std::ceil(clipB.getBottom() / ed.effRowH()));
    for (int row = firstRow; row <= lastRow; ++row)
    {
        const float y = (float) row * ed.effRowH();
        const int pitch = ed.pitchForRow(row);
        if (isBlackKeyPitch(pitch))
        {
            g.setColour(colours::rollShade());
            g.fillRect(clipB.getX(), y, clipB.getWidth(), ed.effRowH());
        }
        if (ed.hasClip && !ed.edit.isNoteFilterEnabled(pitch))
        {
            g.setColour(ds::trk().withAlpha(usesDarkAppearance() ? 0.50f : 0.38f));
            g.fillRect(clipB.getX(), y, clipB.getWidth(), ed.effRowH());
        }
        g.setColour(colours::rollRowline());
        g.fillRect(clipB.getX(), y + ed.effRowH() - 0.5f, clipB.getWidth(), 0.5f);
    }

    // Vertical grid — barely lighter than the background.
    const int total = ed.totalSteps();
    for (int s = 0; s <= total; s += ed.divisionSteps)
    {
        const float x = (float) s * pps;
        if (x < clipB.getX() - 2.0f || x > clipB.getRight() + 2.0f) continue;
        const bool isBar = (s % kStepsPerBar) == 0;
        g.setColour(isBar ? contrastLineOn(ds::surface100()).withAlpha(0.55f)
                          : contrastLineOn(ds::surface100()).withAlpha(0.28f));
        g.fillRect(x, clipB.getY(), isBar ? 0.7f : 0.35f, clipB.getHeight());
    }

    // MIDI notes use --note; playhead uses --acc (§1 / §6).
    const bool draggingNotes = drag == Drag::Note;
    for (const auto& n : ed.grooved)
    {
        const bool isSelected = ed.selection.count(n.id) > 0;
        const bool inDrag = draggingNotes
            && (n.id == dragNoteId || (ed.selection.count(dragNoteId) > 0 && isSelected));
        auto r = ed.noteRect(n, inDrag ? dragDRows : 0, inDrag ? (double) dragDStep : 0.0);
        if (!r.intersects(clipB)) continue;

        const double v = effectiveVelocity(n, ed.groove);
        // Groove: square notes in the clip colour; brightness follows velocity.
        const auto base = ed.noteColour;
        juce::Colour fill = (n.moved || inDrag) ? base.brighter(0.12f) : base;
        g.setColour(fill.withAlpha(juce::jmax(0.75f, (float) (0.55 + 0.45 * v))));
        g.fillRect(r);
        g.setColour(contrastLineOn(fill));
        g.drawRect(r, 1.0f);

        if (isSelected)
        {
            g.setColour(ds::acc());
            g.drawRect(r.expanded(1.0f), 1.5f);
        }
    }

    // Marquee band
    if (drag == Drag::Marquee)
    {
        g.setColour(colours::accentSoft());
        g.fillRect(marquee);
        g.setColour(colours::accentLine());
        g.drawRect(marquee, 1.0f);
    }

    // Playhead: the play colour (it moves because the music does).
    if (ed.playing)
    {
        g.setColour(ds::play());
        g.fillRect((float) (ed.playheadStep * ed.timeStretch) * pps, clipB.getY(), 1.5f, clipB.getHeight());
    }

    if (ed.hasClip)
        ed.paintLoopOverlay(g, clipB, clipB.getY(), clipB.getBottom());
}

void PianoRollEditor::RollContent::mouseDown(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (!ed.hasClip) return;

    dragStart = e.position;
    dragDRows = 0;
    dragDStep = 0;

    if (e.mods.isMiddleButtonDown())
    {
        drag = Drag::Pan;
        panStartView = ed.rollViewport.getViewPosition();
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return;
    }

    if (const int handle = ed.hitLoopHandle(e.position.x); handle >= 0)
    {
        drag = handle == 0 ? Drag::LoopStart : Drag::LoopEnd;
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        return;
    }

    if (const auto* n = ed.noteAt(e.position))
    {
        dragNoteId = n->id;
        drag = Drag::Note;
        if (e.mods.isShiftDown())
        {
            if (!ed.selection.insert(n->id).second)
                ed.selection.erase(n->id);
        }
        else if (ed.selection.count(n->id) == 0)
        {
            ed.selection.clear();
            ed.selection.insert(n->id);
        }
        ed.refreshControls();
        repaint();
        ed.velocityLane.repaint();
        return;
    }

    drag = Drag::Marquee;
    marqueeAdditive = e.mods.isShiftDown();
    marquee = juce::Rectangle<float>(dragStart, dragStart);
    if (!marqueeAdditive)
        ed.selection.clear();
    ed.refreshControls();
    repaint();
}

void PianoRollEditor::RollContent::mouseDrag(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (drag == Drag::Pan)
    {
        const auto d = e.getOffsetFromDragStart();
        ed.rollViewport.setViewPosition(panStartView.x - d.x, panStartView.y - d.y);
        return;
    }
    if (drag == Drag::LoopStart || drag == Drag::LoopEnd)
    {
        const double step = ed.stepFromContentX(e.position.x);
        // Live-update the processor so the audible loop follows the handles.
        if (drag == Drag::LoopStart)
            ed.setLoopSteps(step, ed.loopEndStep, true);
        else
            ed.setLoopSteps(ed.loopStartStep, step, true);
        return;
    }
    if (drag == Drag::Note)
    {
        const float dx = e.position.x - dragStart.x;
        const float dy = e.position.y - dragStart.y;
        const int div = juce::jmax(1, ed.divisionSteps);
        // Account for time-stretch so drag distance matches visual grid.
        const float stepPx = ed.pxPerStep() * (float) ed.timeStretch;
        const int stepsMoved = (int) std::round(dx / juce::jmax(0.01f, stepPx) / (float) div) * div;
        const int rowsMoved = (int) std::round(dy / ed.effRowH());
        if (stepsMoved != dragDStep || rowsMoved != dragDRows)
        {
            dragDStep = stepsMoved;
            dragDRows = rowsMoved;
            repaint();
        }
        return;
    }
    if (drag == Drag::Marquee)
    {
        marquee = juce::Rectangle<float>(dragStart, e.position);
        repaint();
    }
}

void PianoRollEditor::RollContent::mouseUp(const juce::MouseEvent&)
{
    auto& ed = owner;
    if (drag == Drag::LoopStart || drag == Drag::LoopEnd)
    {
        const double div = (double) juce::jmax(1, ed.divisionSteps);
        auto snap = [div](double s) { return std::round(s / div) * div; };
        if (drag == Drag::LoopStart)
            ed.setLoopSteps(snap(ed.loopStartStep), ed.loopEndStep, true);
        else
            ed.setLoopSteps(ed.loopStartStep, snap(ed.loopEndStep), true);
    }
    else if (drag == Drag::Note)
    {
        ed.commitNoteDrag();
        dragNoteId = -1;
    }
    else if (drag == Drag::Marquee)
    {
        if (marquee.getWidth() > 2.0f || marquee.getHeight() > 2.0f)
        {
            if (!marqueeAdditive)
                ed.selection.clear();
            for (const auto& n : ed.grooved)
                if (ed.noteRect(n).intersects(marquee))
                    ed.selection.insert(n.id);
            ed.refreshControls();
            ed.gutter.repaint();
            ed.velocityLane.repaint();
        }
    }
    drag = Drag::None;
    dragDRows = dragDStep = 0;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void PianoRollEditor::RollContent::mouseWheelMove(const juce::MouseEvent& e,
                                                  const juce::MouseWheelDetails& wheel)
{
    // ⌘/ctrl+wheel = horizontal zoom toward the cursor; alt+wheel = vertical
    // zoom. Anything else bubbles to the viewport, which pans both axes
    // (trackpad two-finger scrolling).
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const float factor = 1.0f + juce::jlimit(-0.4f, 0.4f, wheel.deltaY * 2.2f);
        owner.zoomXAround(factor, e.position.x);
        return;
    }
    if (e.mods.isAltDown())
    {
        const float factor = 1.0f + juce::jlimit(-0.4f, 0.4f, wheel.deltaY * 2.2f);
        owner.zoomRowsAround(factor, e.position.y);
        return;
    }
    juce::Component::mouseWheelMove(e, wheel);
}

void PianoRollEditor::RollContent::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    // Trackpad pinch: zoom time toward the cursor (Live/Logic feel).
    owner.zoomXAround(scaleFactor, e.position.x);
}

// ── TimeRuler ────────────────────────────────────────────────────────────────

void PianoRollEditor::TimeRuler::paint(juce::Graphics& g)
{
    auto& ed = owner;
    g.fillAll(colours::panel());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromBottom(1));

    if (!ed.hasClip) return;

    const float viewX = (float) ed.rollViewport.getViewPositionX();
    ed.paintTimeRulerLabels(g, viewX, (float) getWidth(), (float) getHeight());
}

void PianoRollEditor::TimeRuler::mouseDown(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (!ed.hasClip) return;
    const float contentX = e.position.x + (float) ed.rollViewport.getViewPositionX();
    if (const int handle = ed.hitLoopHandle(contentX, 8.0f); handle >= 0)
    {
        drag = handle == 0 ? Drag::LoopStart : Drag::LoopEnd;
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
}

void PianoRollEditor::TimeRuler::mouseDrag(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (drag == Drag::None) return;
    const float contentX = e.position.x + (float) ed.rollViewport.getViewPositionX();
    const double step = ed.stepFromContentX(contentX);
    if (drag == Drag::LoopStart)
        ed.setLoopSteps(step, ed.loopEndStep, true);
    else
        ed.setLoopSteps(ed.loopStartStep, step, true);
}

void PianoRollEditor::TimeRuler::mouseUp(const juce::MouseEvent&)
{
    auto& ed = owner;
    if (drag == Drag::None) return;
    const double div = (double) juce::jmax(1, ed.divisionSteps);
    auto snap = [div](double s) { return std::round(s / div) * div; };
    if (drag == Drag::LoopStart)
        ed.setLoopSteps(snap(ed.loopStartStep), ed.loopEndStep, true);
    else
        ed.setLoopSteps(ed.loopStartStep, snap(ed.loopEndStep), true);
    drag = Drag::None;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

// ── KeyGutter ────────────────────────────────────────────────────────────────

void PianoRollEditor::KeyGutter::paint(juce::Graphics& g)
{
    auto& ed = owner;
    // Dark well behind the keys (matches classic piano-roll chrome).
    g.setColour(ds::rollchrome());
    g.fillAll();

    const int viewY = ed.rollViewport.getViewPositionY();
    const float rowH = ed.effRowH();
    const float W = (float) getWidth() - 1.0f;

    std::set<int> selectedPitches;
    for (const auto& n : ed.resolved.notes)
        if (ed.selection.count(n.id) > 0)
            selectedPitches.insert(n.pitch);

    const int firstRow = juce::jmax(0, (int) std::floor((float) viewY / rowH));
    const int lastRow = juce::jmin(ed.numRows() - 1,
                                   (int) std::ceil((float) (viewY + getHeight()) / rowH));

    // Pass 1 — flat white keys (reference: minimal, no loud bevels).
    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int pitch = ed.pitchForRow(row);
        if (isBlackKeyPitch(pitch)) continue;

        const float y = (float) row * rowH - (float) viewY;
        auto key = juce::Rectangle<float>(0.0f, y, W, rowH);
        g.setColour(colours::kbWhite());
        g.fillRect(key);

        if (!ed.edit.isNoteFilterEnabled(pitch))
        {
            g.setColour(ds::trk().withAlpha(0.55f));
            g.fillRect(key);
        }
        if (selectedPitches.count(pitch) > 0)
        {
            g.setColour(colours::accentSoft());
            g.fillRect(key);
            g.setColour(colours::accent());
            g.fillRect(W - 2.0f, y, 2.0f, rowH);
        }

        g.setColour(contrastLineOn(colours::kbWhite()));
        g.fillRect(0.0f, y + rowH - 0.5f, W, 0.5f);
    }

    // Pass 2 — flat black keys (shorter).
    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int pitch = ed.pitchForRow(row);
        if (!isBlackKeyPitch(pitch)) continue;

        const float y = (float) row * rowH - (float) viewY;
        const float keyW = W * 0.62f;
        auto key = juce::Rectangle<float>(0.0f, y + 0.5f, keyW, rowH - 1.0f);
        g.setColour(colours::kbBlack());
        g.fillRoundedRectangle(key, juce::jmin(2.0f, rowH * 0.25f));

        if (!ed.edit.isNoteFilterEnabled(pitch))
        {
            g.setColour(ds::tx3());
            g.fillRoundedRectangle(key, juce::jmin(2.0f, rowH * 0.25f));
        }

        if (selectedPitches.count(pitch) > 0)
        {
            g.setColour(colours::accent());
            g.drawRoundedRectangle(key.reduced(0.5f), juce::jmin(2.0f, rowH * 0.25f), 1.2f);
        }
    }

    // Pass 3 — rotated note labels (scale with row height).
    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int pitch = ed.pitchForRow(row);
        const bool isC = (pitch % 12 == 0);
        if (!(ed.folded || isC) || rowH < 7.0f) continue;

        const float y = (float) row * rowH - (float) viewY;
        const float fontPt = juce::jlimit(7.0f, 11.0f, rowH * 0.72f);
        const bool black = isBlackKeyPitch(pitch);
        const bool selected = selectedPitches.count(pitch) > 0;
        const auto name = pitchName(pitch);

        const float cx = black ? W * 0.32f : W - 8.0f;
        const float cy = y + rowH * 0.5f;

        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, cx, cy));
        g.setFont(monoFont(fontPt, true));
        g.setColour(selected ? colours::accent()
                   : black ? contrastInkOn(colours::kbBlack())
                           : contrastInkOn(colours::kbWhite()));
        g.drawText(name,
                   juce::Rectangle<float>(cx - rowH * 0.5f, cy - fontPt * 0.55f,
                                          rowH, fontPt * 1.2f).toNearestInt(),
                   juce::Justification::centred, false);
        g.restoreState();
    }

    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromRight(1));
}

void PianoRollEditor::KeyGutter::mouseDown(const juce::MouseEvent& e)
{
    auto& ed = owner;
    if (!ed.hasClip) return;
    const int row = (int) std::floor((e.position.y + (float) ed.rollViewport.getViewPositionY())
                                     / ed.effRowH());
    if (row < 0 || row >= ed.numRows()) return;
    ed.selectPitch(ed.pitchForRow(row), e.mods.isShiftDown());
}

// ── VelocityLane ─────────────────────────────────────────────────────────────

void PianoRollEditor::VelocityLane::paint(juce::Graphics& g)
{
    auto& ed = owner;
    g.fillAll(ds::surface100());
    g.setColour(ds::line());
    g.fillRect(getLocalBounds().removeFromTop(1));

    auto header = getLocalBounds().removeFromTop(headerH).reduced(8, 0);
    auto caret = header.removeFromLeft(12).toFloat().withSizeKeepingCentre(9.0f, 9.0f);
    fx::drawStrokedChevron(g, caret, ds::inkMuted(), ed.velocityOpen);
    header.removeFromLeft(4);
    g.setColour(contrastInkOn(ds::surface100()));
    g.setFont(uiFontFixed(11.0f, false));
    g.drawText("Velocity", header, juce::Justification::centredLeft);

    if (!ed.hasClip || !ed.velocityOpen) return;

    auto lane = getLocalBounds().withTrimmedTop(headerH);
    juce::Graphics::ScopedSaveState clipLane(g);
    g.reduceClipRegion(lane);
    const float pps = ed.pxPerStep();
    const float viewX = (float) ed.rollViewport.getViewPositionX();
    const float laneTop = (float) lane.getY();
    const float laneH = (float) juce::jmax(1, lane.getHeight());
    auto yForVel = [&](int vel)
    {
        const float t = (float) juce::jlimit(0, 127, vel) / 127.0f;
        return laneTop + (1.0f - t) * laneH;
    };

    g.setFont(monoFontFixed(10.0f));
    const int ticks[] = { 127, 96, 64, 32, 0 };
    for (int vel : ticks)
    {
        const float y = yForVel(vel);
        g.setColour(contrastLineOn(ds::surface100()).withAlpha(vel == 0 ? 0.0f : 1.0f));
        g.drawLine((float) gutterW, y, (float) getWidth(), y, 1.0f);
        auto label = juce::Rectangle<int>(0, (int) std::lround(y) - 6, gutterW - 6, 12)
                         .getIntersection(lane);
        if (label.getHeight() < 8) continue;
        g.setColour(contrastInkOn(ds::surface100()));
        g.drawText(juce::String(vel), label, juce::Justification::centredRight, false);
    }

    for (const auto& n : ed.grooved)
    {
        const float noteW = juce::jmax(4.0f, (float) (n.len * ed.timeStretch) * pps);
        const float x = (float) gutterW + ed.contentXFromStep(n.start) - viewX;
        if (x + noteW < (float) gutterW - 4.0f || x > (float) getWidth() + 4.0f) continue;
        const int vel = ed.velocityForNote(n);
        const float top = yForVel(vel);
        const float bh = juce::jmax(2.0f, (float) lane.getBottom() - top);
        // Stem + cap in the clip colour, like the preview's velocity lane.
        g.setColour(ed.noteColour.withAlpha(0.55f));
        g.fillRect(x, top, juce::jmin(noteW, 3.0f), bh);
        g.setColour(ed.noteColour);
        g.fillRect(x, top, juce::jmax(4.0f, juce::jmin(noteW, 8.0f)), 3.0f);
    }
}

void PianoRollEditor::VelocityLane::mouseDown(const juce::MouseEvent& e)
{
    if (e.y <= headerH)
    {
        owner.velocityOpen = !owner.velocityOpen;
        owner.resized();
        return;
    }
    if (!owner.hasClip || !owner.velocityOpen) return;
    if (e.x < gutterW) return;
    if (const auto* n = owner.noteAtVelocityX(e.position.x))
    {
        dragNoteId = n->id;
        owner.setVelocityFromLaneY(dragNoteId, e.position.y);
    }
}

void PianoRollEditor::VelocityLane::mouseDrag(const juce::MouseEvent& e)
{
    if (dragNoteId >= 0)
        owner.setVelocityFromLaneY(dragNoteId, e.position.y);
}

void PianoRollEditor::VelocityLane::mouseUp(const juce::MouseEvent&)
{
    dragNoteId = -1;
}

// ── ScalePanel (Bottom placement) ────────────────────────────────────────────

int PianoRollEditor::ScalePanel::idealHeight() const
{
    return open ? headerH + bodyH : headerH;
}

void PianoRollEditor::ScalePanel::setOpen(bool shouldOpen)
{
    if (open == shouldOpen) return;
    open = shouldOpen;
    owner.resized();
}

bool PianoRollEditor::ScalePanel::hitTest(int x, int y)
{
    // Only the header captures clicks (fold toggle). The body lets sibling
    // scale controls (laid out on top) receive mouse input.
    juce::ignoreUnused(x);
    return y >= 0 && y < headerH;
}

void PianoRollEditor::ScalePanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.y <= headerH)
        setOpen(!open);
}

void PianoRollEditor::ScalePanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::panel());
    g.setColour(colours::line());
    g.fillRect(getLocalBounds().removeFromTop(1));

    auto header = getLocalBounds().removeFromTop(headerH).reduced(10, 0);
    auto caret = header.removeFromLeft(12).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
    drawIcon(g, open ? icons::caretDown : icons::caretUp, caret, colours::text3(), 1.6f);
    header.removeFromLeft(6);
    g.setColour(colours::text2());
    g.setFont(uiFont(13.0f, true));
    g.drawText("SCALE", header, juce::Justification::centredLeft);
}

void PianoRollEditor::ScalePanel::resized()
{
    // Controls are siblings of this panel; PianoRollEditor::resized lays them out.
}

} // namespace pflow
