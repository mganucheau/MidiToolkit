#pragma once
#include <array>
#include <vector>

#include "EditModel.h"

namespace pflow {

// ── Groove engine ────────────────────────────────────────────────────────────
// Non-destructive timing/velocity transform over resolveClip() output.
// Modelled on Ableton Groove Pool + Logic Q-Swing / Humanize conventions:
//
//   Swing     — delay offbeats at a chosen base (1/8 or 1/16); 0 = straight
//   Pocket    — tight<->loose feel (- push / + laid-back); strong beats move less
//   Humanize  — deterministic per-note timing jitter (Ableton Random)
//   Dynamics  — metric accent contrast; bipolar (- inverts, like Ableton Velocity)
//   Length    — note duration scale; 100 = unchanged (center default)
//   Intensity — velocity scale; 100 = file velocities unchanged (center default)
//   Quantize  — snap starts toward a grid (incl. triplets); strength 0 = off
//   Articulation — phrasing shape (length/velocity); strength 0 = off
//   Sustain   — pedal automation for export/preview

struct KnobDef
{
    const char* key;
    const char* label;
    int min, max, def;
    /** Arc fills from 0 (bipolar knobs). */
    bool bipolar() const { return min < 0; }
    /** Arc fills from def when def sits mid-range (Length, Intensity). */
    bool fillFromDefault() const { return !bipolar() && def > min && def < max; }
};

enum class SwingBase { Eighth = 0, Sixteenth = 1 };

/** Quantize grid labels: 1/4, 1/4t, 1/8, 1/8t, 1/16, 1/16t, 1/32, 1/32t. */
enum class QuantizeGrid : int
{
    Quarter = 0,
    QuarterT,
    Eighth,
    EighthT,
    Sixteenth,
    SixteenthT,
    ThirtySecond,
    ThirtySecondT,
    Count
};

enum class Articulation : int
{
    Off = 0,
    Legato,
    Staccato,
    Tenuto,
    Pianissimo,
    Sforzando,
    Slur,
    Count
};

enum class SustainPedalMode : int
{
    Off = 0,
    StartOfClip,
    Auto,
    Count
};

enum class DelayTime : int
{
    Quarter = 0,
    DottedEighth,
    Eighth,
    EighthT,
    Sixteenth,
    SixteenthT,
    ThirtySecond,
    Count
};

enum class ArpDirection : int
{
    Up = 0,
    Down,
    UpDown,
    Random,
    Count
};

enum class ArpShape : int
{
    Walk = 0,
    Skip,
    Pedal,
    Arch,
    Climb,
    Count
};

enum class MelodyStyle : int
{
    March = 0,
    Arch,
    Call,
    Turn,
    Hold,
    Drift,
    Count
};

enum class MelodyContour : int
{
    Even = 0,
    Rise,
    Fall,
    Leap,
    Count
};

enum class MelodySpeed : int
{
    Slow = 0,
    Medium,
    Fast,
    Count
};

enum class MelodyPhrase : int
{
    OneBar = 0,
    TwoBars,
    FourBars,
    EightBars,
    Count
};

enum class ChordVoicing : int
{
    Close = 0,
    Open,
    Drop,
    Count
};

enum class ChordColor : int
{
    Triad = 0,
    Seventh,
    Count
};

enum class ChordRhythm : int
{
    Block = 0,
    Pulse,
    Strum,
    Count
};

enum class BassMotion : int
{
    Pulse = 0,
    Half,
    Shuffle,
    Sparse,
    Count
};

enum class DrumStyle : int
{
    Straight = 0,
    HalfTime,
    Shuffle,
    Sparse,
    Count
};

enum class StrumDirection : int
{
    Up = 0,
    Down,
    Alternate,
    Count
};

constexpr int kNumKnobs = 6;
extern const std::array<KnobDef, kNumKnobs> kKnobDefs;

inline const char* quantizeGridLabel(QuantizeGrid g)
{
    switch (g)
    {
        case QuantizeGrid::Quarter:       return "1/4";
        case QuantizeGrid::QuarterT:      return "1/4t";
        case QuantizeGrid::Eighth:        return "1/8";
        case QuantizeGrid::EighthT:       return "1/8t";
        case QuantizeGrid::Sixteenth:     return "1/16";
        case QuantizeGrid::SixteenthT:    return "1/16t";
        case QuantizeGrid::ThirtySecond:  return "1/32";
        case QuantizeGrid::ThirtySecondT: return "1/32t";
        case QuantizeGrid::Count:         break;
    }
    return "1/8";
}

inline const char* articulationLabel(Articulation a)
{
    switch (a)
    {
        case Articulation::Off:        return "Off";
        case Articulation::Legato:     return "Legato";
        case Articulation::Staccato:   return "Staccato";
        case Articulation::Tenuto:     return "Tenuto";
        case Articulation::Pianissimo: return "Pianissimo";
        case Articulation::Sforzando:  return "Sforzando";
        case Articulation::Slur:       return "Slur";
        case Articulation::Count:      break;
    }
    return "Off";
}

inline const char* sustainPedalLabel(SustainPedalMode m)
{
    switch (m)
    {
        case SustainPedalMode::Off:         return "Off";
        case SustainPedalMode::StartOfClip: return "Start of Clip";
        case SustainPedalMode::Auto:        return "Auto";
        case SustainPedalMode::Count:       break;
    }
    return "Off";
}

inline const char* delayTimeLabel(DelayTime t)
{
    switch (t)
    {
        case DelayTime::Quarter:       return "1/4";
        case DelayTime::DottedEighth:  return "1/8.";
        case DelayTime::Eighth:        return "1/8";
        case DelayTime::EighthT:       return "1/8t";
        case DelayTime::Sixteenth:     return "1/16";
        case DelayTime::SixteenthT:    return "1/16t";
        case DelayTime::ThirtySecond:  return "1/32";
        case DelayTime::Count:         break;
    }
    return "1/8";
}

inline const char* arpDirectionLabel(ArpDirection d)
{
    switch (d)
    {
        case ArpDirection::Up:     return "Up";
        case ArpDirection::Down:   return "Down";
        case ArpDirection::UpDown: return "Up/Down";
        case ArpDirection::Random: return "Random";
        case ArpDirection::Count:  break;
    }
    return "Up";
}

inline const char* arpShapeLabel(ArpShape s)
{
    switch (s)
    {
        case ArpShape::Walk:  return "Walk";
        case ArpShape::Skip:  return "Skip";
        case ArpShape::Pedal: return "Pedal";
        case ArpShape::Arch:  return "Arch";
        case ArpShape::Climb: return "Climb";
        case ArpShape::Count: break;
    }
    return "Walk";
}

inline const char* melodyStyleLabel(MelodyStyle s)
{
    switch (s)
    {
        case MelodyStyle::March: return "March";
        case MelodyStyle::Arch:  return "Arch";
        case MelodyStyle::Call:  return "Call";
        case MelodyStyle::Turn:  return "Turn";
        case MelodyStyle::Hold:  return "Hold";
        case MelodyStyle::Drift: return "Drift";
        case MelodyStyle::Count: break;
    }
    return "March";
}

inline const char* melodyContourLabel(MelodyContour c)
{
    switch (c)
    {
        case MelodyContour::Even: return "Even";
        case MelodyContour::Rise: return "Rise";
        case MelodyContour::Fall: return "Fall";
        case MelodyContour::Leap: return "Leap";
        case MelodyContour::Count: break;
    }
    return "Even";
}

inline const char* melodySpeedLabel(MelodySpeed s)
{
    switch (s)
    {
        case MelodySpeed::Slow:   return "Slow";
        case MelodySpeed::Medium: return "Medium";
        case MelodySpeed::Fast:   return "Fast";
        case MelodySpeed::Count:  break;
    }
    return "Medium";
}

inline const char* melodyPhraseLabel(MelodyPhrase p)
{
    switch (p)
    {
        case MelodyPhrase::OneBar:    return "1 Bar";
        case MelodyPhrase::TwoBars:   return "2 Bars";
        case MelodyPhrase::FourBars:  return "4 Bars";
        case MelodyPhrase::EightBars: return "8 Bars";
        case MelodyPhrase::Count:     break;
    }
    return "4 Bars";
}

inline const char* chordVoicingLabel(ChordVoicing v)
{
    switch (v)
    {
        case ChordVoicing::Close: return "Close";
        case ChordVoicing::Open:  return "Open";
        case ChordVoicing::Drop:  return "Drop";
        case ChordVoicing::Count: break;
    }
    return "Close";
}

inline const char* chordColorLabel(ChordColor c)
{
    switch (c)
    {
        case ChordColor::Triad:   return "Triad";
        case ChordColor::Seventh: return "7th";
        case ChordColor::Count:   break;
    }
    return "Triad";
}

inline const char* chordRhythmLabel(ChordRhythm r)
{
    switch (r)
    {
        case ChordRhythm::Block: return "Block";
        case ChordRhythm::Pulse: return "Pulse";
        case ChordRhythm::Strum: return "Strum";
        case ChordRhythm::Count: break;
    }
    return "Block";
}

inline const char* bassMotionLabel(BassMotion m)
{
    switch (m)
    {
        case BassMotion::Pulse:   return "Pulse";
        case BassMotion::Half:    return "Half";
        case BassMotion::Shuffle: return "Shuffle";
        case BassMotion::Sparse:  return "Sparse";
        case BassMotion::Count:   break;
    }
    return "Pulse";
}

inline const char* drumStyleLabel(DrumStyle s)
{
    switch (s)
    {
        case DrumStyle::Straight: return "Straight";
        case DrumStyle::HalfTime: return "Half Time";
        case DrumStyle::Shuffle:  return "Shuffle";
        case DrumStyle::Sparse:   return "Sparse";
        case DrumStyle::Count:    break;
    }
    return "Straight";
}

inline const char* strumDirectionLabel(StrumDirection d)
{
    switch (d)
    {
        case StrumDirection::Up:        return "Up";
        case StrumDirection::Down:      return "Down";
        case StrumDirection::Alternate: return "Alternate";
        case StrumDirection::Count:     break;
    }
    return "Up";
}

struct GrooveParams
{
    int swing     = 0;      // 0..100     offbeat delay amount
    int pocket    = 0;      // -100..100  - push / + lay back (metric-weighted)
    int humanize  = 0;      // 0..100     timing jitter
    int dynamics  = 0;      // -100..100  metric accent (- inverts)
    int length    = 100;    // 25..200    duration % (100 = original)
    int intensity = 100;    // 0..200     velocity % (100 = original)

