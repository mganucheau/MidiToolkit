#include "EffectsInspector.h"

namespace pflow {

using namespace fx;

EffectsInspector::EffectsInspector()
    : tempoRow("Tempo", tempoToggle)
    , extendRow("Extend", extendPopup)
    , quantizeStrengthSl("Strength", 0, 100, 0, false)
    , swing("Amount", 0, 100, 0, false)
    , pocket("Pocket", -100, 100, 0, true)
    , humanize("Humanize", 0, 100, 0, false)
    , lengthSl("Length", 25, 200, 100, false)
    , dynamicsSl("Dynamics", -100, 100, 0, true)
    , intensitySl("Intensity", 0, 200, 100, false)
    , articulationStrengthSl("Strength", 0, 100, 0, false)
    , velocityRangeSl("Velocity", 1, 127, 1, 127)
    , sustainRow("Sustain Pedal", sustainPopup)
    , complexitySl("Complexity", 0, 100, 50, true)
    , variationsSl("Variations", 0, 16, 0, false)
    , delayAmountSl("Amount", 0, 100, 0, false)
    , delayFeedbackSl("Feedback", 0, 100, 40, false)
    , octaveRangeRow("Octave Range", octaveRangePopup)
    , pitchRangeSl("Range", 0, 127, 0, 127)
    , filterTypeRow("Filter Type", filterTypePopup)
    , trimRow("Trim empty measures", trimSwitch)
    , fitRow("Fit to Scale", fitSwitch)
    , mapRow("Map to Root", mapSwitch)
{
    viewport.setViewedComponent(&body, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    // Listen only on the scroll body — never on this component itself.
    // addMouseListener(this, true) double-fired header clicks (component + listener),
    // so the Toolkit fold triangle toggled open→closed in one press (appeared broken),
    // and nested events used child-local coords that falsely hit fold-all.
    viewport.addMouseListener(this, true);

    btnReset.setComponentID("btnEffectsReset");
    btnReset.setWantsKeyboardFocus(false);
    btnReset.ghost = true;
    btnReset.iconScale = 1.0f;
    btnReset.setTooltip("Reset unlocked Toolkit sections");
    btnReset.onClick = [this] { resetUnlockedSections(); };
    addAndMakeVisible(btnReset);

    btnEffectsLock.setComponentID("btnLock");
    btnEffectsLock.setWantsKeyboardFocus(false);
    btnEffectsLock.ghost = true;
    btnReset.headerChrome = true;
    btnEffectsLock.headerChrome = true;
    btnEffectsLock.iconScale = 1.0f;
    btnEffectsLock.setTooltip("Lock all Toolkit parameters while browsing");
    btnEffectsLock.onClick = [this]
    {
        const bool allOn = (sectionLocks & toolkitLock::All) == toolkitLock::All;
        setAllSectionsLocked(!allOn);
    };
    addAndMakeVisible(btnEffectsLock);

    trimSwitch.setComponentID("btnTrim");
    trimSwitch.setTooltip("Remove empty bars from the start and end of the clip");
    trimSwitch.onClick = [this] { if (onTrimClicked) onTrimClicked(); };

    // Hover tips for inspector controls (shown when Settings → Show Tooltips is on).
    swing.setTooltip("Delay offbeat notes for a swung feel");
    pocket.setTooltip("Push or lay back timing (strong beats move less)");
    humanize.setTooltip("Add subtle deterministic timing jitter");
    lengthSl.setTooltip("Scale note durations (100% = original)");
    dynamicsSl.setTooltip("Metric accent contrast (negative inverts)");
    intensitySl.setTooltip("Scale note velocities (100% = original)");
    quantizeStrengthSl.setTooltip("How strongly notes snap to the quantize grid");
    quantizeTimePopup.setTooltip("Grid for quantize snapping");
    swingTimePopup.setTooltip("Swing subdivision (which offbeats are delayed)");
    articulationStrengthSl.setTooltip("How strongly articulation reshapes phrasing");
    velocityRangeSl.setTooltip("Compress clip velocities into this MIDI range");
    sustainPopup.setTooltip("Generate sustain-pedal automation for preview and export");
    complexitySl.setTooltip("Simplify or enrich the clip relative to its baseline complexity");
    variationsSl.setTooltip("Cycle alternate slight takes of the clip");
    delayTimePopup.setTooltip("Delay tap spacing");
    delayAmountSl.setTooltip("How loud delayed repeats are");
    delayFeedbackSl.setTooltip("How many delay repeats and how long they decay");
    extendPopup.setTooltip("Tile the clip longer so delays can evolve past the file length");
    octaveRangePopup.setTooltip("Fold or spread pitches into 1-3 octaves");
    pitchRow.stepper.setTooltip("Transpose the whole clip by semitones");
    pitchRangeSl.setTooltip("Clamp pitches into this MIDI note range");
    fitSwitch.setTooltip("Snap pitches into the selected key and mode");
    mapSwitch.setTooltip("Transpose so the clip root matches the selected key");

    tempoToggle.onChange = [this](double m)
    {
        bpmMultiplier = m;
        if (onBpmMultiplierChanged) onBpmMultiplierChanged(bpmMultiplier);
        refreshDirtySections();
    };

    auto wireSlider = [this](FlatSliderRow& s, int knobIdx)
    {
        s.onChange = [this, knobIdx](int v)
        {
            groove.set(knobIdx, v);
            notifyGroove();
        };
    };
    wireSlider(swing, 0);
    wireSlider(pocket, 1);
    wireSlider(humanize, 2);
    wireSlider(dynamicsSl, 3);
    wireSlider(lengthSl, 4);
    wireSlider(intensitySl, 5);

    quantizeStrengthSl.onChange = [this](int v)
    {
        groove.quantizeStrength = v;
        notifyGroove();
    };
    articulationStrengthSl.onChange = [this](int v)
    {
        groove.articulationStrength = v;
        groove.applyArticulationToKnobs();
        syncArticulationKnobsToUi();
        notifyGroove();
    };

    velocityRangeSl.onChange = [this](int lo, int hi)
    {
        groove.velocityRangeLo = lo;
        groove.velocityRangeHi = hi;
        velocityRangeSl.valueText = juce::String(lo) + "-" + juce::String(hi);
        notifyGroove();
    };

    variationsSl.onChange = [this](int v)
    {
        groove.variationIndex = v;
        variationsSl.valueText = v <= 0 ? "Off" : ("#" + juce::String(v));
        notifyGroove();
    };

    complexitySl.onChange = [this](int v)
    {
        const int clipCx = juce::jlimit(0, 100, clipComplexity);
        // A locked Effects section stores the number on the slider. Follow-clip
        // (-1) would retarget every file and look like the lock let go.
        if ((sectionLocks & toolkitLock::Effects) != 0 || v != clipCx)
            groove.complexityTarget = v;
        else
            groove.complexityTarget = -1;
        refreshComplexitySlider();
        notifyGroove();
    };

    delayAmountSl.onChange = [this](int v) { groove.delayAmount = v; notifyGroove(); };
    delayFeedbackSl.onChange = [this](int v) { groove.delayFeedback = v; notifyGroove(); };

    swingTimePopup.setItems({ "1/16", "1/8", "1/4", "1/2", "1", "2" }, 1);
    swingTimePopup.onChange = [this](int idx)
    {
        groove.swingGridIndex = idx;
        groove.swingBase = idx == 0 ? SwingBase::Sixteenth : SwingBase::Eighth;
        notifyGroove();
    };

    juce::StringArray qItems;
    for (int i = 0; i < (int) QuantizeGrid::Count; ++i)
        qItems.add(quantizeGridLabel((QuantizeGrid) i));
    quantizeTimePopup.setItems(qItems, (int) QuantizeGrid::Eighth);
    quantizeTimePopup.onChange = [this](int idx)
    {
        groove.quantizeGridIndex = idx;
        notifyGroove();
    };

    juce::StringArray artItems;
    for (int i = 0; i < (int) Articulation::Count; ++i)
        artItems.add(articulationLabel((Articulation) i));
    articulationGrid.title = "Articulation";
    articulationGrid.columns = 6;
    articulationGrid.multiSelect = false;
    articulationGrid.fitContent = true;
    articulationGrid.setItems(artItems);
    articulationGrid.setSelectedIndex((int) Articulation::Off, juce::dontSendNotification);
    articulationGrid.onChange = [this]
    {
        const int idx = juce::jmax(0, articulationGrid.getSelectedIndex());
        groove.articulationIndex = idx;
        if (idx == (int) Articulation::Off)
        {
            groove.articulationStrength = 0;
            articulationStrengthSl.setValue(0, juce::dontSendNotification);
            articulationStrengthSl.valueText = "0%";
        }
        groove.applyArticulationToKnobs();
        syncArticulationKnobsToUi();
        notifyGroove();
    };

    extendPopup.setItems({ "Off", "x2", "x4", "x8" }, 0);
    extendPopup.onChange = [this](int idx)
    {
        static const int kMult[] = { 1, 2, 4, 8 };
        edit.extendMult = kMult[juce::jlimit(0, 3, idx)];
        notifyEdit();
    };

    juce::StringArray susItems;
    for (int i = 0; i < (int) SustainPedalMode::Count; ++i)
        susItems.add(sustainPedalLabel((SustainPedalMode) i));
    sustainPopup.setItems(susItems, 0);
    sustainPopup.onChange = [this](int idx)
    {
        groove.sustainPedalMode = idx;
        notifyGroove();
    };

    juce::StringArray delayItems;
    for (int i = 0; i < (int) DelayTime::Count; ++i)
        delayItems.add(delayTimeLabel((DelayTime) i));
    delayTimePopup.setItems(delayItems, (int) DelayTime::Eighth);
    delayTimePopup.onChange = [this](int idx)
    {
        groove.delayTimeIndex = idx;
        notifyGroove();
    };





    octaveSelector.title = "Octave";
    {
        juce::StringArray octs;
        for (int i = 0; i <= 6; ++i) octs.add(juce::String(i));
        octaveSelector.setItems(octs, 4);
    }
    octaveSelector.onChange = [this](int idx)
    {
        // Absolute octave on the keyboard (file C4 + select 1 → C1).
        edit.octave = juce::jlimit(0, 6, idx);
        // While Pitch is locked, keep the absolute value so browse retargets
        // every clip to this octave (native/-1 would follow each file).
        if ((sectionLocks & toolkitLock::Pitch) == 0 && edit.octave == clipSourceOctave)
            edit.octave = -1; // native = clean
        refreshPitchAnnotation();
        notifyEdit();
    };

    octaveRangePopup.setItems({ "Off", "1", "2", "3" }, 0);
    octaveRangePopup.onChange = [this](int idx)
    {
        edit.octaveRange = juce::jlimit(0, 3, idx);
        notifyEdit();
    };

    auto syncPitchRangeText = [this]
    {
        pitchRangeSl.valueText = pitchName(edit.pitchMin) + "-" + pitchName(edit.pitchMax);
        pitchRangeSl.repaint();
    };
    pitchRangeSl.onChange = [this, syncPitchRangeText](int lo, int hi)
    {
        edit.pitchMin = lo;
        edit.pitchMax = hi;
        syncPitchRangeText();
        notifyEdit();
    };
    syncPitchRangeText();

    pitchRow.stepper.onChange = [this](int v)
    {
        edit.pitchShift = v;
        refreshPitchAnnotation();
        notifyEdit();
    };

    noteFilter.onChange = [this]
    {
        edit.noteFilterMask = noteFilter.getMask();
        notifyEdit();
    };

    filterTypePopup.setItems({ "Mute", "Fold" }, 0);
    filterTypePopup.setTooltip("Mute removes filtered pitches; Fold maps them into enabled ones");
    filterTypePopup.onChange = [this](int idx)
    {
        edit.noteFilterType = idx == 1 ? NoteFilterType::Fold : NoteFilterType::Mute;
        notifyEdit();
    };

    juce::StringArray keys;
    for (int i = 0; i < 12; ++i) keys.add(kNoteNames[(size_t) i]);
    keyGrid.title = "Key";
    keyGrid.columns = 6;
    keyGrid.multiSelect = false;
    keyGrid.setItems(keys);
    keyGrid.setSelectedIndex(0, juce::dontSendNotification);
    keyGrid.onChange = [this]
    {
        edit.root = keyGrid.getSelectedIndex();
        syncNoteFilterScale();
        refreshPitchAnnotation();
        notifyEdit();
    };

    juce::StringArray modes;
    for (int i = 0; i < kNumModes; ++i) modes.add(modeName((Mode) i));
    modeGrid.title = "Mode";
    modeGrid.columns = 6;
    modeGrid.multiSelect = false;
    modeGrid.fitContent = true;
    modeGrid.setItems(modes);
    modeGrid.setSelectedIndex(0, juce::dontSendNotification);
    modeGrid.onChange = [this]
    {
        edit.mode = (Mode) juce::jmax(0, modeGrid.getSelectedIndex());
        syncNoteFilterScale();
        refreshPitchAnnotation();
        notifyEdit();
    };

    fitSwitch.onClick = [this]
    {
        edit.fitScale = fitSwitch.getToggleState();
        // Key/Mode chips are always painted; keep edit in sync with what the
        // user sees so Fit never no-ops with root still at -1.
        if (edit.fitScale)
        {
            const int keyIdx = keyGrid.getSelectedIndex();
            edit.root = keyIdx >= 0 ? keyIdx : 0;
            const int modeIdx = modeGrid.getSelectedIndex();
            if (modeIdx >= 0)
                edit.mode = (Mode) modeIdx;
        }
        refreshPitchAnnotation();
        notifyEdit();
    };

    mapSwitch.onClick = [this]
    {
        edit.mapToRoot = mapSwitch.getToggleState();
        notifyEdit();
    };

    // Dirty predicates — when a section is folded, only dirty rows stay visible.
    playback.addRow(&tempoRow, kRowMinH, [this] { return std::abs(bpmMultiplier - 1.0) > 1.0e-6; });
    playback.addRow(&trimRow, kRowMinH, [this] { return trimSwitch.getToggleState(); });
    playback.addRow(&extendRow, kRowMinH, [this] { return edit.extendMult > 1; });

    timing.addRow(&quantizeBlock, [this] { return quantizeBlock.idealHeight(); }, [this] {
        return groove.quantizeStrength > 0
            || groove.quantizeGridIndex != (int) QuantizeGrid::Eighth;
    });
    timing.addRow(&swingBlock, [this] { return swingBlock.idealHeight(); }, [this] {
        return groove.swing != 0 || groove.swingGridIndex != 1;
    });
    timing.addRow(&timingDivider, [this] { return timingDivider.idealHeight(); });
    timing.addRow(&lengthSl, kSliderRowH, [this] { return groove.length != 100; });
    timing.addRow(&pocket, kSliderRowH, [this] { return groove.pocket != 0; });
    timing.addRow(&humanize, kSliderRowH, [this] { return groove.humanize != 0; });

    performance.addRow(&articulationGrid, [this] { return articulationGrid.idealHeight(); },
                       [this] { return groove.articulationIndex != (int) Articulation::Off; });
    performance.addRow(&articulationStrengthSl, kSliderRowH, [this] {
        return groove.articulationIndex != (int) Articulation::Off && groove.articulationStrength > 0;
    });
    performance.addRow(&performanceDivider, [this] { return performanceDivider.idealHeight(); });
    performance.addRow(&dynamicsSl, kSliderRowH, [this] { return groove.dynamics != 0; });
    performance.addRow(&intensitySl, kSliderRowH, [this] { return groove.intensity != 100; });
    performance.addRow(&velocityRangeSl, [this] { return velocityRangeSl.idealHeight(); }, [this] {
        return groove.velocityRangeLo > 1 || groove.velocityRangeHi < 127;
    });
    performance.addRow(&sustainRow, kRowMinH, [this] {
        return groove.sustainPedalMode != (int) SustainPedalMode::Off;
    });

    pitchSec.addRow(&octaveSelector, [this] { return octaveSelector.idealHeight(); },
                    [this] { return edit.octave >= 0; });
    pitchSec.addRow(&pitchRow, kRowMinH, [this] { return edit.pitchShift != 0; });
    pitchSec.addRow(&octaveRangeRow, kRowMinH, [this] { return edit.octaveRange != 0; });
    pitchSec.addRow(&pitchDividerA, [this] { return pitchDividerA.idealHeight(); });
    pitchSec.addRow(&keyGrid, [this] { return keyGrid.idealHeight(); },
                    [this] { return edit.root >= 0 && (edit.fitScale || edit.mapToRoot); });
    pitchSec.addRow(&modeGrid, [this] { return modeGrid.idealHeight(); },
                    [this] { return edit.root >= 0 && (edit.fitScale || edit.mapToRoot); });
    pitchSec.addRow(&fitRow, kRowMinH, [this] { return edit.fitScale; });
    pitchSec.addRow(&mapRow, kRowMinH, [this] { return edit.mapToRoot; });
    pitchSec.addRow(&pitchDividerB, [this] { return pitchDividerB.idealHeight(); });
    pitchSec.addRow(&noteFilter, [this] { return noteFilter.idealHeight(); },
                    [this] { return edit.hasNoteFilter(); });
    pitchSec.addRow(&pitchRangeSl, [this] { return pitchRangeSl.idealHeight(); },
                    [this] { return edit.hasPitchRange(); });
    pitchSec.addRow(&filterTypeRow, kRowMinH, [this] { return edit.hasNoteFilter(); });

    effectsSec.addRow(&complexitySl, kSliderRowH, [this] { return groove.complexityTarget >= 0; });
    effectsSec.addRow(&variationsSl, kSliderRowH, [this] { return groove.variationIndex > 0; });
    effectsSec.addRow(&effectsDivider, [this] { return effectsDivider.idealHeight(); });
    effectsSec.addRow(&delayBlock, [this] { return delayBlock.idealHeight(); }, [this] {
        return groove.delayAmount > 0;
    });
    effectsSec.addRow(&delayFeedbackSl, kSliderRowH, [this] {
        return groove.delayAmount > 0 && groove.delayFeedback != 40;
    });

    playback.onToggle = timing.onToggle = performance.onToggle = pitchSec.onToggle
        = effectsSec.onToggle = [this] { layoutSections(); };

    playback.onReset = [this] { resetPlaybackSection(); };
    playback.onLockToggle = [this] { toggleSectionLock(toolkitLock::Playback); };

    timing.onReset = [this] { resetTimingSection(); };
    timing.onLockToggle = [this] { toggleSectionLock(toolkitLock::Timing); };

    performance.onReset = [this] { resetPerformanceSection(); };
    performance.onLockToggle = [this] { toggleSectionLock(toolkitLock::Performance); };

    pitchSec.onReset = [this]
    {
        resetPitchEditFields();
        setEdit(edit, juce::dontSendNotification);
        notifyEdit();
    };
    pitchSec.onLockToggle = [this] { toggleSectionLock(toolkitLock::Pitch); };

    effectsSec.onReset = [this] { resetEffectsSection(); };
    effectsSec.onLockToggle = [this] { toggleSectionLock(toolkitLock::Effects); };

    for (auto* sec : { &playback, &timing, &performance, &pitchSec, &effectsSec })
        body.addAndMakeVisible(*sec);

    setGroove(GrooveParams{}, juce::dontSendNotification);
    setEdit(ClipEdit{}, juce::dontSendNotification);
    setBpmMultiplier(1.0, juce::dontSendNotification);
}

void EffectsInspector::resetPitchEditFields()
{
    edit.octave = -1;
    edit.pitchShift = 0;
    edit.octaveRange = 0;
    edit.pitchMin = 0;
    edit.pitchMax = 127;
    edit.extendMult = 1;
    edit.fitScale = false;
    edit.mapToRoot = false;
    edit.root = -1;
    edit.mode = Mode::Ionian;
    edit.noteFilterMask = 0x0FFF;
    edit.noteFilterType = NoteFilterType::Mute;
}

void EffectsInspector::resetPlaybackSection()
{
    setBpmMultiplier(1.0, juce::dontSendNotification);
    edit.extendMult = 1;
    setEdit(edit, juce::dontSendNotification);
    if (onBpmMultiplierChanged) onBpmMultiplierChanged(bpmMultiplier);
    notifyEdit();
}

void EffectsInspector::resetTimingSection()
{
    groove.swing = 0;
    groove.pocket = 0;
    groove.humanize = 0;
    groove.length = 100;
    groove.swingGridIndex = 1;
    groove.quantizeStrength = 0;
    groove.quantizeGridIndex = (int) QuantizeGrid::Eighth;
    setGroove(groove, juce::dontSendNotification);
    notifyGroove();
}

void EffectsInspector::resetPerformanceSection()
{
    groove.dynamics = 0;
    groove.intensity = 100;
    groove.articulationIndex = (int) Articulation::Off;
    groove.articulationStrength = 0;
    groove.velocityRangeLo = 1;
    groove.velocityRangeHi = 127;
    groove.sustainPedalMode = (int) SustainPedalMode::Off;
    groove.applyArticulationToKnobs();
    setGroove(groove, juce::dontSendNotification);
    syncArticulationKnobsToUi();
    notifyGroove();
}

void EffectsInspector::resetEffectsSection()
{
    groove.complexityTarget = -1;
    groove.variationIndex = 0;
    groove.delayAmount = 0;
    groove.delayFeedback = 40;
    groove.delayTimeIndex = (int) DelayTime::Eighth;
    setGroove(groove, juce::dontSendNotification);
    refreshComplexitySlider();
    notifyGroove();
}

void EffectsInspector::refreshHeaderLockButton()
{
    const bool allOn = (sectionLocks & toolkitLock::All) == toolkitLock::All;
    btnEffectsLock.icon = allOn ? icons::lockClosed : icons::lockOpen;
    btnEffectsLock.active = allOn;
    btnEffectsLock.setTooltip(allOn ? "Unlock all Toolkit parameters"
                                    : "Lock all Toolkit parameters while browsing");
    btnEffectsLock.repaint();
}

void EffectsInspector::setSectionLocks(uint32_t locks)
{
    sectionLocks = locks & toolkitLock::All;
    playback.setLocked((sectionLocks & toolkitLock::Playback) != 0);
    timing.setLocked((sectionLocks & toolkitLock::Timing) != 0);
    performance.setLocked((sectionLocks & toolkitLock::Performance) != 0);
    pitchSec.setLocked((sectionLocks & toolkitLock::Pitch) != 0);
    effectsSec.setLocked((sectionLocks & toolkitLock::Effects) != 0);
    refreshHeaderLockButton();
    repaint();
}

void EffectsInspector::toggleSectionLock(uint32_t bit)
{
    bit &= toolkitLock::All;
    if (bit == 0) return;
    if ((sectionLocks & bit) != 0)
        sectionLocks &= ~bit;
    else
        sectionLocks |= bit;
    setSectionLocks(sectionLocks);
    if (onSectionLocksChanged) onSectionLocksChanged(sectionLocks);
}

void EffectsInspector::setAllSectionsLocked(bool locked)
{
    sectionLocks = locked ? toolkitLock::All : 0;
    setSectionLocks(sectionLocks);
    if (onSectionLocksChanged) onSectionLocksChanged(sectionLocks);
}

void EffectsInspector::resetUnlockedSections()
{
    if ((sectionLocks & toolkitLock::Playback) == 0)
        resetPlaybackSection();
    if ((sectionLocks & toolkitLock::Timing) == 0)
        resetTimingSection();
    if ((sectionLocks & toolkitLock::Performance) == 0)
        resetPerformanceSection();
    if ((sectionLocks & toolkitLock::Pitch) == 0)
    {
        resetPitchEditFields();
        setEdit(edit, juce::dontSendNotification);
        notifyEdit();
    }
    if ((sectionLocks & toolkitLock::Effects) == 0)
        resetEffectsSection();
    if (onResetUnlocked) onResetUnlocked();
}

void EffectsInspector::syncArticulationKnobsToUi()
{
    lengthSl.setValue(groove.length, juce::dontSendNotification);
    intensitySl.setValue(groove.intensity, juce::dontSendNotification);
    dynamicsSl.setValue(groove.dynamics, juce::dontSendNotification);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], groove.length);
    intensitySl.valueText = grooveValueText(kKnobDefs[5], groove.intensity);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], groove.dynamics);
    lengthSl.repaint();
    intensitySl.repaint();
    dynamicsSl.repaint();
}

