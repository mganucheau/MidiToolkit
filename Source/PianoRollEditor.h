#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <set>
#include "EditModel.h"
#include "GrooveEngine.h"
#include "UiAtoms.h"
#include "EffectsInspectorWidgets.h"

namespace pflow {

/** Effective per-note velocity (file velocity as base, groove on top). */
double effectiveVelocity(const RollNote& n, const GrooveParams& k);

struct PitchRowMap
{
    int minPitch = 48, maxPitch = 72;
    float rowH = 8.0f;
    int numRows() const { return maxPitch - minPitch + 1; }
    float yForPitchTop(int pitch, float height) const
    {
        return height - (float) (pitch - minPitch + 1) * rowH;
    }
    /** Fit so the lowest visible row is the key root in the lowest note's octave,
        and the highest is the key root at or above the highest note. */
    void fit(const std::vector<RollNote>& notes, float height, int rootPc, int minRange = 16);
};

/** Read-only compact roll for the browser preview strip — mirrors the editor
    piano-roll look (keyboard gutter, scale shading, beat grid, notes). */
class PianoRollMini : public juce::Component
{
public:
    void setNotes(std::vector<RollNote> resolvedGrooved, int bars, const GrooveParams& groove,
                  int rootPc = 0, std::vector<RollNote> frameNotes = {},
                  Mode mode = Mode::Ionian, int divisionSteps = 4,
                  uint16_t noteFilterMask = 0x0FFF);
    void setPlayheadStep(double step, bool playing);
    void setTimeStretch(double stretch);
    void paint(juce::Graphics&) override;

    /** Groove: notes take the clip's kind colour; optional velocity lane under the roll. */
    void setNoteColour(juce::Colour c) { noteColour = c; repaint(); }
    void setShowVelocity(bool on) { showVelocity = on; repaint(); }
    bool isShowingVelocity() const { return showVelocity; }
    /** 0..1 position of the playhead through the (stretched) clip. */
    double playheadFraction() const;
    static constexpr int kRulerH = 14;
    static constexpr int kVelocityH = 40;

private:
    juce::Colour noteColour { 0xff6fb5f0 };
    bool showVelocity = false;
    std::vector<RollNote> notes;
    std::vector<RollNote> frameNotes; // stable pitch frame (usually source clip)
    GrooveParams knobs;
    int bars = 1;
    int rootPc = 0;
    Mode mode = Mode::Ionian;
    int divisionSteps = 4;
    uint16_t noteFilterMask = 0x0FFF;
    double playheadStep = 0.0;
    bool playing = false;
    double timeStretch = 1.0;
    static constexpr float kGutterW = 20.0f;
};

// Cupertino editor: clip header · toolbar · roll · velocity lane.

class PianoRollEditor : public juce::Component
{
public:
    PianoRollEditor();
    ~PianoRollEditor() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

    /** Feed the selected clip + its edit + groove. The editor never mutates
        the clip; user actions come back through the callbacks. */
    void setClip(const StepClip& clip, const ClipEdit& edit, const GrooveParams& groove);
    /** Notes take the clip kind's colour (Groove clip-1 … clip-8). */
    juce::Colour noteColour { 0xff6fb5f0 };
    /** Shown over an empty roll (e.g. "Play MIDI in, then press record" in Through). */
    void setEmptyHint(const juce::String& h) { emptyHint = h; repaint(); }
    juce::String emptyHint;
    /** Input mode (Through): header reads "Input" with a record status instead of clip tools. */
    void setInputMode(bool isInput, const juce::String& subtitle);
    bool isInputMode() const { return inputMode; }
    void clearClip();
    /** Live take: notes grow in place and loop handles stay hidden. */
    void beginRecording(const StepClip& clip);
    void setRecordingNotes(const StepClip& clip, double playheadStep);
    /** Show the finished take and place the movable loop. */
    void endRecording(const StepClip& clip, const ClipEdit& edit, const GrooveParams& groove,
                      double loopEndStep);
    void setPlayheadStep(double step, bool playing);
    void setLockActive(bool locked);
    void setTimeStretch(double stretch);   // 1/bpmMultiplier — stretches roll + export view
    void setScalePlacement(ScalePlacement placement);
    bool hasSelection() const { return !selection.empty(); }
    const std::set<int>& selectedNoteIds() const { return selection; }
    void deleteSelectedNotes();   // non-destructive (edit.deleted)
    void toggleTrim();

