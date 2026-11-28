#pragma once

#include "MidiFileData.h"
#include <vector>

namespace pflow {

struct AlsClip
{
    juce::String track;
    juce::String name;
    int timeSigNum = 4;
    int timeSigDen = 4;
    double lengthBeats = 0.0;
    std::vector<NoteEvent> notes;
};

struct AlsSession
{
    juce::File setFile;
    juce::String setName;
    double bpm = 120.0;
    std::vector<AlsClip> clips;
};

/** Last `Loading document "...als"` path in a Live Log.txt. Empty when none. */
juce::File liveSetPathFromLog(const juce::String& logText);

/** Set recorded by the newest Ableton Log.txt, including a crash-recovery copy. */
juce::File findCurrentLiveSet();

/** Gunzip and read MIDI clips. Empty clips when the file cannot be read. */
AlsSession parseAlsSession(const juce::File& alsFile);

/** Replace `~/Library/Caches/MidiToolkit/Session/<set name>/` with one .mid per clip. */
juce::File writeSessionMidiFiles(const AlsSession& session);

} // namespace pflow