int EffectsInspector::displayedOctave() const
{
    return edit.octave >= 0 ? juce::jlimit(0, 6, edit.octave) : juce::jlimit(0, 6, clipSourceOctave);
}

void EffectsInspector::syncNoteFilterScale()
{
    const int root = juce::jmax(0, keyGrid.getSelectedIndex());
    const auto mode = (Mode) juce::jmax(0, modeGrid.getSelectedIndex());
    noteFilter.setScaleContext(root, mode);
}

void EffectsInspector::refreshOctaveSummary()
{
    const bool native = edit.octave < 0;
    octaveSelector.summary = native
        ? ("Clip (" + juce::String(displayedOctave()) + ")")
        : juce::String(displayedOctave());
    octaveSelector.summaryDefault = native;
    octaveSelector.nativeIndex = clipSourceOctave;
    octaveSelector.repaint();
}

void EffectsInspector::refreshDependentEnables()
{
    const bool artOn = groove.articulationIndex != (int) Articulation::Off;
    articulationStrengthSl.setEnabled(artOn);
    articulationStrengthSl.setAlpha(artOn ? 1.0f : 0.38f);

    const bool delayOn = groove.delayAmount > 0;
    delayFeedbackSl.setEnabled(delayOn);
    delayFeedbackSl.setAlpha(delayOn ? 1.0f : 0.38f);

    const bool filterOn = edit.hasNoteFilter();
    filterTypeRow.setEnabled(filterOn);
    filterTypePopup.setEnabled(filterOn);
    filterTypeRow.setAlpha(filterOn ? 1.0f : 0.38f);
}