    std::function<void(const ClipEdit&)> onEditChanged;
    std::function<void(bool active, bool enabled, const juce::String& label)> onTrimStateChanged;
    /** When set, the trim control asks the editor before toggling (lock-aware). */
    std::function<void()> onTrimRequested;
    /** Loop region in unstretched steps [start, end); end exclusive. */
    std::function<void(double startStep, double endStep)> onLoopChanged;
    /** Fired when the user clicks into the piano roll (sticky key-nav). */
    std::function<void()> onActivated;

private:
    // ── inner surfaces ──
    class RollContent : public juce::Component
    {
    public:
        explicit RollContent(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;
        PianoRollEditor& owner;

        // interaction state
        enum class Drag { None, Note, Marquee, Pan, LoopStart, LoopEnd };
        Drag drag = Drag::None;
        int dragNoteId = -1;
        juce::Point<float> dragStart;
        juce::Point<int> panStartView;
        int dragDRows = 0, dragDStep = 0;       // live preview offsets
        juce::Rectangle<float> marquee;
        bool marqueeAdditive = false;
    };

    /** Bar/beat labels + loop brace handles above the roll grid. */
    class TimeRuler : public juce::Component
    {
    public:
        explicit TimeRuler(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        PianoRollEditor& owner;
        enum class Drag { None, LoopStart, LoopEnd };
        Drag drag = Drag::None;
    };

    class KeyGutter : public juce::Component
    {
    public:
        explicit KeyGutter(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        PianoRollEditor& owner;
    };

    class VelocityLane : public juce::Component
    {
    public:
        explicit VelocityLane(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        PianoRollEditor& owner;
        int dragNoteId = -1;
        static constexpr int headerH = 22;
        static constexpr int laneH = 76;

    };

    /** Folding Scale section (Bottom placement): octave / key / mode / fit / map. */
    class ScalePanel : public juce::Component
    {
    public:
        explicit ScalePanel(PianoRollEditor& o) : owner(o) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void resized() override;
        bool hitTest(int x, int y) override;
        int idealHeight() const;
        bool isOpen() const { return open; }
        void setOpen(bool shouldOpen);
        PianoRollEditor& owner;
        static constexpr int headerH = 32;
        static constexpr int bodyH = 44;
        bool open = true;
    };

    struct NotifyingViewport : juce::Viewport
    {
        std::function<void()> onScrolled;
        void visibleAreaChanged(const juce::Rectangle<int>&) override
        {
            if (onScrolled) onScrolled();
        }
    };

    friend class RollContent;
    friend class TimeRuler;
    friend class KeyGutter;
    friend class VelocityLane;
    friend class ScalePanel;

    // ── model → view ──
    void rebuildResolved();
    void applyEdit(std::function<void(ClipEdit&)> mutate);
    void refreshControls();
    void updateRollSize();
    void scrollToContent();
    /** Fit horizontal width to the clip and vertical zoom to one lane above and below the notes. */
    void zoomToClip();
    void layoutScaleControls(juce::Rectangle<int> area);
    void nudgeSelection(int dPitch, int dStep);

    // ── row geometry ──
    float pxPerStep() const { return pxPerStepBase * zoomX; }
    bool rowsCollapsed() const { return folded || scaleFiltered; }
    float effRowH() const { return rowsCollapsed() ? rowH * 2.0f : rowH; }   // folded rows are 2x tall
    int totalSteps() const
    {
        return juce::jmax(1, (int) std::lround((double) resolved.bars * kStepsPerBar * timeStretch));
    }
    int numRows() const
    {
        if (rowsCollapsed())
            return juce::jmax(1, (int) foldPitches.size());
        if (pitchWindowActive)
            return juce::jmax(1, pitchWindowHi - pitchWindowLo + 1);
        return 128;
    }
    int rowForPitch(int pitch) const;
    int pitchForRow(int row) const;
    juce::Rectangle<float> noteRect(const RollNote&, int rowOffset = 0, double stepOffset = 0.0) const;
    const RollNote* noteAt(juce::Point<float> contentPos) const;
    void computePxPerStepBase();
    void zoomXAround(float factor, float contentX);
    void zoomRowsAround(float factor, float contentY);
    void commitNoteDrag();
    void selectPitch(int pitch, bool additive);
    void refreshTrimButtonState();
    int velocityForNote(const RollNote& n) const;
    const RollNote* noteAtVelocityX(float laneX) const;
    void setVelocityFromLaneY(int noteId, float laneY);
    void applyBarsZoomFromPopup();
    void applyDivisionFromPopup();
    int barsZoomPopupIndex() const;
    int divisionPopupIndex() const;
    void removeBadge(const juce::String& key);
    void resetLoopToClip();
    void setLoopSteps(double start, double end, bool notify);
    void notifyLoopChanged();
    double clipSteps() const { return (double) resolved.bars * (double) kStepsPerBar; }
    double minLoopSteps() const { return (double) juce::jmax(1, divisionSteps); }
    double stepFromContentX(float x) const;
    float contentXFromStep(double step) const;
    int hitLoopHandle(float contentX, float hitPx = 6.0f) const; // -1 none, 0 start, 1 end
    void paintLoopOverlay(juce::Graphics&, juce::Rectangle<float> clipB, float top, float bottom) const;
    void paintTimeRulerLabels(juce::Graphics&, float viewX, float width, float height) const;

