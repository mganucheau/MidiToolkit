#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "MidiFileData.h"

namespace pflow {

// ── Step-domain clip model (16th-note grid, 4/4) ─────────────────────────────
// Port of Prototype/mb/data.jsx. All edits are non-destructive: the source
// clip is never mutated; every change lives in a ClipEdit and is applied as a
// display/playback transform by resolveClip().

constexpr int kStepsPerBar = 16;

struct RollNote
{
    int    id    = 0;
    int    pitch = 60;      // MIDI note number (60 = C4)
    double start = 0.0;     // 16th steps from clip start
    double len   = 1.0;     // 16th steps
    int    fileVelocity = 100;  // velocity as loaded from disk (0–127)
    int    channel = 1;
    bool   moved = false;   // set by resolveClip when a manual move applies
};

enum class ClipKind { Bass, Piano, Lead, Drums };
constexpr int kNumClipKinds = 4;

const char* clipKindName(ClipKind k);

/** Best-effort kind guess from a file name / path (keywords like bass, kit, keys). */
ClipKind hintKindFromFileName(const juce::String& nameOrPath);

/** Content tags derived from note analysis. Bitmask on StepClip / BrowserSearch. */
namespace clipTag
{
    constexpr uint16_t Swing         = 1u << 0;
    constexpr uint16_t SixteenthHats = 1u << 1;
    constexpr uint16_t Backbeat      = 1u << 2;
    constexpr uint16_t SeventhChords = 1u << 3;
    constexpr uint16_t Sparse        = 1u << 4;
    constexpr int kCount = 5;
}

inline const char* clipTagName(int bit)
{
    switch (bit)
    {
        case 0: return "Swing";
        case 1: return "16th Hats";
        case 2: return "Backbeat";
        case 3: return "7ths";
        case 4: return "Sparse";
        default: return "";
    }
}

struct StepClip
{
    juce::String name;
    juce::String filePath;
    ClipKind kind = ClipKind::Lead;
    /** 0..100 confidence in `kind`. Low values mean the nearest label is a guess. */
    int    kindConfidence = 50;
    /** True when a non-trivial share of notes sit on the GM drum map. */
    bool   hasDrumContent = false;
    /** Bitmask of clipTag::* derived from content. */
    uint16_t tagMask = 0;
    int    root = -1;       // pitch-class 0..11, -1 = unknown
    double bpm  = 120.0;
    int    bars = 1;
    int    timeSigNum = 4;
    int    timeSigDen = 4;
    int    noteCount = 0;
    int    difNotes = 0;    // distinct pitches
    int    complexity = 1;  // 1..100 density x variety score
    /** True after a full parse. Browse scans leave this false and keep `notes` empty. */
    bool   notesResident = false;
    std::vector<RollNote> notes;
};

/** True when pitch is in the General MIDI percussion key map (35–81). */
bool isGmDrumPitch(int midi);

// ── Music theory ─────────────────────────────────────────────────────────────

extern const std::array<const char*, 12> kNoteNames;   // 'C'..'B' (sharps)

enum class Mode { Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian };
constexpr int kNumModes = 7;

const char* modeName(Mode m);
const std::array<int, 7>& modeIntervals(Mode m);

/** Soften a detected mode for audition defaults without breaking relatives.
    Lydian/Mixolydian → Major at the same root (so Fit snaps #4 / b7).
    Dorian/Phrygian/Aeolian/Locrian stay as detected — collapsing Dorian to
    Minor at the same root is wrong (D Dorian ≠ D minor). */
Mode auditionMode(Mode m);

juce::String pitchName(int midi);           // MIDI 60 = "C4"
bool isBlackKeyPitch(int midi);

/** Scientific octave for a MIDI pitch (60 → 4). */
int scientificOctave(int midi);
/** Lowest-note octave of a clip, clamped to 0..6 (empty → 4). */
int clipReferenceOctave(const StepClip& clip);

/** Candidate keys/modes that contain every pitch-class in the clip. */
struct ScaleAnalysis
{
    uint16_t keyMask = 0;   // bit i = pitch-class i is a viable root
    uint16_t modeMask = 0;  // bit i = Mode i is viable with some root
    int primaryRoot = -1;   // best root (prefers estimated clip.root)
    Mode primaryMode = Mode::Ionian;
    /** 0 = Off (notes within one octave). Else 1..3 spanning octaves. */
    int octaveRange = 0;
};

/** Detect viable keys/modes and octave span from clip note content. */
ScaleAnalysis analyseClipScale(const StepClip& clip);

/** True when midi pitch-class belongs to scale(rootPc, mode). */
bool pitchInScale(int midi, int rootPc, Mode mode);
int  noteNameIndex(const juce::String& name);   // "C"..."B" -> 0..11, else -1

/** Snap a pitch to the nearest tone of scale(root, mode), preserving register.
    Searches ±6 semitones; on distance ties the lower candidate wins. */
int fitToScale(int midi, int rootPc, Mode mode);

/** Re-snap every note into scale(root, mode). No-op when rootPc < 0. */
void refitNotesToScale(std::vector<RollNote>& notes, int rootPc, Mode mode);

/** Note Filter: Mute drops filtered pitch-classes; Fold remaps to nearest kept class. */
enum class NoteFilterType { Mute = 0, Fold = 1 };

/** Snap midi to the nearest pitch-class allowed by `enabledMask` (bit = PC enabled).
    Prefers tones in scale(root, mode) when rootPc >= 0. */
int foldToEnabledPitchClasses(int midi, uint16_t enabledMask, int rootPc, Mode mode);

/** Scale degree label for a pitch class in key/mode, e.g. "3rd". Empty if out of scale. */
juce::String scaleDegreeLabel(int midi, int rootPc, Mode mode);

/** Note + degree annotation for the inspector, e.g. "B4 (3rd)". */
juce::String pitchScaleAnnotation(int midi, int rootPc, Mode mode);

/** What happens to notes that fall outside an Octave Range window. */
enum class OutsideRangeMode { Clip = 0, Fold = 1 };

/** MIDI note of C in scientific octave 0..6 (octave 4 → C4 = 60). */
inline int octaveWindowBase(int octaveIndex)
{
    return juce::jlimit(0, 127, (juce::jlimit(0, 6, octaveIndex) + 1) * 12);
}

/** Map one pitch into the N-octave window that starts at C of octaveIndex.
    Fold wraps by the window size (Ableton OctaveFold). Clip clamps to the edges. */
inline int mapPitchToOctaveWindow(int pitch, int octaveIndex, int octaveRange, OutsideRangeMode mode)
{
    const int span = juce::jlimit(1, 3, octaveRange) * 12;
    const int lo = octaveWindowBase(octaveIndex);
    const int hi = juce::jmin(127, lo + span - 1);
    if (mode == OutsideRangeMode::Clip)
        return juce::jlimit(lo, hi, pitch);
    const int usable = juce::jmax(1, hi - lo + 1);
    int rel = pitch - lo;
    int wrapped = rel % usable;
    if (wrapped < 0)
        wrapped += usable;
    return lo + wrapped;
}

// ── Edit record (one per clip; the clip itself is never touched) ─────────────

struct NoteMove { int dPitch = 0; int dStep = 0; };

struct ClipEdit
{
    /** Absolute target octave 0..6. -1 = keep the clip's native octave. */
    int  octave    = -1;
    int  pitchShift = 0;   // whole-file semitone transpose, -12..12
    /** Keyboard latches a root for browsing / generator parts with Input off. */
    bool pitchInput = false;
    /** 0 = Off. 1..3 = keep notes inside that many octaves from the target C. */
    int  octaveRange = 0;
    /** Clip clamps outside notes. Fold wraps them into the window. */
    OutsideRangeMode outsideRange = OutsideRangeMode::Fold;
    /** Inclusive MIDI clamp/fold window. Defaults = full range (inactive). */
    int  pitchMin = 0;
    int  pitchMax = 127;
    /** Playback length multiplier: 1 (Off), 2, 4, or 8. Tiles the clip. */
    int  extendMult = 1;
    bool fitScale  = false;
    bool mapToRoot = false;
    int  root      = -1;            // target root pitch-class, -1 = unset
    Mode mode      = Mode::Ionian;  // displayed as "Major"
    /** Bit i set = pitch-class i is kept (default: all 12 on). */
    uint16_t noteFilterMask = 0x0FFF;
    NoteFilterType noteFilterType = NoteFilterType::Mute;
    std::map<int, NoteMove> moves;  // per-note manual moves keyed by note id
    /** Empty bars removed by Trim (any position, including middle gaps).
        Sorted ascending. Legacy trimLead/trimTail are migrated on load. */
    std::vector<int> removedBars;
    /** Session-load only: old trimLead/trimTail until resolve has clip.bars. */
    int legacyTrimLead = 0;
    int legacyTrimTail = 0;
    std::map<int, int> velocities;  // per-note velocity overrides (1-127)
    std::set<int> deleted;          // per-note non-destructive deletions