int EffectsInspector::resolvedPitchMidi() const
{
    const int refPc = edit.root >= 0 ? edit.root : (clipRootPc >= 0 ? clipRootPc : 0);
    const int oct = displayedOctave();
    int midi = juce::jlimit(0, 127, (oct + 1) * 12 + refPc + edit.pitchShift);
    if (edit.root >= 0 && edit.fitScale)
        midi = fitToScale(midi, edit.root, edit.mode);
    return midi;
}

void EffectsInspector::refreshPitchAnnotation()
{
    const int refPc = edit.root >= 0 ? edit.root : (clipRootPc >= 0 ? clipRootPc : -1);
    pitchRow.annotation = pitchScaleAnnotation(resolvedPitchMidi(), refPc, edit.mode);
    refreshOctaveSummary();
    pitchRow.repaint();
}

void EffectsInspector::refreshComplexitySlider()
{
    const int v = groove.resolvedComplexityTarget(clipComplexity);
    complexitySl.setDefault(clipComplexity);
    complexitySl.setValue(v, juce::dontSendNotification);
    const bool frozen = (sectionLocks & toolkitLock::Effects) != 0 && groove.complexityTarget >= 0;
    if (frozen)
        complexitySl.valueText = juce::String(v);
    else
    {
        const int delta = v - clipComplexity;
        complexitySl.valueText = delta == 0 ? "Clip" : signedIntText(delta);
    }
    complexitySl.repaint();
}