    // ── state ──
    bool hasClip = false;
    bool pendingScrollToContent = false;
    bool recordingVisual = false;
    StepClip clip;
    ClipEdit edit;
    GrooveParams groove;
    ResolvedClip resolved;                  // resolveClip output
    std::vector<RollNote> grooved;          // applyGroove(resolved)
    std::vector<int> emptyBarIndices;       // all empty bars pre-trim
    std::set<int> selection;
    std::vector<int> foldPitches;           // ascending note-bearing pitches
    bool folded = false;
    /** When true, hide pitches/rows that are outside the selected scale. */
    bool scaleFiltered = false;
    float pxPerStepBase = 6.0f;             // fit-to-width at zoomX 1
    float zoomX = 1.0f;                     // horizontal zoom, 1..6
    int visibleBarsZoom = -1;               // -1 = Clip (fit width + notes); else 2/4/8/16
    float rowH = 13.0f;                     // vertical zoom (px per semitone)
    /** When true, only pitches [pitchWindowLo, pitchWindowHi] are shown (clip vertical fit). */
    bool pitchWindowActive = false;
    int pitchWindowLo = 0;
    int pitchWindowHi = 127;
    int divisionSteps = 4;                  // grid + snap (1/4 default)
    double playheadStep = 0.0;
    bool playing = false;
    bool lockActive = false;
    double timeStretch = 1.0;               // 1 / bpmMultiplier
    ScalePlacement scalePlacement = ScalePlacement::Top;
    double loopStartStep = 0.0;             // unstretched steps
    double loopEndStep = 16.0;              // exclusive

    static constexpr float kMinRowH = 5.0f;    // max notes on screen
    static constexpr float kMaxRowH = 26.0f;   // min notes on screen
    static constexpr float kMaxZoomX = 6.0f;
    static constexpr int rulerH = 20;

    // ── children ──
    Stepper octaveStepper;
    juce::ComboBox rootPicker, modePicker;
    fx::FlatPopup divisionPopup;
    fx::FlatPopup barsZoomPopup;
    MiniSwitch fitSwitch { "Fit to scale" };
    MiniSwitch mapSwitch { "Map to root" };
    IconBtn btnLock { icons::lockOpen, "Lock pitch edits while browsing" };
    IconBtn btnRevert { icons::undo, "Revert all edits" };
    ChipBtn btnFold { "Fold", icons::foldRows };
    ChipBtn btnScale { "Scale" };
    IconBtn btnZoomOut { icons::zoomOut, "Zoom out" };
    IconBtn btnZoomIn { icons::zoomIn, "Zoom in" };
    ChipBtn selBadge { "0 sel" };
    ChipBtn btnRevertNotes { "Revert" };
    bool inputMode = false;
    juce::String inputSub;
    bool hasNoteEdits() const;
    void revertNoteEdits();
    void layoutHeaderTools();
    std::vector<std::unique_ptr<ChipBtn>> badgeChips;

    KeyGutter gutter { *this };
    TimeRuler timeRuler { *this };
    NotifyingViewport rollViewport;
    RollContent rollContent { *this };
    VelocityLane velocityLane { *this };
    bool velocityOpen = false;
    ScalePanel scalePanel { *this };

    static constexpr int stripH = 0;
    /** Matches Files / Generator and Effects headers (metrics::paneHeaderH). */
    int toolbarHeight() const { return metrics::paneHeaderH(); }
    static constexpr int gutterW = 56;

    juce::Rectangle<int> zoomLabelBounds, gridLabelBounds, titleBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditor)
};

} // namespace pflow
