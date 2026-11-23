#pragma once
#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

namespace pflow {

// ── Single note event ────────────────────────────────────────────────────────
struct NoteEvent
{
    int    noteNumber   = 60;
    int    velocity     = 100;
    double startBeat    = 0.0;
    double lengthBeats  = 1.0;
    int    channel      = 1;
};

// ── Automation point (per-clip CC data) ─────────────────────────────────────
struct AutomationPoint
{
    double beat  = 0.0;
    int    value = 0;   // 0–127
    bool operator==(const AutomationPoint& o) const
    {
        return std::abs(beat - o.beat) < 1e-12 && value == o.value;
    }
};

// ── A loaded MIDI clip ───────────────────────────────────────────────────────
struct MidiClip
{
    juce::String       name;
    juce::String       filePath;
    double             bpm = 120.0;    // tempo from the file, if present
    double             lengthBeats = 4.0;
    int                timeSigNum = 4;
    int                timeSigDen = 4;
    std::vector<NoteEvent> notes;
    /** Copy of notes as loaded from file (for reverting transpose when Scale is off). Empty if not from a file load. */
    std::vector<NoteEvent> notesAtFileLoad;
    juce::Colour       colour { 0xff3a7bd5 };

    // Scale / transpose helpers
    int  rootNoteOffset = 0;  // semitones from original

    // Clip start offset: draggable start point within the clip (beats)
    double clipStartOffset = 0.0;

    // Automation: CC number -> time-ordered points (beat, 0–127)
    std::map<int, std::vector<AutomationPoint>> automation;

    juce::MidiMessageSequence toMidiSequence(double bpm) const;

    // Get notes filtered by note number
    std::vector<NoteEvent> getNotesForPitch(int noteNumber) const;
    std::vector<int> getDistinctPitches() const;
};

// ── Parse a standard MIDI file ───────────────────────────────────────────────
MidiClip parseMidiFile(const juce::File& file);

// Remove leading/trailing/middle measures (beatsPerBar-wide) that contain no note audio
void trimEmptyMeasuresInClip(MidiClip& clip, double beatsPerBar = 4.0);
/** Drop measures after the last note. Empty bars in the middle stay. */
void trimTrailingEmptyMeasures(MidiClip& clip);

// ── Write a MidiClip to a standard MIDI file ────────────────────────────────
// maxLengthBeats: if > 0, trim clip to this length (notes beyond are excluded/trimmed)
bool writeMidiFile(const MidiClip& clip, const juce::File& file, double bpm = 120.0, double maxLengthBeats = 0.0);

// ── Scale definitions ────────────────────────────────────────────────────────
enum class ScaleType
{
    Chromatic, Major, NaturalMinor, HarmonicMinor, MelodicMinor,
    Dorian, Phrygian, Lydian, Mixolydian, Locrian,
    PentatonicMajor, PentatonicMinor, Blues, WholeTone,
    Count
};

juce::String scaleTypeName(ScaleType s);
std::vector<int> scaleIntervals(ScaleType s);

// Map an incoming note to the nearest note within a given scale + root
int quantiseToScale(int inNote, int scaleRoot, ScaleType scale);

// Dominant pitch class (0–11) from note content, weighted by note length
int estimatePitchClassFromNotes(const std::vector<NoteEvent>& notes);

// ── Lane/region model ────────────────────────────────────────────────────────
struct CompRegion
{
    double startBeat  = 0.0;
    double endBeat    = 4.0;
    int    clipIndex  = -1;       // index into the lane's clip list
    int    noteFilter = -1;       // -1 = all notes, >=0 = specific pitch only
    bool   muted      = false;

    void ensureSelectionInBounds()
    {
        // Clamp region span only.
        startBeat = std::max(0.0, startBeat);
        endBeat = std::max(startBeat + 0.01, endBeat);
    }
};

struct CompLane
{
    juce::String            name;
    juce::Colour            colour { 0xff3a7bd5 };
    std::vector<MidiClip>   clips;
    std::vector<CompRegion> regions;
    bool                    expanded = false;
    bool                    muted    = false;
    bool                    solo     = false;

    void addClipAtPosition(const MidiClip& clip, double beatPos);
    void removeRegion(int index);
};

// ── MIDI split routing ───────────────────────────────────────────────────────
struct MidiSplitRule
{
    int noteMin     = 0;
    int noteMax     = 127;
    int outputIndex = 0;   // virtual output bus 0..15
};

} // namespace pflow