void EffectsInspector::refreshDirtySections()
{
    for (auto* sec : { &playback, &timing, &performance, &pitchSec, &effectsSec })
        sec->refreshDirtyRows();
    refreshDependentEnables();
    layoutSections();
}

void EffectsInspector::setHasClip(bool has)
{
    hasClip = has;
    repaint();
}

void EffectsInspector::setClipRoot(int rootPc)
{
    clipRootPc = rootPc;
    syncNoteFilterScale();
    refreshPitchAnnotation();
}

void EffectsInspector::setClipSourceOctave(int oct)
{
    clipSourceOctave = juce::jlimit(0, 6, oct);
    octaveSelector.setIndex(displayedOctave(), juce::dontSendNotification);
    refreshPitchAnnotation();
}

void EffectsInspector::setClipScaleAnalysis(const ScaleAnalysis& analysis)
{
    // Candidates only — Key/Mode selection comes from ClipEdit via setEdit.
    // Overwriting the chips here desynced the UI from edit.root/mode and made
    // Fit-to-Scale look enabled for a key that was never applied.
    candidateKeyMask = analysis.keyMask;
    candidateModeMask = analysis.modeMask;
    keyGrid.setCandidateMask(candidateKeyMask);
    modeGrid.setCandidateMask(candidateModeMask);
    syncNoteFilterScale();
    refreshPitchAnnotation();
    refreshDirtySections();
}

