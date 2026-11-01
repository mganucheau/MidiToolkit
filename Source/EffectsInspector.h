#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EditModel.h"
#include "GrooveEngine.h"
#include "UiAtoms.h"
#include "EffectsInspectorWidgets.h"

namespace pflow {

/** Flat macOS-style effects inspector (Playback / Timing / Performance / Pitch & Scale / Effects). */
class EffectsInspector : public juce::Component, public juce::SettableTooltipClient
{
public:
    EffectsInspector();

    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    void setGroove(const GrooveParams&, juce::NotificationType notify = juce::dontSendNotification);
    GrooveParams getGroove() const { return groove; }

    void setEdit(const ClipEdit&, juce::NotificationType notify = juce::dontSendNotification);
    ClipEdit getEdit() const { return edit; }

    void setBpmMultiplier(double mult, juce::NotificationType notify = juce::dontSendNotification);
    double getBpmMultiplier() const { return bpmMultiplier; }

    void setSectionLocks(uint32_t locks);
    uint32_t getSectionLocks() const { return sectionLocks; }
    void setHasClip(bool has);
    void setClipRoot(int rootPc);
    /** Native scientific octave of the loaded clip (for absolute Octave selector). */
    void setClipSourceOctave(int oct);
    /** Highlight all viable keys/modes for the loaded clip. */
    void setClipScaleAnalysis(const ScaleAnalysis& analysis);
    /** Clip baseline complexity (1..100) — drives the Complexity slider position. */
    void setClipComplexity(int complexity);
    void setTrimState(bool active, bool enabled, const juce::String& label = {});
    /** Live MIDI stream: hide Playback, Complexity, and Variations. */
    void setLiveStreamMode(bool on);

    std::function<void(const GrooveParams&)> onGrooveChanged;
    std::function<void(const ClipEdit&)> onEditChanged;
    std::function<void(double)> onBpmMultiplierChanged;
    /** Fired when header or section lock bits change. */
    std::function<void(uint32_t)> onSectionLocksChanged;
    /** Reset unlocked sections only (header refresh). */
    std::function<void()> onResetUnlocked;
    std::function<void()> onTrimClicked;
    bool isTrimOn() const { return trimSwitch.getToggleState(); }
    std::function<void()> onActivated;
    /** Transport + toolkit header + current section heights. */
    int contentHeight() const;
    /** Fired when a section fold changes the preferred pane height. */
    std::function<void()> onContentHeightChanged;

    fx::Section& playbackSection() { return playback; }
    fx::Section& timingSection() { return timing; }
    fx::Section& performanceSection() { return performance; }
    fx::Section& pitchSection() { return pitchSec; }
    fx::Section& effectsSection() { return effectsSec; }
    bool delayPairVisible() const
    {
        return delayBlock.isVisible() && delayTimePopup.isVisible() && delayAmountSl.isVisible();
    }
    bool delayFeedbackVisible() const { return delayFeedbackSl.isVisible(); }
    bool articulationStrengthEnabled() const { return articulationStrengthSl.isEnabled(); }
    juce::String noteFilterLegendLead() const { return noteFilter.legendLead(); }

private:
    void notifyGroove();
    void notifyEdit();
    void layoutSections();
    void refreshDirtySections();
    void refreshPitchAnnotation();
    void refreshComplexitySlider();
    void syncArticulationKnobsToUi();
    void syncNoteFilterScale();
    int displayedOctave() const;
    int resolvedPitchMidi() const;
    void resetPitchEditFields();
    void resetPlaybackSection();
    void resetTimingSection();
    void resetPerformanceSection();
    void resetEffectsSection();
    void toggleSectionLock(uint32_t bit);
    void setAllSectionsLocked(bool locked);
    void refreshHeaderLockButton();
    void resetUnlockedSections();
    void foldAllSections();
    bool anySectionOpen() const;
    void updateToolkitFoldBounds();
    void refreshDependentEnables();
    void refreshOctaveSummary();

    GrooveParams groove;
    ClipEdit edit;
    double bpmMultiplier = 1.0;
    uint32_t sectionLocks = 0;
    bool hasClip = false;
    bool liveStream = false;
    int clipRootPc = -1;
    int clipSourceOctave = 4;
    int clipComplexity = 50;
    uint16_t candidateKeyMask = 0;
    uint16_t candidateModeMask = 0;

    IconBtn btnReset { icons::undo, "Reset effects to defaults" };
    IconBtn btnEffectsLock { icons::lockOpen, "Lock effects while browsing" };
    juce::Rectangle<int> toolkitFoldBounds;

    fx::Section playback { "Playback" };
    fx::Section timing { "Timing" };
    fx::Section performance { "Performance" };
    fx::Section pitchSec { "Pitch & Scale" };
    fx::Section effectsSec { "Effects" };

    fx::TempoToggle tempoToggle;
    fx::InlineRow tempoRow;
    fx::FlatSwitch trimSwitch;
    fx::FlatPopup extendPopup;
    fx::InlineRow extendRow;

    fx::FlatPopup swingTimePopup;
    fx::FlatPopup quantizeTimePopup;
    fx::FlatSliderRow quantizeStrengthSl;
    fx::GridAmountBlock quantizeBlock { "Quantize", quantizeTimePopup, quantizeStrengthSl };
    fx::FlatSliderRow swing;
    fx::GridAmountBlock swingBlock { "Swing", swingTimePopup, swing };
    fx::FlatSliderRow pocket;
    fx::FlatSliderRow humanize;
    fx::FlatSliderRow lengthSl;

    fx::FlatSliderRow dynamicsSl;
    fx::FlatSliderRow intensitySl;
    fx::ChipGrid articulationGrid;
    fx::FlatSliderRow articulationStrengthSl;
    fx::FlatRangeSliderRow velocityRangeSl;
    fx::FlatPopup sustainPopup;
    fx::InlineRow sustainRow;

    fx::FlatSliderRow complexitySl;
    fx::FlatSliderRow variationsSl;
    fx::FlatPopup delayTimePopup;
    fx::FlatSliderRow delayAmountSl;
    fx::GridAmountBlock delayBlock { "Delay", delayTimePopup, delayAmountSl };
    fx::FlatSliderRow delayFeedbackSl;

    fx::SegmentedSelector octaveSelector;
    fx::FlatPopup octaveRangePopup;
    fx::InlineRow octaveRangeRow;
    fx::PitchRow pitchRow;
    fx::ChipGrid keyGrid;
    fx::ChipGrid modeGrid;
    fx::FlatSwitch fitSwitch;
    fx::FlatSwitch mapSwitch;
    fx::NoteFilterBlock noteFilter;
    fx::FlatRangeSliderRow pitchRangeSl;
    fx::FlatPopup filterTypePopup;
    fx::InlineRow filterTypeRow;

    fx::InlineRow trimRow;
    fx::InlineRow fitRow;
    fx::InlineRow mapRow;

    fx::Divider timingDivider;
    fx::Divider performanceDivider;
    fx::Divider pitchDividerA;
    fx::Divider pitchDividerB;
    fx::Divider effectsDivider;

    juce::Viewport viewport;
    juce::Component body;
    bool notifyingHeight = false;

    static constexpr int kPanelW = 300;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectsInspector)
};

} // namespace pflow