    SwingBase swingBase = SwingBase::Eighth;
    /** Swing grid: 0=1/16 ... 5=2 (see kSwingGridLabels). */
    int swingGridIndex = 1;

    int quantizeGridIndex = (int) QuantizeGrid::Eighth;
    int quantizeStrength = 0;       // 0..100
    int articulationIndex = (int) Articulation::Off;
    int articulationStrength = 0;   // 0..100 (ignored when Articulation::Off)
    int sustainPedalMode = (int) SustainPedalMode::Off;

    /** -1 = follow clip baseline (no morph). Else 0..100 target complexity. */
    int complexityTarget = -1;

    int delayTimeIndex = (int) DelayTime::Eighth;
    int delayAmount = 0;            // 0..100
    int delayFeedback = 40;         // 0..100

    bool arpEnabled = false;
    bool arpInput = false;
    int arpDirectionIndex = (int) ArpDirection::Up;
    int arpShapeIndex = (int) ArpShape::Walk;
    int arpRateIndex = (int) DelayTime::Sixteenth;
    int arpGate = 70;               // 10..100 %
    int arpOctaves = 1;             // 1..3
    int arpVariation = 1;           // 1..16 seed
    int arpNonce = 0;

    bool melodyEnabled = false;
    bool melodyInput = false;
    int melodyStyleIndex = (int) MelodyStyle::March;
    int melodyContourIndex = (int) MelodyContour::Even;
    int melodySpeedIndex = (int) MelodySpeed::Medium;
    int melodyPhraseIndex = (int) MelodyPhrase::FourBars;
    int melodyVariation = 1;        // 1..16 seed
    /** Bumped by Generate so the live engine restarts the phrase. */
    int melodyNonce = 0;