void EffectsInspector::setClipComplexity(int complexity)
{
    clipComplexity = juce::jlimit(0, 100, complexity);
    complexitySl.setDefault(clipComplexity);
    refreshComplexitySlider();
}

void EffectsInspector::setLiveStreamMode(bool on)
{
    if (liveStream == on) return;
    liveStream = on;
    // Complexity, variations, and the whole Playback section need a clip.
    effectsSec.setRowForcedHidden(&complexitySl, on);
    effectsSec.setRowForcedHidden(&variationsSl, on);
    effectsSec.setRowForcedHidden(&effectsDivider, on);
    layoutSections();
}

void EffectsInspector::setTrimState(bool active, bool enabled, const juce::String&)
{
    trimSwitch.setToggleState(active, juce::dontSendNotification);
    trimSwitch.setEnabled(enabled);
    trimSwitch.repaint();
    refreshDirtySections();
}

void EffectsInspector::setBpmMultiplier(double mult, juce::NotificationType)
{
    bpmMultiplier = juce::jlimit(0.25, 4.0, mult);
    tempoToggle.setMultiplier(bpmMultiplier, juce::dontSendNotification);
    refreshDirtySections();
}

void EffectsInspector::setGroove(const GrooveParams& g, juce::NotificationType)
{
    groove = g;
    swing.setValue(g.swing, juce::dontSendNotification);
    pocket.setValue(g.pocket, juce::dontSendNotification);
    humanize.setValue(g.humanize, juce::dontSendNotification);
    dynamicsSl.setValue(g.dynamics, juce::dontSendNotification);
    lengthSl.setValue(g.length, juce::dontSendNotification);
    intensitySl.setValue(g.intensity, juce::dontSendNotification);
    quantizeStrengthSl.setValue(g.quantizeStrength, juce::dontSendNotification);
    articulationStrengthSl.setValue(g.articulationStrength, juce::dontSendNotification);
    delayAmountSl.setValue(g.delayAmount, juce::dontSendNotification);
    delayFeedbackSl.setValue(g.delayFeedback, juce::dontSendNotification);
    velocityRangeSl.setRange(g.velocityRangeLo, g.velocityRangeHi, juce::dontSendNotification);
    variationsSl.setValue(g.variationIndex, juce::dontSendNotification);

    swingTimePopup.setIndex(juce::jlimit(0, 5, g.swingGridIndex), juce::dontSendNotification);
    quantizeTimePopup.setIndex(juce::jlimit(0, (int) QuantizeGrid::Count - 1, g.quantizeGridIndex),
                               juce::dontSendNotification);
    articulationGrid.setSelectedIndex(
        juce::jlimit(0, (int) Articulation::Count - 1, g.articulationIndex),
        juce::dontSendNotification);
    sustainPopup.setIndex(juce::jlimit(0, (int) SustainPedalMode::Count - 1, g.sustainPedalMode),
                          juce::dontSendNotification);
    delayTimePopup.setIndex(juce::jlimit(0, (int) DelayTime::Count - 1, g.delayTimeIndex),
                            juce::dontSendNotification);

    swing.valueText = grooveValueText(kKnobDefs[0], g.swing);
    pocket.valueText = grooveValueText(kKnobDefs[1], g.pocket);
    humanize.valueText = grooveValueText(kKnobDefs[2], g.humanize);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], g.dynamics);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], g.length);
    intensitySl.valueText = grooveValueText(kKnobDefs[5], g.intensity);
    quantizeStrengthSl.valueText = juce::String(g.quantizeStrength) + "%";
    articulationStrengthSl.valueText = juce::String(g.articulationStrength) + "%";
    delayAmountSl.valueText = juce::String(g.delayAmount) + "%";
    delayFeedbackSl.valueText = juce::String(g.delayFeedback) + "%";
    velocityRangeSl.valueText = juce::String(g.velocityRangeLo) + "-" + juce::String(g.velocityRangeHi);
    variationsSl.valueText = g.variationIndex <= 0 ? "Off" : ("#" + juce::String(g.variationIndex));
    refreshComplexitySlider();
    refreshDirtySections();
    repaint();
}

