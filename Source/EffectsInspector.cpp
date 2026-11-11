#include "EffectsInspector.h"

namespace pflow {

using namespace fx;

EffectsInspector::EffectsInspector()
    : tempoRow("Tempo", tempoToggle)
    , extendRow("Extend", extendPopup)
    , reverseRow("Reverse", reverseSwitch)
    , invertRow("Invert", invertSwitch)
    , selectedNotesRow("Selected notes only", selectedNotesSwitch)
    , quantizeStrengthSl("Strength", 0, 100, 0, false)
    , swing("Amount", 0, 100, 0, false)
    , pocket("Pocket", -100, 100, 0, true)
    , humanize("Humanize", 0, 100, 0, false)
    , lengthSl("Length", 25, 200, 100, false)
    , dynamicsSl("Dynamics", -100, 100, 0, true)
    , intensitySl("Intensity", 0, 200, 100, false)
    , articulationRow("Articulation", articulationPopup)
    , articulationStrengthSl("Strength", 0, 100, 0, false)
    , velocityRangeSl("Velocity", 1, 127, 1, 127)
    , sustainRow("Sustain Pedal", sustainPopup)
    , complexitySl("Complexity", 0, 100, 50, true)
    , syncopationSl("Syncopation", 0, 100, 0, false)
    , variationsSl("Variations", 0, 16, 0, false)
    , delayAmountSl("Amount", 0, 100, 0, false)
    , delayFeedbackSl("Feedback", 0, 100, 40, false)
    , arpGenerateRow("Generate", arpGenerateBtn)
    , arpInputRow("Input", arpInputSwitch)
    , arpLengthRow("Length", arpLengthPopup)
    , arpFormRow("Form", arpFormPopup)
    , arpShapeRow("Type", arpShapePopup)
    , arpRateRow("Rate", arpRatePopup)
    , arpDensitySl("Density", 0, 100, 55, false)
    , arpIntensitySl("Intensity", 0, 100, 70, false)
    , arpDynamicsSl("Dynamics", 0, 100, 55, false)
    , arpGateSl("Gate", 10, 100, 70, false)
    , melodyGenerateRow("Generate", melodyGenerateBtn)
    , melodyInputRow("Input", melodyInputSwitch)
    , melodyLengthRow("Length", melodyLengthPopup)
    , melodyFormRow("Form", melodyFormPopup)
    , melodyStyleRow("Style", melodyStylePopup)
    , melodyContourRow("Contour", melodyContourPopup)
    , melodySpeedRow("Speed", melodySpeedPopup)
    , chordsGenerateRow("Generate", chordsGenerateBtn)
    , chordsInputRow("Input", chordsInputSwitch)
    , chordLengthRow("Length", chordLengthPopup)
    , chordFormRow("Form", chordFormPopup)
    , chordVoicingRow("Voicing", chordVoicingPopup)
    , chordColorRow("Color", chordColorPopup)
    , chordRhythmRow("Rhythm", chordRhythmPopup)
    , bassGenerateRow("Generate", bassGenerateBtn)
    , bassInputRow("Input", bassInputSwitch)
    , bassLengthRow("Length", bassLengthPopup)
    , bassFormRow("Form", bassFormPopup)
    , bassMotionRow("Motion", bassMotionPopup)
    , drumGenerateRow("Generate", drumGenerateBtn)
    , drumStyleRow("Style", drumStylePopup)
    , drumLengthRow("Length", drumLengthPopup)
    , drumFormRow("Form", drumFormPopup)
    , drumDensitySl("Density", 0, 100, 50, false)
    , drumFillSl("Fill", 0, 100, 35, false)
    , octaveRangeRow("Octave Range", octaveRangePopup)
    , outsideRangeRow("Outside Range", outsideRangePopup)
    , pitchInputRow("Input", pitchInputSwitch)
    , modeRow("Mode", modePopup)
    , pitchRangeSl("Range", 0, 127, 0, 127)
    , filterTypeRow("Filter Type", filterTypePopup)
    , trimRow("Trim", trimSwitch)
    , fitRow("Fit to Scale", fitSwitch)
    , mapRow("Map to Root", mapSwitch)
{
    viewport.setViewedComponent(&body, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(10);
    addAndMakeVisible(viewport);
    // Listen only on the scroll body — never on this component itself.
    // addMouseListener(this, true) double-fired header clicks (component + listener),
    // so the Toolkit fold triangle toggled open→closed in one press (appeared broken),
    // and nested events used child-local coords that falsely hit fold-all.
    viewport.addMouseListener(this, true);

    btnReset.setComponentID("btnEffectsReset");
    btnReset.setWantsKeyboardFocus(false);
    btnReset.ghost = false;
    btnReset.iconScale = 1.0f;
    btnReset.setTooltip("Reset unlocked Toolkit sections");
    btnReset.onClick = [this] { resetUnlockedSections(); };
    addAndMakeVisible(btnReset);

    btnEffectsLock.setComponentID("btnLock");
    btnEffectsLock.setWantsKeyboardFocus(false);
    btnEffectsLock.ghost = false;
    btnEffectsLock.iconScale = 1.0f;
    btnEffectsLock.setTooltip("Lock all Toolkit parameters while browsing");
    btnEffectsLock.onClick = [this]
    {
        const bool allOn = (sectionLocks & toolkitLock::All) == toolkitLock::All;
        setAllSectionsLocked(!allOn);
    };
    addAndMakeVisible(btnEffectsLock);

    trimSwitch.setComponentID("btnTrim");
    trimSwitch.setTooltip("Remove empty bars from the clip");
    trimSwitch.onClick = [this] { if (onTrimClicked) onTrimClicked(); };

    reverseSwitch.setComponentID("btnReverse");
    reverseSwitch.setTooltip("Play the clip backwards in time");
    reverseSwitch.onClick = [this]
    {
        edit.reverse = reverseSwitch.getToggleState();
        notifyEdit();
        refreshDirtySections();
    };

    invertSwitch.setComponentID("btnInvert");
    invertSwitch.setTooltip("Mirror pitches around the midpoint of the clip");
    invertSwitch.onClick = [this]
    {
        edit.invert = invertSwitch.getToggleState();
        notifyEdit();
        refreshDirtySections();
    };

    selectedNotesSwitch.setComponentID("btnSelectedNotesOnly");
    selectedNotesSwitch.setTooltip("Apply Invert / Reverse only to notes selected in the piano roll");
    selectedNotesSwitch.onClick = [this]
    {
        edit.selectedNotesOnly = selectedNotesSwitch.getToggleState();
        notifyEdit();
        refreshDirtySections();
    };

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
    syncopationSl.setTooltip("Pull offbeat attacks earlier for a more syncopated feel");
    variationsSl.setTooltip("Cycle alternate slight takes of the clip");
    delayTimePopup.setTooltip("Delay tap spacing");
    delayAmountSl.setTooltip("How loud delayed repeats are");
    delayFeedbackSl.setTooltip("How many delay repeats and how long they decay");
    extendPopup.setTooltip("Tile the clip longer so delays can evolve past the file length");
    octaveRangePopup.setTooltip("Keep notes inside this many octaves, starting at the target C");
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

    syncopationSl.onChange = [this](int v)
    {
        groove.syncopation = juce::jlimit(0, 100, v);
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
    articulationPopup.setItems(artItems, (int) Articulation::Off);
    articulationPopup.onChange = [this](int idx)
    {
        idx = juce::jmax(0, idx);
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





    octaveRow.label = "Octave";
    octaveRow.stepper.minV = 0;
    octaveRow.stepper.maxV = 6;
    octaveRow.stepper.format = [](int v) { return juce::String(v); };
    octaveRow.stepper.setTooltip("Target octave");
    octaveRow.stepper.onChange = [this](int v)
    {
        // Absolute octave on the keyboard (file C4 + select 1 → C1).
        edit.octave = juce::jlimit(0, 6, v);
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

    outsideRangePopup.setItems({ "Clip", "Fold" }, 1);
    outsideRangePopup.setTooltip("Clip clamps notes to the window. Fold wraps them into it");
    outsideRangePopup.onChange = [this](int idx)
    {
        edit.outsideRange = idx == 0 ? OutsideRangeMode::Clip : OutsideRangeMode::Fold;
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
    modePopup.setItems(modes, 0);
    modePopup.onChange = [this](int idx)
    {
        edit.mode = (Mode) juce::jmax(0, idx);
        // Selecting a mode commits the visible key so Hide Unused keeps both.
        if (edit.root < 0)
        {
            const int keyIdx = keyGrid.getSelectedIndex();
            edit.root = keyIdx >= 0 ? keyIdx : 0;
        }
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
            const int modeIdx = modePopup.getIndex();
            if (modeIdx >= 0)
                edit.mode = (Mode) modeIdx;
        }
        refreshPitchAnnotation();
        notifyEdit();
    };

    mapSwitch.onClick = [this]
    {
        edit.mapToRoot = mapSwitch.getToggleState();
        if (edit.mapToRoot && edit.root < 0)
        {
            const int keyIdx = keyGrid.getSelectedIndex();
            edit.root = keyIdx >= 0 ? keyIdx : 0;
        }
        notifyEdit();
    };

    // Dirty predicates — when a section is folded, only dirty rows stay visible.
    playback.addRow(&trimRow, kRowMinH, [this] { return trimSwitch.getToggleState(); });
    playback.addRow(&tempoRow, kRowMinH, [this] { return std::abs(bpmMultiplier - 1.0) > 1.0e-6; });
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

    performance.addRow(&articulationRow, kRowMinH,
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

    pitchSec.addRow(&octaveRow, kRowMinH, [this] { return edit.octave >= 0; });
    pitchSec.addRow(&pitchRow, kRowMinH, [this] { return edit.pitchShift != 0; });
    pitchInputSwitch.setTooltip("Keyboard sets the root for browsing and for generators with Input off");
    pitchInputSwitch.onClick = [this]
    {
        edit.pitchInput = pitchInputSwitch.getToggleState();
        notifyEdit();
    };
    pitchSec.addRow(&pitchInputRow, kRowMinH, [this] { return edit.pitchInput; });
    pitchSec.addRow(&octaveRangeRow, kRowMinH, [this] { return edit.octaveRange != 0; });
    pitchSec.addRow(&outsideRangeRow, kRowMinH, [this] { return edit.octaveRange != 0; });
    pitchSec.addRow(&pitchDividerA, [this] { return pitchDividerA.idealHeight(); });
    pitchSec.addRow(&keyGrid, [this] { return keyGrid.idealHeight(); },
                    [this] { return edit.root >= 0; });
    pitchSec.addRow(&modeRow, kRowMinH,
                    [this] { return edit.root >= 0 || edit.mode != Mode::Ionian; });
    pitchSec.addRow(&fitRow, kRowMinH, [this] { return edit.fitScale; });
    pitchSec.addRow(&mapRow, kRowMinH, [this] { return edit.mapToRoot; });
    pitchSec.addRow(&pitchDividerB, [this] { return pitchDividerB.idealHeight(); });
    pitchSec.addRow(&noteFilter, [this] { return noteFilter.idealHeight(); },
                    [this] { return edit.hasNoteFilter(); });
    pitchSec.addRow(&pitchRangeSl, [this] { return pitchRangeSl.idealHeight(); },
                    [this] { return edit.hasPitchRange(); });
    pitchSec.addRow(&filterTypeRow, kRowMinH, [this] { return edit.hasNoteFilter(); });

    formSec.addRow(&complexitySl, kSliderRowH, [this] { return groove.complexityTarget >= 0; });
    formSec.addRow(&syncopationSl, kSliderRowH, [this] { return groove.syncopation > 0; });
    formSec.addRow(&variationsSl, kSliderRowH, [this] { return groove.variationIndex > 0; });
    formSec.addRow(&reverseRow, kRowMinH, [this] { return edit.reverse; });
    formSec.addRow(&invertRow, kRowMinH, [this] { return edit.invert; });
    formSec.addRow(&selectedNotesRow, kRowMinH, [this] { return edit.selectedNotesOnly; });

    effectsSec.addRow(&delayBlock, [this] { return delayBlock.idealHeight(); }, [this] {
        return groove.delayAmount > 0;
    });
    effectsSec.addRow(&delayFeedbackSl, kSliderRowH, [this] {
        return groove.delayAmount > 0 && groove.delayFeedback != 40;
    });

    {
        auto fillLengthItems = [](fx::FlatPopup& popup, int selected)
        {
            juce::StringArray items;
            for (int i = 0; i < (int) GeneratorLength::Count; ++i)
                items.add(generatorLengthLabel((GeneratorLength) i));
            popup.setItems(items, selected);
        };
        auto fillFormItems = [](fx::FlatPopup& popup, int selected)
        {
            juce::StringArray items;
            for (int i = 0; i < (int) GeneratorForm::Count; ++i)
                items.add(generatorFormLabel((GeneratorForm) i));
            popup.setItems(items, selected);
        };

        juce::StringArray shapes;
        for (int i = 0; i < (int) ArpShape::Count; ++i)
            shapes.add(arpShapeLabel((ArpShape) i));
        arpShapePopup.setItems(shapes, 0);
        arpShapePopup.setTooltip("Morphs phrasing from open intervals toward classic lines");
        arpShapePopup.onChange = [this](int idx)
        {
            groove.arpShapeIndex = juce::jlimit(0, (int) ArpShape::Count - 1, idx);
            notifyGroove();
        };
        juce::StringArray rates;
        for (int i = 0; i < (int) DelayTime::Count; ++i)
            rates.add(delayTimeLabel((DelayTime) i));
        arpRatePopup.setItems(rates, (int) DelayTime::Sixteenth);
        arpRatePopup.onChange = [this](int idx)
        {
            groove.arpRateIndex = idx;
            notifyGroove();
        };
        fillLengthItems(arpLengthPopup, (int) GeneratorLength::FourBars);
        arpLengthPopup.onChange = [this](int idx)
        {
            groove.arpLengthIndex = juce::jlimit(0, (int) GeneratorLength::Count - 1, idx);
            notifyGroove();
        };
        fillFormItems(arpFormPopup, (int) GeneratorForm::Auto);
        arpFormPopup.onChange = [this](int idx)
        {
            groove.arpFormIndex = juce::jlimit(0, (int) GeneratorForm::Count - 1, idx);
            notifyGroove();
        };
        arpDensitySl.onChange = [this](int v)
        {
            groove.arpDensity = v;
            arpDensitySl.valueText = juce::String(v) + "%";
            notifyGroove();
        };
        arpIntensitySl.onChange = [this](int v)
        {
            groove.arpIntensity = v;
            arpIntensitySl.valueText = juce::String(v) + "%";
            notifyGroove();
        };
        arpDynamicsSl.onChange = [this](int v)
        {
            groove.arpDynamics = v;
            arpDynamicsSl.valueText = juce::String(v) + "%";
            notifyGroove();
        };
        arpGateSl.onChange = [this](int v)
        {
            groove.arpGate = v;
            arpGateSl.valueText = juce::String(v) + "%";
            notifyGroove();
        };
        arpInputSwitch.onClick = [this]
        {
            groove.arpInput = arpInputSwitch.getToggleState();
            notifyGroove();
        };
        arpGenerateBtn.setTooltip("Roll a new pattern into the piano roll");
        arpGenerateBtn.onClick = [this]
        {
            groove.arpEnabled = true;
            groove.melodyEnabled = groove.chordsEnabled = groove.bassEnabled = groove.drumsEnabled = false;
            groove.arpVariation = groove.arpVariation >= 16 ? 1 : groove.arpVariation + 1;
            ++groove.arpNonce;
            notifyGroove();
            if (onGeneratorGenerated) onGeneratorGenerated(0);
        };
        arpSec.addRow(&arpGenerateRow, kRowMinH, [this] { return groove.arpEnabled; });
        arpSec.addRow(&arpInputRow, kRowMinH, [this] { return groove.arpInput; });
        arpSec.addRow(&arpLengthRow, kRowMinH, [this] { return groove.arpEnabled; });
        arpSec.addRow(&arpFormRow, kRowMinH, [this] {
            // Visible when folded if Form is non-Auto or length is long enough for tiling.
            return groove.arpEnabled
                && (groove.arpFormIndex != (int) GeneratorForm::Auto
                    || groove.arpLengthIndex >= (int) GeneratorLength::FourBars);
        });
        arpSec.addRow(&arpShapeRow, kRowMinH, [this] { return groove.arpEnabled; });
        arpSec.addRow(&arpRateRow, kRowMinH, [this] { return groove.arpEnabled; });
        arpSec.addRow(&arpDensitySl, kSliderRowH, [this] {
            return groove.arpEnabled && groove.arpDensity != 55;
        });
        arpSec.addRow(&arpIntensitySl, kSliderRowH, [this] {
            return groove.arpEnabled && groove.arpIntensity != 70;
        });
        arpSec.addRow(&arpDynamicsSl, kSliderRowH, [this] {
            return groove.arpEnabled && groove.arpDynamics != 55;
        });
        arpSec.addRow(&arpGateSl, kSliderRowH, [this] {
            return groove.arpEnabled && groove.arpGate != 70;
        });

        juce::StringArray styles;
        for (int i = 0; i < (int) MelodyStyle::Count; ++i)
            styles.add(melodyStyleLabel((MelodyStyle) i));
        melodyStylePopup.setItems(styles, 0);
        melodyStylePopup.onChange = [this](int idx)
        {
            groove.melodyStyleIndex = idx;
            notifyGroove();
        };
        fillLengthItems(melodyLengthPopup, (int) GeneratorLength::FourBars);
        melodyLengthPopup.onChange = [this](int idx)
        {
            groove.melodyLengthIndex = juce::jlimit(0, (int) GeneratorLength::Count - 1, idx);
            notifyGroove();
        };
        fillFormItems(melodyFormPopup, (int) GeneratorForm::Auto);
        melodyFormPopup.onChange = [this](int idx)
        {
            groove.melodyFormIndex = juce::jlimit(0, (int) GeneratorForm::Count - 1, idx);
            notifyGroove();
        };
        juce::StringArray contours;
        for (int i = 0; i < (int) MelodyContour::Count; ++i)
            contours.add(melodyContourLabel((MelodyContour) i));
        melodyContourPopup.setItems(contours, 0);
        melodyContourPopup.onChange = [this](int idx)
        {
            groove.melodyContourIndex = idx;
            notifyGroove();
        };
        juce::StringArray speeds;
        for (int i = 0; i < (int) MelodySpeed::Count; ++i)
            speeds.add(melodySpeedLabel((MelodySpeed) i));
        melodySpeedPopup.setItems(speeds, (int) MelodySpeed::Medium);
        melodySpeedPopup.onChange = [this](int idx)
        {
            groove.melodySpeedIndex = idx;
            notifyGroove();
        };
        melodyInputSwitch.onClick = [this]
        {
            groove.melodyInput = melodyInputSwitch.getToggleState();
            notifyGroove();
        };
        melodyGenerateBtn.setTooltip("Roll a new melody into the piano roll");
        melodyGenerateBtn.onClick = [this]
        {
            groove.melodyEnabled = true;
            groove.arpEnabled = groove.chordsEnabled = groove.bassEnabled = groove.drumsEnabled = false;
            groove.melodyVariation = groove.melodyVariation >= 16 ? 1 : groove.melodyVariation + 1;
            ++groove.melodyNonce;
            notifyGroove();
            if (onGeneratorGenerated) onGeneratorGenerated(1);
        };
        melodySec.addRow(&melodyGenerateRow, kRowMinH, [this] { return groove.melodyEnabled; });
        melodySec.addRow(&melodyInputRow, kRowMinH, [this] { return groove.melodyInput; });
        melodySec.addRow(&melodyLengthRow, kRowMinH, [this] { return groove.melodyEnabled; });
        melodySec.addRow(&melodyFormRow, kRowMinH, [this] {
            return groove.melodyEnabled
                && (groove.melodyFormIndex != (int) GeneratorForm::Auto
                    || groove.melodyLengthIndex >= (int) GeneratorLength::FourBars);
        });
        melodySec.addRow(&melodyStyleRow, kRowMinH, [this] { return groove.melodyEnabled; });
        melodySec.addRow(&melodyContourRow, kRowMinH, [this] { return groove.melodyEnabled; });
        melodySec.addRow(&melodySpeedRow, kRowMinH, [this] { return groove.melodyEnabled; });

        juce::StringArray voicings;
        for (int i = 0; i < (int) ChordVoicing::Count; ++i)
            voicings.add(chordVoicingLabel((ChordVoicing) i));
        chordVoicingPopup.setItems(voicings, 0);
        chordVoicingPopup.onChange = [this](int idx)
        {
            groove.chordVoicingIndex = idx;
            notifyGroove();
        };
        juce::StringArray colors;
        for (int i = 0; i < (int) ChordColor::Count; ++i)
            colors.add(chordColorLabel((ChordColor) i));
        chordColorPopup.setItems(colors, 0);
        chordColorPopup.onChange = [this](int idx)
        {
            groove.chordColorIndex = idx;
            notifyGroove();
        };
        juce::StringArray rhythms;
        for (int i = 0; i < (int) ChordRhythm::Count; ++i)
            rhythms.add(chordRhythmLabel((ChordRhythm) i));
        chordRhythmPopup.setItems(rhythms, 0);
        chordRhythmPopup.onChange = [this](int idx)
        {
            groove.chordRhythmIndex = idx;
            notifyGroove();
        };
        fillLengthItems(chordLengthPopup, (int) GeneratorLength::FourBars);
        chordLengthPopup.onChange = [this](int idx)
        {
            groove.chordLengthIndex = juce::jlimit(0, (int) GeneratorLength::Count - 1, idx);
            notifyGroove();
        };
        fillFormItems(chordFormPopup, (int) GeneratorForm::Auto);
        chordFormPopup.onChange = [this](int idx)
        {
            groove.chordFormIndex = juce::jlimit(0, (int) GeneratorForm::Count - 1, idx);
            notifyGroove();
        };
        chordsInputSwitch.onClick = [this]
        {
            groove.chordsInput = chordsInputSwitch.getToggleState();
            notifyGroove();
        };
        chordsGenerateBtn.setTooltip("Roll a new chord pattern into the piano roll");
        chordsGenerateBtn.onClick = [this]
        {
            groove.chordsEnabled = true;
            groove.arpEnabled = groove.melodyEnabled = groove.bassEnabled = groove.drumsEnabled = false;
            groove.chordVariation = groove.chordVariation >= 16 ? 1 : groove.chordVariation + 1;
            ++groove.chordNonce;
            notifyGroove();
            if (onGeneratorGenerated) onGeneratorGenerated(2);
        };
        chordsSec.addRow(&chordsGenerateRow, kRowMinH, [this] { return groove.chordsEnabled; });
        chordsSec.addRow(&chordsInputRow, kRowMinH, [this] { return groove.chordsInput; });
        chordsSec.addRow(&chordLengthRow, kRowMinH, [this] { return groove.chordsEnabled; });
        chordsSec.addRow(&chordFormRow, kRowMinH, [this] {
            return groove.chordsEnabled
                && (groove.chordFormIndex != (int) GeneratorForm::Auto
                    || groove.chordLengthIndex >= (int) GeneratorLength::FourBars);
        });
        chordsSec.addRow(&chordVoicingRow, kRowMinH, [this] { return groove.chordsEnabled; });
        chordsSec.addRow(&chordColorRow, kRowMinH, [this] { return groove.chordsEnabled; });
        chordsSec.addRow(&chordRhythmRow, kRowMinH, [this] { return groove.chordsEnabled; });

        juce::StringArray motions;
        for (int i = 0; i < (int) BassMotion::Count; ++i)
            motions.add(bassMotionLabel((BassMotion) i));
        bassMotionPopup.setItems(motions, 0);
        bassMotionPopup.onChange = [this](int idx)
        {
            groove.bassMotionIndex = idx;
            notifyGroove();
        };
        fillLengthItems(bassLengthPopup, (int) GeneratorLength::FourBars);
        bassLengthPopup.onChange = [this](int idx)
        {
            groove.bassLengthIndex = juce::jlimit(0, (int) GeneratorLength::Count - 1, idx);
            notifyGroove();
        };
        fillFormItems(bassFormPopup, (int) GeneratorForm::Auto);
        bassFormPopup.onChange = [this](int idx)
        {
            groove.bassFormIndex = juce::jlimit(0, (int) GeneratorForm::Count - 1, idx);
            notifyGroove();
        };
        bassInputSwitch.onClick = [this]
        {
            groove.bassInput = bassInputSwitch.getToggleState();
            notifyGroove();
        };
        bassGenerateBtn.setTooltip("Roll a new bass pattern into the piano roll");
        bassGenerateBtn.onClick = [this]
        {
            groove.bassEnabled = true;
            groove.arpEnabled = groove.melodyEnabled = groove.chordsEnabled = groove.drumsEnabled = false;
            groove.bassVariation = groove.bassVariation >= 16 ? 1 : groove.bassVariation + 1;
            ++groove.bassNonce;
            notifyGroove();
            if (onGeneratorGenerated) onGeneratorGenerated(3);
        };
        bassSec.addRow(&bassGenerateRow, kRowMinH, [this] { return groove.bassEnabled; });
        bassSec.addRow(&bassInputRow, kRowMinH, [this] { return groove.bassInput; });
        bassSec.addRow(&bassLengthRow, kRowMinH, [this] { return groove.bassEnabled; });
        bassSec.addRow(&bassFormRow, kRowMinH, [this] {
            return groove.bassEnabled
                && (groove.bassFormIndex != (int) GeneratorForm::Auto
                    || groove.bassLengthIndex >= (int) GeneratorLength::FourBars);
        });
        bassSec.addRow(&bassMotionRow, kRowMinH, [this] { return groove.bassEnabled; });

        juce::StringArray drumStyles;
        for (int i = 0; i < (int) DrumStyle::Count; ++i)
            drumStyles.add(drumStyleLabel((DrumStyle) i));
        drumStylePopup.setItems(drumStyles, 0);
        drumStylePopup.onChange = [this](int idx)
        {
            groove.drumStyleIndex = idx;
            notifyGroove();
        };
        fillLengthItems(drumLengthPopup, (int) GeneratorLength::FourBars);
        drumLengthPopup.onChange = [this](int idx)
        {
            groove.drumLengthIndex = juce::jlimit(0, (int) GeneratorLength::Count - 1, idx);
            notifyGroove();
        };
        fillFormItems(drumFormPopup, (int) GeneratorForm::Auto);
        drumFormPopup.onChange = [this](int idx)
        {
            groove.drumFormIndex = juce::jlimit(0, (int) GeneratorForm::Count - 1, idx);
            notifyGroove();
        };
        drumGenerateBtn.setTooltip("Roll a new drum groove into the piano roll");
        drumGenerateBtn.onClick = [this]
        {
            groove.drumsEnabled = true;
            groove.arpEnabled = groove.melodyEnabled = groove.chordsEnabled = groove.bassEnabled = false;
            groove.drumVariation = groove.drumVariation >= 16 ? 1 : groove.drumVariation + 1;
            ++groove.drumNonce;
            notifyGroove();
            if (onGeneratorGenerated) onGeneratorGenerated(4);
        };
        drumDensitySl.onChange = [this](int v)
        {
            groove.drumDensity = v;
            drumDensitySl.valueText = juce::String(v) + "%";
            notifyGroove();
        };
        drumFillSl.onChange = [this](int v)
        {
            groove.drumFill = v;
            drumFillSl.valueText = juce::String(v) + "%";
            notifyGroove();
        };
        drumSec.addRow(&drumGenerateRow, kRowMinH, [this] { return groove.drumsEnabled; });
        drumSec.addRow(&drumStyleRow, kRowMinH, [this] { return groove.drumsEnabled; });
        drumSec.addRow(&drumLengthRow, kRowMinH, [this] { return groove.drumsEnabled; });
        drumSec.addRow(&drumFormRow, kRowMinH, [this] {
            return groove.drumsEnabled
                && (groove.drumFormIndex != (int) GeneratorForm::Auto
                    || groove.drumLengthIndex >= (int) GeneratorLength::FourBars);
        });
        drumSec.addRow(&drumDensitySl, kSliderRowH, [this] {
            return groove.drumsEnabled && groove.drumDensity != 50;
        });
        drumSec.addRow(&drumFillSl, kSliderRowH, [this] {
            return groove.drumsEnabled && groove.drumFill != 35;
        });
    }

    playback.onToggle = timing.onToggle = performance.onToggle = pitchSec.onToggle
        = effectsSec.onToggle = [this] { layoutSections(); };
    // Generators are an accordion: opening one folds the other four and is the
    // only active part (others stop sounding until Generate rolls them).
    auto wireGeneratorFold = [this](fx::Section& self)
    {
        self.onToggle = [this, &self]
        {
            if (self.open)
            {
                for (auto* other : { &arpSec, &melodySec, &chordsSec, &bassSec, &drumSec })
                    if (other != &self && other->open)
                        other->setOpen(false);
                // Keep only this section's enabled flag so LiveMidiFx plays one part.
                const bool keepArp = &self == &arpSec && groove.arpEnabled;
                const bool keepMel = &self == &melodySec && groove.melodyEnabled;
                const bool keepCh = &self == &chordsSec && groove.chordsEnabled;
                const bool keepBass = &self == &bassSec && groove.bassEnabled;
                const bool keepDrum = &self == &drumSec && groove.drumsEnabled;
                groove.arpEnabled = keepArp;
                groove.melodyEnabled = keepMel;
                groove.chordsEnabled = keepCh;
                groove.bassEnabled = keepBass;
                groove.drumsEnabled = keepDrum;
                notifyGroove();
            }
            layoutSections();
        };
    };
    wireGeneratorFold(arpSec);
    wireGeneratorFold(melodySec);
    wireGeneratorFold(chordsSec);
    wireGeneratorFold(bassSec);
    wireGeneratorFold(drumSec);
    for (auto* gen : { &arpSec, &melodySec, &chordsSec, &bassSec, &drumSec })
        gen->showHeaderChrome = false;

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

    formSec.onReset = [this] { resetFormSection(); };
    formSec.onLockToggle = [this] { toggleSectionLock(toolkitLock::Effects); };

    effectsSec.onReset = [this] { resetEffectsSection(); };
    effectsSec.onLockToggle = [this] { toggleSectionLock(toolkitLock::Effects); };

    playback.sectionId = "playback";
    timing.sectionId = "timing";
    performance.sectionId = "performance";
    pitchSec.sectionId = "pitch";
    formSec.sectionId = "form";
    effectsSec.sectionId = "effects";
    arpSec.sectionId = "arp";
    melodySec.sectionId = "melody";
    chordsSec.sectionId = "chords";
    bassSec.sectionId = "bass";
    drumSec.sectionId = "drums";
    sectionOrder = { &playback, &formSec, &timing, &performance, &pitchSec, &effectsSec };
    for (auto* sec : { &arpSec, &melodySec, &chordsSec, &bassSec, &drumSec,
                       &playback, &timing, &performance, &pitchSec, &formSec, &effectsSec })
    {
        body.addAndMakeVisible(*sec);
        sec->onDragStart = [this, sec] { beginSectionDrag(sec); };
        sec->onDragMove = [this](int screenY) { moveSectionDrag(screenY); };
        sec->onDragEnd = [this] { endSectionDrag(); };
    }
    // Default fold: Playback + Timing + Performance + Form open so Transform controls are findable.
    playback.setOpen(true);
    timing.setOpen(true);
    performance.setOpen(true);
    formSec.setOpen(true);
    arpSec.setOpen(true);

    // "Hide Unused": fold every section down to its changed rows (the default way of working).
    btnChanged.setComponentID("btnToolkitChanged");
    btnChanged.setWantsKeyboardFocus(false);
    btnChanged.ghost = false;
    btnChanged.iconScale = 1.0f;
    btnChanged.setTooltip("Hide unused controls — show only what you have changed");
    btnChanged.onClick = [this]
    {
        if (externalGeneratorHost == nullptr && paneTab != 1)
            setPaneTab(1);
        foldAllSections();
        if (onActivated) onActivated();
    };
    addAndMakeVisible(btnChanged);

    setGroove(GrooveParams{}, juce::dontSendNotification);
    setEdit(ClipEdit{}, juce::dontSendNotification);
    setBpmMultiplier(1.0, juce::dontSendNotification);
}

void EffectsInspector::resetPitchEditFields()
{
    edit.octave = -1;
    edit.pitchShift = 0;
    edit.pitchInput = false;
    edit.octaveRange = 0;
    edit.outsideRange = OutsideRangeMode::Fold;
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

void EffectsInspector::resetFormSection()
{
    groove.complexityTarget = -1;
    groove.syncopation = 0;
    groove.variationIndex = 0;
    edit.reverse = false;
    edit.invert = false;
    edit.selectedNotesOnly = false;
    setGroove(groove, juce::dontSendNotification);
    setEdit(edit, juce::dontSendNotification);
    refreshComplexitySlider();
    notifyGroove();
    notifyEdit();
}

void EffectsInspector::resetEffectsSection()
{
    groove.delayAmount = 0;
    groove.delayFeedback = 40;
    groove.delayTimeIndex = (int) DelayTime::Eighth;
    setGroove(groove, juce::dontSendNotification);
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
    formSec.setLocked((sectionLocks & toolkitLock::Effects) != 0);
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
    {
        resetFormSection();
        resetEffectsSection();
    }
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
    const auto mode = (Mode) juce::jmax(0, modePopup.getIndex());
    noteFilter.setScaleContext(root, mode);
}

void EffectsInspector::refreshOctaveSummary()
{
    const int oct = displayedOctave();
    octaveRow.annotation.clear();
    octaveRow.stepper.setValue(oct, juce::dontSendNotification);
    octaveRow.repaint();
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

    // Form tiles four sections — hide below 4 bars even when the section is open.
    const int four = (int) GeneratorLength::FourBars;
    arpSec.setRowForcedHidden(&arpFormRow, groove.arpLengthIndex < four);
    melodySec.setRowForcedHidden(&melodyFormRow, groove.melodyLengthIndex < four);
    chordsSec.setRowForcedHidden(&chordFormRow, groove.chordLengthIndex < four);
    bassSec.setRowForcedHidden(&bassFormRow, groove.bassLengthIndex < four);
    drumSec.setRowForcedHidden(&drumFormRow, groove.drumLengthIndex < four);
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
    for (auto* sec : { &playback, &timing, &performance, &pitchSec, &formSec, &effectsSec })
        sec->refreshDirtyRows();
    refreshDependentEnables();
    layoutSections();
}

void EffectsInspector::setHasClip(bool has)
{
    if (hasClip == has)
    {
        refreshFormClipGating();
        return;
    }
    hasClip = has;
    refreshFormClipGating();
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
    octaveRow.stepper.setValue(displayedOctave(), juce::dontSendNotification);
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

void EffectsInspector::refreshFormClipGating()
{
    // Always show Form controls. Hiding them when passthrough had no clip left an
    // empty open section even with a browser clip on the roll (hasClip could lag
    // or stay false). Transforms simply no-op until a clip is present.
    juce::ignoreUnused(hasClip, liveStream);
    for (auto* c : { (juce::Component*) &complexitySl,
                     (juce::Component*) &syncopationSl,
                     (juce::Component*) &variationsSl,
                     (juce::Component*) &reverseRow,
                     (juce::Component*) &invertRow,
                     (juce::Component*) &selectedNotesRow })
        formSec.setRowForcedHidden(c, false);
    layoutSections();
}

void EffectsInspector::setLiveStreamMode(bool on)
{
    if (liveStream == on)
    {
        refreshFormClipGating();
        return;
    }
    liveStream = on;
    refreshFormClipGating();
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
    syncopationSl.setValue(g.syncopation, juce::dontSendNotification);
    variationsSl.setValue(g.variationIndex, juce::dontSendNotification);

    swingTimePopup.setIndex(juce::jlimit(0, 5, g.swingGridIndex), juce::dontSendNotification);
    quantizeTimePopup.setIndex(juce::jlimit(0, (int) QuantizeGrid::Count - 1, g.quantizeGridIndex),
                               juce::dontSendNotification);
    articulationPopup.setIndex(
        juce::jlimit(0, (int) Articulation::Count - 1, g.articulationIndex),
        juce::dontSendNotification);
    sustainPopup.setIndex(juce::jlimit(0, (int) SustainPedalMode::Count - 1, g.sustainPedalMode),
                          juce::dontSendNotification);
    delayTimePopup.setIndex(juce::jlimit(0, (int) DelayTime::Count - 1, g.delayTimeIndex),
                            juce::dontSendNotification);
    arpInputSwitch.setToggleState(g.arpInput, juce::dontSendNotification);
    arpLengthPopup.setIndex(juce::jlimit(0, (int) GeneratorLength::Count - 1, g.arpLengthIndex),
                            juce::dontSendNotification);
    arpFormPopup.setIndex(juce::jlimit(0, (int) GeneratorForm::Count - 1, g.arpFormIndex),
                          juce::dontSendNotification);
    arpShapePopup.setIndex(juce::jlimit(0, (int) ArpShape::Count - 1, g.arpShapeIndex),
                           juce::dontSendNotification);
    arpRatePopup.setIndex(juce::jlimit(0, (int) DelayTime::Count - 1, g.arpRateIndex),
                          juce::dontSendNotification);
    arpDensitySl.setValue(g.arpDensity, juce::dontSendNotification);
    arpDensitySl.valueText = juce::String(g.arpDensity) + "%";
    arpIntensitySl.setValue(g.arpIntensity, juce::dontSendNotification);
    arpIntensitySl.valueText = juce::String(g.arpIntensity) + "%";
    arpDynamicsSl.setValue(g.arpDynamics, juce::dontSendNotification);
    arpDynamicsSl.valueText = juce::String(g.arpDynamics) + "%";
    arpGateSl.setValue(g.arpGate, juce::dontSendNotification);
    arpGateSl.valueText = juce::String(g.arpGate) + "%";
    melodyInputSwitch.setToggleState(g.melodyInput, juce::dontSendNotification);
    melodyStylePopup.setIndex(juce::jlimit(0, (int) MelodyStyle::Count - 1, g.melodyStyleIndex),
                              juce::dontSendNotification);
    melodyLengthPopup.setIndex(juce::jlimit(0, (int) GeneratorLength::Count - 1, g.melodyLengthIndex),
                               juce::dontSendNotification);
    melodyFormPopup.setIndex(juce::jlimit(0, (int) GeneratorForm::Count - 1, g.melodyFormIndex),
                             juce::dontSendNotification);
    melodyContourPopup.setIndex(juce::jlimit(0, (int) MelodyContour::Count - 1, g.melodyContourIndex),
                                juce::dontSendNotification);
    melodySpeedPopup.setIndex(juce::jlimit(0, (int) MelodySpeed::Count - 1, g.melodySpeedIndex),
                              juce::dontSendNotification);
    chordsInputSwitch.setToggleState(g.chordsInput, juce::dontSendNotification);
    chordLengthPopup.setIndex(juce::jlimit(0, (int) GeneratorLength::Count - 1, g.chordLengthIndex),
                              juce::dontSendNotification);
    chordFormPopup.setIndex(juce::jlimit(0, (int) GeneratorForm::Count - 1, g.chordFormIndex),
                            juce::dontSendNotification);
    chordVoicingPopup.setIndex(juce::jlimit(0, (int) ChordVoicing::Count - 1, g.chordVoicingIndex),
                               juce::dontSendNotification);
    chordColorPopup.setIndex(juce::jlimit(0, (int) ChordColor::Count - 1, g.chordColorIndex),
                             juce::dontSendNotification);
    chordRhythmPopup.setIndex(juce::jlimit(0, (int) ChordRhythm::Count - 1, g.chordRhythmIndex),
                              juce::dontSendNotification);
    bassInputSwitch.setToggleState(g.bassInput, juce::dontSendNotification);
    bassLengthPopup.setIndex(juce::jlimit(0, (int) GeneratorLength::Count - 1, g.bassLengthIndex),
                             juce::dontSendNotification);
    bassFormPopup.setIndex(juce::jlimit(0, (int) GeneratorForm::Count - 1, g.bassFormIndex),
                           juce::dontSendNotification);
    bassMotionPopup.setIndex(juce::jlimit(0, (int) BassMotion::Count - 1, g.bassMotionIndex),
                             juce::dontSendNotification);
    drumStylePopup.setIndex(juce::jlimit(0, (int) DrumStyle::Count - 1, g.drumStyleIndex),
                            juce::dontSendNotification);
    drumLengthPopup.setIndex(juce::jlimit(0, (int) GeneratorLength::Count - 1, g.drumLengthIndex),
                             juce::dontSendNotification);
    drumFormPopup.setIndex(juce::jlimit(0, (int) GeneratorForm::Count - 1, g.drumFormIndex),
                           juce::dontSendNotification);
    drumDensitySl.setValue(g.drumDensity, juce::dontSendNotification);
    drumDensitySl.valueText = juce::String(g.drumDensity) + "%";
    drumFillSl.setValue(g.drumFill, juce::dontSendNotification);
    drumFillSl.valueText = juce::String(g.drumFill) + "%";

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
    syncopationSl.valueText = juce::String(g.syncopation);
    variationsSl.valueText = g.variationIndex <= 0 ? "Off" : ("#" + juce::String(g.variationIndex));
    refreshComplexitySlider();
    refreshDirtySections();
    repaint();
}

void EffectsInspector::setEdit(const ClipEdit& e, juce::NotificationType)
{
    edit = e;
    octaveRow.stepper.setValue(displayedOctave(), juce::dontSendNotification);
    octaveRangePopup.setIndex(juce::jlimit(0, 3, e.octaveRange), juce::dontSendNotification);
    outsideRangePopup.setIndex(e.outsideRange == OutsideRangeMode::Clip ? 0 : 1,
                               juce::dontSendNotification);
    pitchRow.stepper.setValue(e.pitchShift, juce::dontSendNotification);
    pitchInputSwitch.setToggleState(e.pitchInput, juce::dontSendNotification);
    pitchRangeSl.setRange(e.pitchMin, e.pitchMax, juce::dontSendNotification);
    pitchRangeSl.valueText = pitchName(e.pitchMin) + "-" + pitchName(e.pitchMax);
    {
        int extIdx = 0;
        if (e.extendMult == 2) extIdx = 1;
        else if (e.extendMult == 4) extIdx = 2;
        else if (e.extendMult == 8) extIdx = 3;
        extendPopup.setIndex(extIdx, juce::dontSendNotification);
    }
    reverseSwitch.setToggleState(e.reverse, juce::dontSendNotification);
    invertSwitch.setToggleState(e.invert, juce::dontSendNotification);
    selectedNotesSwitch.setToggleState(e.selectedNotesOnly, juce::dontSendNotification);
    keyGrid.setCandidateMask(candidateKeyMask);
    keyGrid.setSelectedIndex(e.root >= 0 ? e.root : 0, juce::dontSendNotification);
    modePopup.setIndex((int) e.mode, juce::dontSendNotification);
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
    // Groove: section frames stacked with 4 px gaps inside an 8 px inset.
    const int pad = metrics::scaled(8);
    const int gap = metrics::scaled(4);
    const int w = juce::jmax(1, body.getWidth() - pad * 2);
    int y = pad;
    const bool showGenerators = paneTab == 0 && externalGeneratorHost == nullptr;
    auto place = [&](fx::Section& sec, bool visible)
    {
        if (!visible)
        {
            sec.setVisible(false);
            sec.setBounds(0, 0, 0, 0);
            return;
        }
        sec.setVisible(true);
        const int h = sec.idealHeight();
        sec.setBounds(pad, y, w, h);
        sec.resized();
        y += h + gap;
    };

    if (showGenerators)
    {
        for (auto* sec : sectionOrder)
            place(*sec, false);
        place(arpSec, true);
        place(melodySec, true);
        place(chordsSec, true);
        place(bassSec, true);
        place(drumSec, true);
    }
    else
    {
        if (externalGeneratorHost == nullptr)
        {
            place(arpSec, false);
            place(melodySec, false);
            place(chordsSec, false);
            place(bassSec, false);
            place(drumSec, false);
        }
        for (auto* sec : sectionOrder)
            place(*sec, true);
    }
    y += pad - gap;
    body.setSize(body.getWidth(), juce::jmax(y, viewport.getMaximumVisibleHeight()));
    if (externalGeneratorHost != nullptr)
        layoutExternalGenerators();
    refreshChangedButton();
    if (onContentHeightChanged != nullptr && !notifyingHeight)
    {
        notifyingHeight = true;
        onContentHeightChanged();
        notifyingHeight = false;
    }
}

void EffectsInspector::refreshChangedButton()
{
    const bool folded = !anySectionOpen();
    if (btnChanged.active != folded)
    {
        btnChanged.active = folded;
        btnChanged.repaint();
    }
}

void EffectsInspector::setSectionOrder(const juce::String& ids)
{
    std::vector<fx::Section*> all { &playback, &timing, &performance, &pitchSec, &formSec, &effectsSec };
    std::vector<fx::Section*> next;
    for (const auto& id : juce::StringArray::fromTokens(ids, ",", {}))
        for (auto* sec : all)
            if (sec->sectionId == id.trim() && std::find(next.begin(), next.end(), sec) == next.end())
                next.push_back(sec);
    for (auto* sec : all)
        if (std::find(next.begin(), next.end(), sec) == next.end())
            next.push_back(sec);
    sectionOrder = next;
    layoutSections();
}

juce::String EffectsInspector::getSectionOrder() const
{
    juce::StringArray ids;
    for (auto* sec : sectionOrder)
        ids.add(sec->sectionId);
    return ids.joinIntoString(",");
}

void EffectsInspector::beginSectionDrag(fx::Section* sec)
{
    draggedSection = sec;
    sec->toFront(false);
    jiggleStartMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz(60);
}

void EffectsInspector::moveSectionDrag(int screenY)
{
    if (draggedSection == nullptr)
        return;
    const int y = body.getLocalPoint(nullptr, juce::Point<int>(0, screenY)).y;
    // Insert the dragged section before the first other section whose centre is below the pointer.
    std::vector<fx::Section*> others;
    for (auto* sec : sectionOrder)
        if (sec != draggedSection)
            others.push_back(sec);
    size_t insertAt = others.size();
    for (size_t i = 0; i < others.size(); ++i)
        if (others[i]->isVisible() && y < others[i]->getBounds().getCentreY())
        {
            insertAt = i;
            break;
        }
    others.insert(others.begin() + (std::ptrdiff_t) insertAt, draggedSection);
    if (others != sectionOrder)
    {
        sectionOrder = others;
        draggedSection->setTransform({});
        layoutSections();
    }
}

void EffectsInspector::endSectionDrag()
{
    stopTimer();
    if (draggedSection != nullptr)
        draggedSection->setTransform({});
    draggedSection = nullptr;
    layoutSections();
    if (onSectionOrderChanged)
        onSectionOrderChanged(getSectionOrder());
}

void EffectsInspector::timerCallback()
{
    // The picked-up section jiggles a little (about +/-0.7 degrees) until it is dropped.
    if (draggedSection == nullptr)
    {
        stopTimer();
        return;
    }
    const double t = (juce::Time::getMillisecondCounterHiRes() - jiggleStartMs) / 1000.0;
    const float angle = 0.012f * (float) std::sin(t * 24.0);
    const auto c = draggedSection->getLocalBounds().getCentre().toFloat();
    draggedSection->setTransform(juce::AffineTransform::rotation(angle, c.x, c.y));
}

int EffectsInspector::contentHeight() const
{
    const int pad = metrics::scaled(8);
    const int gap = metrics::scaled(4);
    int y = metrics::paneHeaderH() + pad;
    if (paneTab == 0)
    {
        y += arpSec.idealHeight() + gap;
        y += melodySec.idealHeight() + gap;
        y += chordsSec.idealHeight() + gap;
        y += bassSec.idealHeight() + gap;
        y += drumSec.idealHeight() + gap;
    }
    else
    {
        for (auto* sec : sectionOrder)
            y += sec->idealHeight() + gap;
    }
    return y + pad - gap;
}

int EffectsInspector::defaultFoldHeight() const
{
    // Screenshot fold: Timing + Performance open, Pitch & Effects closed.
    const int pad = metrics::scaled(8);
    const int gap = metrics::scaled(4);
    int y = metrics::paneHeaderH() + pad;
    y += timing.idealHeightForState(true) + gap;
    y += performance.idealHeightForState(true) + gap;
    y += pitchSec.idealHeightForState(false) + gap;
    y += effectsSec.idealHeightForState(false) + gap;
    return y + pad - gap;
}

void EffectsInspector::setPaneTab(int tab)
{
    const int next = externalGeneratorHost != nullptr ? 1
                   : generatorsEnabled ? juce::jlimit(0, 1, tab) : 1;
    if (paneTab == next) return;
    paneTab = next;
    layoutSections();
    repaint();
    if (onPaneTabChanged) onPaneTabChanged(paneTab);
}

void EffectsInspector::setExternalGeneratorHost(juce::Component* host)
{
    if (externalGeneratorHost == host)
        return;
    externalGeneratorHost = host;
    juce::Component& parent = host != nullptr ? *host : body;
    for (auto* sec : { &arpSec, &melodySec, &chordsSec, &bassSec, &drumSec })
        parent.addAndMakeVisible(sec);
    if (host != nullptr)
        paneTab = 1;
    layoutSections();
    repaint();
}

void EffectsInspector::layoutExternalGenerators()
{
    if (externalGeneratorHost == nullptr)
        return;
    auto& host = *externalGeneratorHost;
    const int pad = metrics::scaled(8);
    const int gap = metrics::scaled(4);
    const int w = juce::jmax(1, host.getWidth() - pad * 2);
    int y = pad;
    for (auto* sec : { &arpSec, &melodySec, &chordsSec, &bassSec, &drumSec })
    {
        sec->setVisible(true);
        const int h = sec->idealHeight();
        sec->setBounds(pad, y, w, h);
        sec->resized();
        y += h + gap;
    }
    host.setSize(juce::jmax(1, host.getWidth()), juce::jmax(y + pad - gap, 1));
}

void EffectsInspector::setGeneratorsEnabled(bool on)
{
    if (generatorsEnabled == on) return;
    generatorsEnabled = on;
    if (!generatorsEnabled && paneTab == 0)
        paneTab = 1;
    layoutSections();
    repaint();
}

bool EffectsInspector::anySectionOpen() const
{
    if (paneTab == 0)
        return arpSec.open || melodySec.open || chordsSec.open || bassSec.open || drumSec.open;
    if (playback.open) return true;
    return timing.open || performance.open || pitchSec.open || formSec.open || effectsSec.open;
}

void EffectsInspector::foldAllSections()
{
    // Folding keeps changed rows visible — same as the per-section chevrons.
    const bool expand = !anySectionOpen();
    if (paneTab == 0)
    {
        for (auto* sec : { &arpSec, &melodySec, &chordsSec, &bassSec, &drumSec })
            sec->setOpen(expand);
    }
    else
    {
        for (auto* sec : sectionOrder)
            sec->setOpen(expand);
    }
    // Re-evaluate dirty predicates after the fold so Key / Mode / Fit / Map stay up.
    refreshDirtySections();
    layoutSections();
    repaint();
}

void EffectsInspector::updateToolkitFoldBounds()
{
    toolkitFoldBounds = {};   // the "Changed" toggle replaces the header chevron
}

void EffectsInspector::resized()
{
    auto r = getLocalBounds();
    const int headerH = metrics::paneHeaderH();
    auto header = r.removeFromTop(headerH).reduced(metrics::scaled(8), 0).withTrimmedBottom(1);
    const int ctl = metrics::chromeIconButton();
    // Right → left: Reset · Lock. Hide Unused (icon) sits on the left (replaces Effects title).
    btnEffectsLock.setBounds(header.removeFromRight(ctl).withSizeKeepingCentre(ctl, ctl));
    header.removeFromRight(metrics::scaled(4));
    btnReset.setBounds(header.removeFromRight(ctl).withSizeKeepingCentre(ctl, ctl));
    header.removeFromRight(metrics::scaled(8));
    if (externalGeneratorHost != nullptr)
    {
        generatorsTabBounds = {};
        effectsTabBounds = {};
        btnChanged.setBounds(header.removeFromLeft(ctl).withSizeKeepingCentre(ctl, ctl));
    }
    else
    {
        const int genW = juce::roundToInt(juce::GlyphArrangement::getStringWidth(uiFontFixed(12.0f, true), "Generators")) + 4;
        generatorsTabBounds = header.removeFromLeft(genW).withSizeKeepingCentre(genW, ctl);
        header.removeFromLeft(metrics::scaled(10));
        effectsTabBounds = {};
        btnChanged.setBounds(header.removeFromLeft(ctl).withSizeKeepingCentre(ctl, ctl));
    }
    updateToolkitFoldBounds();

    viewport.setBounds(r);
    body.setSize(juce::jmax(1, viewport.getMaximumVisibleWidth()), body.getHeight());

    // Match octave-range / outside-range width (octave stepper ideal).
    const int stepW = octaveRow.stepper.idealWidth();
    // Mode / Sustain / Articulation need room for longer labels — half the section.
    const int halfW = juce::jmax(stepW, (body.getWidth() - kContentPadX - kContentPadR) / 2);
    auto fitSelect = [stepW](fx::FlatPopup& p, fx::InlineRow& row)
    {
        p.setSize(stepW, kControlH);
        row.setControlWidth(stepW);
    };
    auto fitWide = [halfW](fx::FlatPopup& p, fx::InlineRow& row)
    {
        p.setSize(halfW, kControlH);
        row.setControlWidth(halfW);
    };
    {
        const int tempoW = kSelectHalfW;
        tempoToggle.setSize(tempoW, kControlH);
        tempoRow.setControlWidth(tempoW);
    }
    trimRow.setControlWidth(trimSwitch.idealWidth());
    reverseRow.setControlWidth(reverseSwitch.idealWidth());
    invertRow.setControlWidth(invertSwitch.idealWidth());
    fitSelect(extendPopup, extendRow);
    fitWide(sustainPopup, sustainRow);
    fitSelect(octaveRangePopup, octaveRangeRow);
    fitSelect(outsideRangePopup, outsideRangeRow);
    fitWide(articulationPopup, articulationRow);
    fitWide(modePopup, modeRow);
    fitSelect(filterTypePopup, filterTypeRow);
    fitSelect(arpLengthPopup, arpLengthRow);
    fitSelect(arpFormPopup, arpFormRow);
    fitSelect(arpShapePopup, arpShapeRow);
    fitSelect(arpRatePopup, arpRateRow);
    fitSelect(melodyLengthPopup, melodyLengthRow);
    fitSelect(melodyFormPopup, melodyFormRow);
    fitSelect(melodyStylePopup, melodyStyleRow);
    fitSelect(melodyContourPopup, melodyContourRow);
    fitSelect(melodySpeedPopup, melodySpeedRow);
    fitSelect(chordLengthPopup, chordLengthRow);
    fitSelect(chordFormPopup, chordFormRow);
    fitSelect(chordVoicingPopup, chordVoicingRow);
    fitSelect(chordColorPopup, chordColorRow);
    fitSelect(chordRhythmPopup, chordRhythmRow);
    fitSelect(bassLengthPopup, bassLengthRow);
    fitSelect(bassFormPopup, bassFormRow);
    fitSelect(bassMotionPopup, bassMotionRow);
    fitSelect(drumStylePopup, drumStyleRow);
    fitSelect(drumLengthPopup, drumLengthRow);
    fitSelect(drumFormPopup, drumFormRow);
    quantizeBlock.setControlWidth(stepW);
    swingBlock.setControlWidth(stepW);
    delayBlock.setControlWidth(stepW);
    arpGenerateRow.setControlWidth(arpGenerateBtn.idealWidth());
    arpInputRow.setControlWidth(arpInputSwitch.idealWidth());
    melodyGenerateRow.setControlWidth(melodyGenerateBtn.idealWidth());
    melodyInputRow.setControlWidth(melodyInputSwitch.idealWidth());
    chordsGenerateRow.setControlWidth(chordsGenerateBtn.idealWidth());
    chordsInputRow.setControlWidth(chordsInputSwitch.idealWidth());
    bassGenerateRow.setControlWidth(bassGenerateBtn.idealWidth());
    bassInputRow.setControlWidth(bassInputSwitch.idealWidth());
    drumGenerateRow.setControlWidth(drumGenerateBtn.idealWidth());
    pitchInputRow.setControlWidth(pitchInputSwitch.idealWidth());
    arpDensitySl.setSize(arpDensitySl.getWidth(), kControlH);
    arpIntensitySl.setSize(arpIntensitySl.getWidth(), kControlH);
    arpDynamicsSl.setSize(arpDynamicsSl.getWidth(), kControlH);
    arpGateSl.setSize(arpGateSl.getWidth(), kControlH);
    drumDensitySl.setSize(drumDensitySl.getWidth(), kControlH);
    drumFillSl.setSize(drumFillSl.getWidth(), kControlH);

    pitchRow.stepper.setSize(pitchRow.stepper.idealWidth(), kControlH);
    octaveRow.stepper.setSize(octaveRow.stepper.idealWidth(), kControlH);
    fitRow.setControlWidth(fitSwitch.idealWidth());
    mapRow.setControlWidth(mapSwitch.idealWidth());

    layoutSections();
}

void EffectsInspector::paint(juce::Graphics& g)
{
    g.fillAll(ds::surface100());
    g.setColour(ds::line());
    g.fillRect(getLocalBounds().removeFromLeft(1));

    auto header = getLocalBounds().removeFromTop(metrics::paneHeaderH());
    g.setColour(ds::surface300());
    g.fillRect(header);
    g.setColour(ds::line());
    g.fillRect(header.getX(), header.getBottom() - 1, header.getWidth(), 1);
    // Column rule on the left edge of the Effects header.
    g.fillRect(0, header.getY(), 1, header.getHeight());

    g.setFont(uiFontFixed(12.0f, true));
    if (externalGeneratorHost != nullptr)
        return;

    auto drawTab = [&](juce::Rectangle<int> r, const juce::String& label, bool active, bool enabled)
    {
        g.setColour(active ? ds::ink() : (enabled ? ds::inkMuted() : ds::inkFaint()));
        g.drawText(label, r, juce::Justification::centredLeft, false);
        if (active)
        {
            g.setColour(ds::acc());
            g.fillRect(r.getX(), header.getBottom() - 2, r.getWidth(), 2);
        }
    };
    drawTab(generatorsTabBounds, "Generators", paneTab == 0, generatorsEnabled);
    // Effects title is replaced by the Hide Unused control.
}

void EffectsInspector::mouseDown(const juce::MouseEvent& e)
{
    // Listener events are relative to the nested source — map into our space.
    const auto local = e.getEventRelativeTo(this).getPosition();
    updateToolkitFoldBounds();

    if (local.y < metrics::paneHeaderH())
    {
        if (externalGeneratorHost == nullptr && generatorsTabBounds.expanded(4, 4).contains(local))
        {
            if (generatorsEnabled)
                setPaneTab(0);
            if (onActivated) onActivated();
            return;
        }
    }

    // Toolkit title chevron lives in the pane header (outside the viewport).
    if (local.y < metrics::paneHeaderH()
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
    const bool overFold = local.y < metrics::paneHeaderH()
                       && toolkitFoldBounds.expanded(8, 6).contains(local);
    if (overFold)
        setTooltip("Fold all, keeping changed controls");
    setMouseCursor(overFold ? juce::MouseCursor::PointingHandCursor
                            : juce::MouseCursor::NormalCursor);
}

} // namespace pflow