    bool chordsEnabled = false;
    bool chordsInput = false;
    int chordVoicingIndex = (int) ChordVoicing::Close;
    int chordColorIndex = (int) ChordColor::Triad;
    int chordRhythmIndex = (int) ChordRhythm::Block;
    int chordVariation = 1;
    int chordNonce = 0;

    bool bassEnabled = false;
    bool bassInput = false;
    int bassMotionIndex = (int) BassMotion::Pulse;
    int bassVariation = 1;
    int bassNonce = 0;

    bool drumsEnabled = false;
    int drumStyleIndex = (int) DrumStyle::Straight;
    int drumDensity = 50;           // 0..100
    int drumVariation = 1;          // 1..16 seed
    int drumNonce = 0;

    int strumDirectionIndex = (int) StrumDirection::Up;
    int strumSpeed = 40;            // 0..100 spread
    int strumAmount = 0;            // 0..100 (0 = off)

    /** Compress clip velocities into this MIDI window (1..127). */
    int velocityRangeLo = 1;
    int velocityRangeHi = 127;

    /** 0 = original. 1..16 = alternate slight variations of the clip. */
    int variationIndex = 0;

    int  get(int knobIndex) const;
    void set(int knobIndex, int value);
    bool isDefault() const;
    int  activeCount() const;   // knobs/toggles off default (badge count)