void EffectsInspector::setEdit(const ClipEdit& e, juce::NotificationType)
{
    edit = e;
    octaveSelector.setIndex(displayedOctave(), juce::dontSendNotification);
    octaveRangePopup.setIndex(juce::jlimit(0, 3, e.octaveRange), juce::dontSendNotification);
    pitchRow.stepper.setValue(e.pitchShift, juce::dontSendNotification);
    pitchRangeSl.setRange(e.pitchMin, e.pitchMax, juce::dontSendNotification);
    pitchRangeSl.valueText = pitchName(e.pitchMin) + "-" + pitchName(e.pitchMax);
    {
        int extIdx = 0;
        if (e.extendMult == 2) extIdx = 1;
        else if (e.extendMult == 4) extIdx = 2;
        else if (e.extendMult == 8) extIdx = 3;
        extendPopup.setIndex(extIdx, juce::dontSendNotification);
    }
    keyGrid.setCandidateMask(candidateKeyMask);
    modeGrid.setCandidateMask(candidateModeMask);
    keyGrid.setSelectedIndex(e.root >= 0 ? e.root : 0, juce::dontSendNotification);
    modeGrid.setSelectedIndex((int) e.mode, juce::dontSendNotification);
    fitSwitch.setToggleState(e.fitScale, juce::dontSendNotification);
    mapSwitch.setToggleState(e.mapToRoot, juce::dontSendNotification);
    noteFilter.setState(e.noteFilterMask, juce::dontSendNotification);
    filterTypePopup.setIndex(e.noteFilterType == NoteFilterType::Fold ? 1 : 0,
                             juce::dontSendNotification);
    syncNoteFilterScale();
    refreshPitchAnnotation();
    refreshDirtySections();
}

