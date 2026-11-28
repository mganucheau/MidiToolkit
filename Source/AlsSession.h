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

/** Last real set in a Live Log.txt. Track and factory templates are ignored. */
juce::File liveSetPathFromLog(const juce::String& logText);

/** Absolute .als paths embedded in CrashRecoveryInfo.cfg, in file order. */
juce::StringArray alsPathsInRecoveryInfo(const juce::MemoryBlock& data);

/** Newest existing set. When two files share a time, the one outside BaseFiles wins. */
juce::File chooseLiveSetFile(const juce::Array<juce::File>& candidates);

/** Set recorded by the newest Ableton Log.txt, including a crash-recovery copy. */
juce::File findCurrentLiveSet();

/** Gunzip and read MIDI clips. Empty clips when the file cannot be read. */
AlsSession parseAlsSession(const juce::File& alsFile);

/** Replace `~/Library/Caches/MidiToolkit/Session/<set name>/` with one .mid per clip. */
juce::File writeSessionMidiFiles(const AlsSession& session);

} // namespace pflow