    /** Write Length / Intensity / Dynamics from articulation type + strength. */
    void applyArticulationToKnobs();

    QuantizeGrid quantizeGrid() const
    {
        return (QuantizeGrid) juce::jlimit(0, (int) QuantizeGrid::Count - 1, quantizeGridIndex);
    }
    Articulation articulation() const
    {
        return (Articulation) juce::jlimit(0, (int) Articulation::Count - 1, articulationIndex);
    }
    SustainPedalMode sustainMode() const
    {
        return (SustainPedalMode) juce::jlimit(0, (int) SustainPedalMode::Count - 1, sustainPedalMode);
    }
    DelayTime delayTime() const
    {
        return (DelayTime) juce::jlimit(0, (int) DelayTime::Count - 1, delayTimeIndex);
    }
    ArpDirection arpDirection() const
    {
        return (ArpDirection) juce::jlimit(0, (int) ArpDirection::Count - 1, arpDirectionIndex);
    }
    ArpShape arpShape() const
    {
        return (ArpShape) juce::jlimit(0, (int) ArpShape::Count - 1, arpShapeIndex);
    }
    DelayTime arpRate() const
    {
        return (DelayTime) juce::jlimit(0, (int) DelayTime::Count - 1, arpRateIndex);
    }
    MelodyStyle melodyStyle() const
    {
        return (MelodyStyle) juce::jlimit(0, (int) MelodyStyle::Count - 1, melodyStyleIndex);
    }
    MelodyContour melodyContour() const
    {
        return (MelodyContour) juce::jlimit(0, (int) MelodyContour::Count - 1, melodyContourIndex);
    }
    MelodySpeed melodySpeed() const
    {
        return (MelodySpeed) juce::jlimit(0, (int) MelodySpeed::Count - 1, melodySpeedIndex);
    }
    MelodyPhrase melodyPhrase() const
    {
        return (MelodyPhrase) juce::jlimit(0, (int) MelodyPhrase::Count - 1, melodyPhraseIndex);
    }
    ChordVoicing chordVoicing() const
    {
        return (ChordVoicing) juce::jlimit(0, (int) ChordVoicing::Count - 1, chordVoicingIndex);
    }
    ChordColor chordColor() const
    {
        return (ChordColor) juce::jlimit(0, (int) ChordColor::Count - 1, chordColorIndex);
    }
    ChordRhythm chordRhythm() const
    {
        return (ChordRhythm) juce::jlimit(0, (int) ChordRhythm::Count - 1, chordRhythmIndex);
    }
    BassMotion bassMotion() const
    {
        return (BassMotion) juce::jlimit(0, (int) BassMotion::Count - 1, bassMotionIndex);
    }
    DrumStyle drumStyle() const
    {
        return (DrumStyle) juce::jlimit(0, (int) DrumStyle::Count - 1, drumStyleIndex);
    }
    StrumDirection strumDirection() const
    {
        return (StrumDirection) juce::jlimit(0, (int) StrumDirection::Count - 1, strumDirectionIndex);
    }