void EffectsInspector::notifyGroove()
{
    swing.valueText = grooveValueText(kKnobDefs[0], groove.swing);
    pocket.valueText = grooveValueText(kKnobDefs[1], groove.pocket);
    humanize.valueText = grooveValueText(kKnobDefs[2], groove.humanize);
    dynamicsSl.valueText = grooveValueText(kKnobDefs[3], groove.dynamics);
    lengthSl.valueText = grooveValueText(kKnobDefs[4], groove.length);
    intensitySl.valueText = grooveValueText(kKnobDefs[5], groove.intensity);
    quantizeStrengthSl.valueText = juce::String(groove.quantizeStrength) + "%";
    articulationStrengthSl.valueText = juce::String(groove.articulationStrength) + "%";
    delayAmountSl.valueText = juce::String(groove.delayAmount) + "%";
    delayFeedbackSl.valueText = juce::String(groove.delayFeedback) + "%";
    velocityRangeSl.valueText = juce::String(groove.velocityRangeLo) + "-"
                              + juce::String(groove.velocityRangeHi);
    variationsSl.valueText = groove.variationIndex <= 0 ? "Off"
                           : ("#" + juce::String(groove.variationIndex));
    if (onGrooveChanged) onGrooveChanged(groove);
    refreshDirtySections();
}

void EffectsInspector::notifyEdit()
{
    if (onEditChanged) onEditChanged(edit);
    refreshDirtySections();
}

void EffectsInspector::layoutSections()
{
    const int w = juce::jmax(1, body.getWidth());
    int y = 0;
    fx::Section* secs[] = { &playback, &timing, &performance, &pitchSec, &effectsSec };
    const int n = (int) (sizeof(secs) / sizeof(secs[0]));
    fx::Section* last = &effectsSec;
    for (int i = 0; i < n; ++i)
    {
        auto* sec = secs[i];
        if (liveStream && sec == &playback)
        {
            sec->setVisible(false);
            sec->setBounds(0, 0, 0, 0);
            continue;
        }
        sec->setVisible(true);
        last = sec;
        sec->showBottomDivider = true;
        const int h = sec->idealHeight();
        sec->setBounds(0, y, w, h);
        sec->resized();
        y += h;
    }
    if (last != nullptr)
        last->showBottomDivider = false;
    // Match header / section top pad so the last control isn't flush to the scroll edge.
    y += toolkitBodyPadB();
    body.setSize(w, juce::jmax(y, viewport.getMaximumVisibleHeight()));
    if (onContentHeightChanged != nullptr && !notifyingHeight)
    {
        notifyingHeight = true;
        onContentHeightChanged();
        notifyingHeight = false;
    }
}