    bool hasTrim() const
    {
        return !removedBars.empty() || legacyTrimLead > 0 || legacyTrimTail > 0;
    }

    bool hasPitchRange() const
    {
        return pitchMin > 0 || pitchMax < 127;
    }

    bool hasNoteFilter() const
    {
        return (noteFilterMask & 0x0FFF) != 0x0FFF;
    }

    bool isNoteFilterEnabled(int pitchClass) const
    {
        const int pc = ((pitchClass % 12) + 12) % 12;
        return (noteFilterMask & (uint16_t) (1u << pc)) != 0;
    }

    void setNoteFilterEnabled(int pitchClass, bool on)
    {
        const int pc = ((pitchClass % 12) + 12) % 12;
        const uint16_t bit = (uint16_t) (1u << pc);
        if (on)
            noteFilterMask = (uint16_t) ((noteFilterMask | bit) & 0x0FFF);
        else
        {
            const uint16_t next = (uint16_t) (noteFilterMask & (uint16_t) ~bit & 0x0FFF);
            // Keep at least one pitch-class enabled.
            if (next != 0)
                noteFilterMask = next;
        }
    }

    void clearTrim()
    {
        removedBars.clear();
        legacyTrimLead = 0;
        legacyTrimTail = 0;
    }
};

bool editIsClean(const ClipEdit& e);

/** Copy pitch-shaping fields (octave, fit-to-scale, map-to-root, root, mode,
    note filter) from `locked` into `target`. Per-note moves, trim, and extend
    stay on the target — those belong to Playback lock. */
void applyPitchLock(const ClipEdit& locked, ClipEdit& target);

/** Stamp locked Playback onto `target`: extend, and either re-trim this clip's
    empty bars (`autoTrim`) or clear trim so a previous visit cannot turn it back on. */
void applyPlaybackLock(const ClipEdit& locked, ClipEdit& target, bool autoTrim,
                       const StepClip& clip);

// ── Transform pipeline ───────────────────────────────────────────────────────

struct ResolvedClip
{
    std::vector<RollNote> notes;
    int bars = 1;
};

/** Effective notes + bar count for display and playback. Order:
    per-note move + octave → map-to-root → fit-to-scale → octave-range
    → pitch-range fold → step move → trim. */
ResolvedClip resolveClip(const StepClip& clip, const ClipEdit& edit);

struct EdgeBars { int lead = 0; int tail = 0; };

/** Fully-empty leading/trailing bars of a note set. */
EdgeBars emptyEdgeBars(const std::vector<RollNote>& notes, int bars);

/** Indices of every fully-empty bar (including middle gaps). Sorted ascending. */
std::vector<int> emptyBars(const std::vector<RollNote>& notes, int bars);

/** Remap a step position after removing the given bar indices. */
double remapStepAfterRemovingBars(double step, const std::vector<int>& removedBars);

struct EditBadge
{
    juce::String key;    // "oct" | "scale" | "map" | "moves" | "trim"
    juce::String label;
};

/** Toolbar chips for the non-default transforms in an edit. */
std::vector<EditBadge> editBadges(const StepClip& clip, const ClipEdit& edit);

// ── Bridging from the beats-based file model ─────────────────────────────────

/** Beats→steps view of a parsed MIDI file (1 beat = 4 steps). Assigns stable
    sequential note ids in file order; estimates root and kind from content. */
StepClip makeStepClip(const MidiClip& clip);

/** Browse metadata only: bpm, bars, kind, complexity. Does not retain note events. */
StepClip scanMidiBrowseMeta(const juce::File& file);

} // namespace pflow
