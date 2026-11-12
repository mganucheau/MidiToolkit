#pragma once
#include "GrooveEngine.h"
#include "EditModel.h"
#include <cstdint>
#include <vector>

namespace pflow {
namespace gen {

/** Note on (velocity > 0) or note off (velocity == 0) at host PPQ. */
struct Event
{
    double ppq = 0.0;
    int note = 60;
    int velocity = 0;
};

struct Harmony
{
    int root = 0;     // pitch class
    int third = 4;    // semitone offset from root
    int fifth = 7;
    int seventh = -1; // -1 = none, else 10/11
};

/** Chord from held MIDI pitches, or a short key progression when empty. */
Harmony harmonyFromHeld(const int* chordNotes, int chordCount, int keyRoot, Mode mode, int bar);
Harmony progressionHarmony(int keyRoot, Mode mode, int bar);

/** Scale degrees for the edit mode (7 tones). */
const int* scaleDegrees(Mode mode, int& count);

int pitchInRange(int pitchClass, int low, int high, int prefer);

void renderArp(const GrooveParams& g, double startPpq, double endPpq,
               const int* chordNotes, int chordCount, int keyRoot, Mode mode,
               std::vector<Event>& out);

void renderMelody(const GrooveParams& g, double startPpq, double endPpq,
                  const int* chordNotes, int chordCount, int keyRoot, Mode mode,
                  std::vector<Event>& out);

void renderChords(const GrooveParams& g, double startPpq, double endPpq,
                  const int* chordNotes, int chordCount, int keyRoot, Mode mode,
                  std::vector<Event>& out);

void renderBass(const GrooveParams& g, double startPpq, double endPpq,
                const int* chordNotes, int chordCount, int keyRoot, Mode mode,
                std::vector<Event>& out);

void renderDrums(const GrooveParams& g, double startPpq, double endPpq,
                 std::vector<Event>& out);

enum class Part : int
{
    Arp = 0,
    Melody,
    Chords,
    Bass,
    Drums,
    Count
};

/** Pair note-ons with offs into RollNotes. ppq → 16th steps (×4). */
std::vector<RollNote> eventsToRollNotes(const std::vector<Event>& events, int channel = 1);

/** Render one generator part into a StepClip for the piano roll. */
StepClip renderPartClip(Part part, const GrooveParams& g, int keyRoot, Mode mode,
                        const int* chordNotes = nullptr, int chordCount = 0);

} // namespace gen
} // namespace pflow