int EffectsInspector::contentHeight() const
{
    int y = metrics::toolkitHeaderH();
    for (auto* sec : { &playback, &timing, &performance, &pitchSec, &effectsSec })
    {
        if (liveStream && sec == &playback) continue;
        y += sec->idealHeight();
    }
    y += toolkitBodyPadB();
    return y;
}

bool EffectsInspector::anySectionOpen() const
{
    if (!liveStream && playback.open) return true;
    return timing.open || performance.open || pitchSec.open || effectsSec.open;
}

void EffectsInspector::foldAllSections()
{
    // Collapse keeps dirty (modified) rows visible — same as per-section chevrons.
    const bool expand = !anySectionOpen();
    for (auto* sec : { &playback, &timing, &performance, &pitchSec, &effectsSec })
        sec->setOpen(expand);
    layoutSections();
    repaint();
}

void EffectsInspector::updateToolkitFoldBounds()
{
    auto header = getLocalBounds().removeFromTop(metrics::toolkitHeaderH());
    toolkitFoldBounds = { 12, (header.getHeight() - 16) / 2, 16, 16 };
}

void EffectsInspector::resized()
{
    auto r = getLocalBounds();
    const int headerH = metrics::toolkitHeaderH();
    auto header = r.removeFromTop(headerH);
    auto icons = header.removeFromRight(10 + 24 + 24);
    icons.removeFromRight(10);
    btnEffectsLock.setBounds(icons.removeFromRight(24).withSizeKeepingCentre(24, 22));
    btnReset.setBounds(icons.removeFromRight(24).withSizeKeepingCentre(24, 22));
    updateToolkitFoldBounds();

    viewport.setBounds(r);
    body.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()), body.getHeight());

    auto fitSelect = [](fx::FlatPopup& p, fx::InlineRow& row)
    {
        p.setSize(kSelectW, kControlH);
        row.setControlWidth(kSelectW);
    };
    {
        const int tempoW = kSelectHalfW;
        tempoToggle.setSize(tempoW, kControlH);
        tempoRow.setControlWidth(tempoW);
    }
    trimRow.setControlWidth(trimSwitch.idealWidth());
    fitSelect(extendPopup, extendRow);
    fitSelect(sustainPopup, sustainRow);
    fitSelect(octaveRangePopup, octaveRangeRow);
    fitSelect(filterTypePopup, filterTypeRow);

    pitchRow.stepper.setSize(pitchRow.stepper.idealWidth(), kControlH);
    fitRow.setControlWidth(fitSwitch.idealWidth());
    mapRow.setControlWidth(mapSwitch.idealWidth());

    layoutSections();
}

void EffectsInspector::paint(juce::Graphics& g)
{
    const auto& t = inspectorTokens();
    g.fillAll(ds::panel());
    g.setColour(t.divider);
    g.fillRect(getLocalBounds().removeFromLeft(1));

    auto header = getLocalBounds().removeFromTop(metrics::toolkitHeaderH());
    g.setColour(ds::head());
    g.fillRect(header);
    g.setColour(t.divider);
    g.fillRect(header.getX(), header.getBottom() - 1, header.getWidth(), 1);

    updateToolkitFoldBounds();
    auto chev = toolkitFoldBounds.toFloat().withSizeKeepingCentre(10.0f, 10.0f);
    drawStrokedChevron(g, chev, t.valueText, anySectionOpen());

    auto titleArea = header;
    titleArea.removeFromLeft(12 + 16 + 8);
    titleArea.removeFromRight(10 + 24 + 24);
    g.setColour(t.headerText);
    g.setFont(uiFontFixed(13.0f, true));
    g.drawText("Toolkit", titleArea, juce::Justification::centredLeft, false);
}

void EffectsInspector::mouseDown(const juce::MouseEvent& e)
{
    // Listener events are relative to the nested source — map into our space.
    const auto local = e.getEventRelativeTo(this).getPosition();
    updateToolkitFoldBounds();

    // Toolkit title chevron lives in the pane header (outside the viewport).
    if (local.y < metrics::toolkitHeaderH()
        && toolkitFoldBounds.expanded(8, 6).contains(local))
    {
        foldAllSections();
        if (onActivated) onActivated();
        return;
    }

    if (onActivated)
        onActivated();
}

void EffectsInspector::mouseMove(const juce::MouseEvent& e)
{
    const auto local = e.getEventRelativeTo(this).getPosition();
    updateToolkitFoldBounds();
    const bool overFold = local.y < metrics::toolkitHeaderH()
                       && toolkitFoldBounds.expanded(8, 6).contains(local);
    if (overFold)
        setTooltip("Fold all — keeps modified controls");
    setMouseCursor(overFold ? juce::MouseCursor::PointingHandCursor
                            : juce::MouseCursor::NormalCursor);
}

} // namespace pflow