    /** Effective complexity for the UI slider (falls back to clip baseline). */
    int resolvedComplexityTarget(int clipComplexity) const
    {
        if (complexityTarget >= 0)
            return juce::jlimit(0, 100, complexityTarget);
        return juce::jlimit(0, 100, clipComplexity);
    }
};

/** Per-section browse locks in the Toolkit pane. */
namespace toolkitLock
{
    constexpr uint32_t Playback     = 1u << 0; // tempo mult + extend
    constexpr uint32_t Timing       = 1u << 1;
    constexpr uint32_t Performance  = 1u << 2;
    constexpr uint32_t Pitch        = 1u << 3;
    constexpr uint32_t Effects      = 1u << 4;
    constexpr uint32_t All = Playback | Timing | Performance | Pitch | Effects;
    constexpr uint32_t AnyGroove = Timing | Performance | Effects;
}

/** Overlay Timing / Performance / Effects fields from `locked` when those bits are set. */
void applyGrooveSectionLock(const GrooveParams& locked, GrooveParams& target, uint32_t locks);
/** Copy those section fields from `src` into `locked`.
    clipComplexity >= 0 freezes a follow-clip complexity (-1) to that absolute value
    so a locked Effects section does not track each file. */
void captureGrooveSectionLock(const GrooveParams& src, GrooveParams& locked, uint32_t locks,
                              int clipComplexity = -1);

/** Timing + generative transforms. sourceComplexity is the clip's baseline (1..100).
    kind / root / mode steer complexity morph and variation so edits stay musical. */
std::vector<RollNote> applyGroove(const std::vector<RollNote>& notes, const GrooveParams& k,
                                  int sourceComplexity = -1,
                                  ClipKind kind = ClipKind::Lead,
                                  int rootPc = -1,
                                  Mode mode = Mode::Ionian);

/** Build CC64 sustain points (beat domain) for export/preview. Empty when Off. */
std::vector<AutomationPoint> buildSustainPedalAutomation(const std::vector<RollNote>& notes,
                                                         double lengthBeats,
                                                         const GrooveParams& k);

/** Deterministic per-note base velocity, 0.45–1.0 (hashed from note id). */
double baseVel(int id);

/** Effective velocity 0.04–1.0. Intensity scales the whole performance;
    Dynamics widens (or inverts) contrast by metric position. */
double noteVelocity(const RollNote& n, const GrooveParams& k);

/** noteVelocity with an explicit base — used for real MIDI files so the
    file's own velocities survive as the base performance. */
double noteVelocityWithBase(double base, const RollNote& n, const GrooveParams& k);

/** Formatted knob readout (e.g. "50%", "+12", "100%"). */
juce::String grooveValueText(const KnobDef& def, int value);

} // namespace pflow
